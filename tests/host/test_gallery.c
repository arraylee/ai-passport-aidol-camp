#include "hr_gallery.h"
#include "esp_partition.h"
#include "esp_jpeg_dec.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* These fakes replace target-only flash and the RISC-V decoder library. The
 * storage protocol, validation, CRCs, and bank selection are production code. */
static uint8_t flash[0xc0000], baseline[0xc0000];
static esp_partition_t partition = {0x400000, sizeof flash};
static bool session = true, present = true, fail_map;
static int failure_at = -1, mutations, partial, corrupt_write;
static int decoder_fault, decode_step, revoke_step;
static bool revoke_after_frame;
static size_t pixel_writes;
static unsigned width = 240, height = 320;
static uint8_t color = 0x36;
static const uint8_t jpeg[] = {
    0xff,0xd8, 0xff,0xc0,0,17,8,1,64,0,240,3,
    1,0x22,0,2,0x11,1,3,0x11,1,
    0xff,0xda,0,12,3,1,0,2,0x11,3,0x11,0,63,0,
    0x34,0x56,0xff,0,0x78,0xff,0xd9};

bool hr_profile_session_valid(void) { return session; }
const esp_partition_t *esp_partition_find_first(int type,int sub,const char *name) {
  assert(type == ESP_PARTITION_TYPE_DATA && sub == 0x43);
  assert(strcmp(name,"gallery_store") == 0);
  return present ? &partition : NULL;
}
int esp_partition_mmap(const esp_partition_t *p,size_t off,size_t n,int type,
                      const void **out,esp_partition_mmap_handle_t *handle) {
  assert(p == &partition && off == 0 && n == sizeof flash);
  assert(type == ESP_PARTITION_MMAP_DATA);
  if (fail_map) return -1;
  *out = flash; *handle = 1; return ESP_OK;
}
void esp_partition_munmap(esp_partition_mmap_handle_t handle) { assert(handle == 1); }
int esp_partition_erase_range(const esp_partition_t *p,size_t off,size_t n) {
  assert(p == &partition && off <= sizeof flash && n <= sizeof flash-off);
  assert((off % 4096) == 0 && (n % 4096) == 0);
  if (mutations++ == failure_at) {
    if (partial) memset(flash+off,0xff,n/2);
    return -1;
  }
  memset(flash+off,0xff,n);
  if(off<0x80000) pixel_writes=0;
  return ESP_OK;
}
int esp_partition_write(const esp_partition_t *p,size_t off,const void *src,size_t n) {
  assert(p == &partition && off <= sizeof flash && n <= sizeof flash-off);
  const uint8_t *s = src;
  bool fail = mutations++ == failure_at;
  if (fail && !partial) return -1;
  size_t written = fail ? n/2 : n;
  for (size_t i=0;i<written;i++) { assert((flash[off+i]&s[i]) == s[i]); flash[off+i]=s[i]; }
  if (off < 0x80000 && n > 32) {
    if(corrupt_write) flash[off] ^= 1;
    pixel_writes += written;
    if(revoke_after_frame && pixel_writes == 153600) session=false;
  }
  return fail ? -1 : ESP_OK;
}
jpeg_error_t jpeg_dec_open(jpeg_dec_config_t *cfg,jpeg_dec_handle_t *dec) {
  assert(cfg->block_enable && cfg->output_type == JPEG_PIXEL_FORMAT_RGB565_LE);
  if (decoder_fault == 1) return JPEG_ERR_NO_MEM;
  *dec = (void *)1; decode_step = 0; return JPEG_ERR_OK;
}
jpeg_error_t jpeg_dec_parse_header(jpeg_dec_handle_t dec,jpeg_dec_io_t *io,jpeg_dec_header_info_t *info) {
  assert(dec && io->inbuf >= flash+0x80000 && io->inbuf_len > 0);
  info->width = width; info->height = height;
  return decoder_fault == 2 ? JPEG_ERR_BAD_DATA : JPEG_ERR_OK;
}
jpeg_error_t jpeg_dec_get_outbuf_len(jpeg_dec_handle_t dec,int *n) {
  assert(dec); *n = decoder_fault == 3 ? 8000 : width*16*2;
  return decoder_fault == 4 ? JPEG_ERR_FAIL : JPEG_ERR_OK;
}
jpeg_error_t jpeg_dec_get_process_count(jpeg_dec_handle_t dec,int *n) {
  assert(dec); *n = decoder_fault == 5 ? 10000 : (height+15)/16;
  return decoder_fault == 6 ? JPEG_ERR_FAIL : JPEG_ERR_OK;
}
jpeg_error_t jpeg_dec_process(jpeg_dec_handle_t dec,jpeg_dec_io_t *io) {
  assert(dec && ((uintptr_t)io->outbuf % 16) == 0);
  assert(hr_gallery_busy());
  unsigned rows = height - decode_step*16;
  if (rows>16) rows=16;
  memset(io->outbuf,color,width*rows*2);
  io->out_size = width*rows*2;
  if (++decode_step == revoke_step) session = false;
  if (decoder_fault == 7 && decode_step == 20) return JPEG_ERR_BAD_DATA;
  if (decoder_fault == 8) io->out_size = 0;
  if (decoder_fault == 9) io->out_size = 8000;
  if (decoder_fault == 10 && decode_step == 20) io->out_size -= 2;
  return JPEG_ERR_OK;
}
jpeg_error_t jpeg_dec_close(jpeg_dec_handle_t dec) { assert(dec); return JPEG_ERR_OK; }

static unsigned begin(size_t n,uint32_t now) {
  uint8_t b[] = {0,n,n>>8,n>>16,n>>24};
  return hr_gallery_image(b,sizeof b,now);
}
static unsigned send_bytes(const uint8_t *p,size_t n,uint32_t now) {
  uint8_t b[256]; assert(n < sizeof b); b[0]=1; memcpy(b+1,p,n);
  return hr_gallery_image(b,n+1,now);
}
static unsigned end(uint32_t now) { uint8_t b=2; return hr_gallery_image(&b,1,now); }
static unsigned upload(const uint8_t *p,size_t n) {
  unsigned s=begin(n,100); if(s) return s;
  s=send_bytes(p,n,101); return s ? s : end(102);
}
static void photo(uint8_t want) {
  const lv_image_dsc_t *d=hr_gallery_image_descriptor();
  assert(d && d->header.w==240 && d->header.h==320 && d->header.stride==480);
  assert(d->header.cf==LV_COLOR_FORMAT_RGB565 && d->data_size==153600);
  for(unsigned i=0;i<d->data_size;i++) assert(d->data[i]==want);
}
static void reset_faults(void) {
  session=true; failure_at=-1; mutations=0; partial=0; corrupt_write=0;
  decoder_fault=0; revoke_step=0; revoke_after_frame=false; pixel_writes=0; width=240; height=320;
}
static void blank(void) {
  reset_faults(); present=true; fail_map=false; partition.address=0x400000;
  partition.size=sizeof flash; memset(flash,0xff,sizeof flash);
  assert(hr_gallery_init()); assert(!hr_gallery_image_descriptor());
}
static void test_sequence(void) {
  blank(); assert(end(1)==0x12); assert(send_bytes(jpeg,3,2)==0x12);
  assert(begin(0,3)==0x15); assert(begin(0x40001,4)==0x15);
  assert(begin(sizeof jpeg,10)==0); assert(hr_gallery_busy());
  assert(begin(sizeof jpeg,11)==0x1b); assert(end(12)==0x1a);
  assert(!hr_gallery_busy()); assert(!hr_gallery_image_descriptor());
  assert(begin(sizeof jpeg,20)==0); assert(send_bytes(jpeg,sizeof jpeg,21)==0);
  assert(send_bytes(jpeg,1,22)==0x15); assert(end(23)==0x12);
  assert(upload(jpeg,sizeof jpeg)==1); photo(color); assert(!hr_gallery_busy());
  assert(hr_gallery_init()); photo(color);
}
static void test_interrupt_session_timeout(void) {
  blank(); assert(upload(jpeg,sizeof jpeg)==1); photo(color);
  assert(begin(sizeof jpeg,1000)==0); assert(send_bytes(jpeg,4,1001)==0);
  assert(hr_gallery_init()); photo(color); assert(end(1002)==0x12);
  assert(begin(sizeof jpeg,2000)==0); session=false;
  assert(send_bytes(jpeg,4,2001)==0x16); assert(!hr_gallery_busy());
  session=true; revoke_step=20;
  assert(upload(jpeg,sizeof jpeg)==0x16); photo(color);
  reset_faults(); revoke_after_frame=true;
  assert(upload(jpeg,sizeof jpeg)==0x16); photo(color);
  reset_faults(); assert(hr_gallery_init()); photo(color);
  assert(begin(sizeof jpeg,UINT32_MAX-5000)==0);
  hr_gallery_expire(4999); assert(hr_gallery_busy());
  hr_gallery_expire(5000); assert(!hr_gallery_busy());
  assert(begin(sizeof jpeg,0)==0); assert(end(10001)==0x11);
  assert(!hr_gallery_busy()); photo(color);
}
static void test_decode_failures(void) {
  blank(); assert(upload(jpeg,sizeof jpeg)==1); photo(color);
  for (int f=1;f<=10;f++) {
    decoder_fault=f;
    unsigned s=upload(jpeg,sizeof jpeg); assert(s==0x17 || s==0x1b);
    assert(!hr_gallery_busy()); photo(color); assert(hr_gallery_init()); photo(color);
  }
  decoder_fault=0; width=248; assert(upload(jpeg,sizeof jpeg)==0x17); photo(color);
  width=240;
  uint8_t broken[sizeof jpeg]; memcpy(broken,jpeg,sizeof jpeg);
  broken[3]=0xc2; assert(upload(broken,sizeof broken)==0x17);
  memcpy(broken,jpeg,sizeof jpeg); broken[10]=0xf8;
  assert(upload(broken,sizeof broken)==0x17);
  assert(upload(jpeg,sizeof jpeg-2)==0x17);
  memcpy(broken,jpeg,sizeof jpeg); broken[4]=0xff;
  assert(upload(broken,sizeof broken)==0x17); photo(color);
}
static void test_atomic_flash_failures(void) {
  blank(); color=0x36; assert(upload(jpeg,sizeof jpeg)==1); memcpy(baseline,flash,sizeof flash);
  color=0x79; reset_faults(); assert(upload(jpeg,sizeof jpeg)==1);
  int writes=mutations; assert(writes>20); photo(color);
  for(int p=0;p<2;p++) for(int f=0;f<writes;f++) {
    memcpy(flash,baseline,sizeof flash); reset_faults(); assert(hr_gallery_init());
    failure_at=f; partial=p;
    assert(upload(jpeg,sizeof jpeg)==0x16); photo(0x36);
    reset_faults(); assert(hr_gallery_init()); photo(0x36);
  }
  memcpy(flash,baseline,sizeof flash); reset_faults(); assert(hr_gallery_init());
  corrupt_write=1; assert(upload(jpeg,sizeof jpeg)==0x16); photo(0x36);
  reset_faults(); assert(hr_gallery_init()); photo(0x36);
  assert(upload(jpeg,sizeof jpeg)==1); photo(0x79);
  /* A bad newer pixel CRC must fall back to the complete older photo. */
  const lv_image_dsc_t *d=hr_gallery_image_descriptor();
  flash[d->data-flash+1]^=1;
  assert(hr_gallery_init()); photo(0x36);
}
static void test_capacity_abort_and_bad_frames(void) {
  blank(); color=0x52; assert(upload(jpeg,sizeof jpeg)==1);
  assert(begin(0x40000,10)==0);
  uint8_t bytes[255]={0};
  for (size_t at=0;at<0x40000;) {
    size_t n=0x40000-at; if(n>sizeof bytes) n=sizeof bytes;
    assert(send_bytes(bytes,n,11)==0); at+=n;
  }
  assert(end(12)==0x17); photo(0x52);
  assert(begin(sizeof jpeg,20)==0); hr_gallery_abort();
  assert(!hr_gallery_busy() && end(21)==0x12); photo(0x52);
  assert(hr_gallery_image(NULL,0,22)==0x10);
  assert(begin(sizeof jpeg,30)==0);
  uint8_t bad[]={3,4}; assert(hr_gallery_image(bad,sizeof bad,31)==0x10);
  assert(!hr_gallery_busy()); photo(0x52);
  assert(begin(sizeof jpeg,40)==0);
  uint8_t empty=1; assert(hr_gallery_image(&empty,1,41)==0x15);
  assert(!hr_gallery_busy()); photo(0x52);
  /* Invalid input must be rejected before even opening the target decoder. */
  decoder_fault=1;
  for(size_t n=1;n<sizeof jpeg-1;n++) assert(upload(jpeg,n)==0x17);
  decoder_fault=0;
  assert(hr_gallery_init()); photo(0x52);
}
static void test_partition_guards(void) {
  blank(); present=false; assert(!hr_gallery_init()); assert(begin(30,0)==0x1b);
  present=true; partition.address=0x380000; assert(!hr_gallery_init());
  partition.address=0x400000; partition.size=0xb0000; assert(!hr_gallery_init());
  partition.size=sizeof flash; fail_map=true; assert(!hr_gallery_init());
  fail_map=false; assert(hr_gallery_init());
}
int main(void) {
  test_sequence(); test_interrupt_session_timeout(); test_decode_failures();
  test_atomic_flash_failures(); test_capacity_abort_and_bad_frames(); test_partition_guards();
  puts("gallery: protocol, session, decoder, atomic failure/reboot tests passed");
}
