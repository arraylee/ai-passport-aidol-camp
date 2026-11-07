#include "hr_gallery.h"
#include "esp_jpeg_dec.h"
#include "esp_partition.h"
#include "hr_core.h"
#include <stdlib.h>
#include <string.h>

/* Two independently committed RGB565 banks and one JPEG upload scratch bank.
 * No partition other than the exact gallery_store layout can be written. */
#define BANK_SIZE 0x40000u
#define SCRATCH (2u * BANK_SIZE)
#define HEADER_SIZE 32u
#define MAX_WIDTH 240u
#define MAX_HEIGHT 320u
#define STRIPE_BYTES (MAX_WIDTH * 16u * 2u)
#define UPLOAD_TIMEOUT 10000u

static const esp_partition_t *part;
static const uint8_t *mapped;
static esp_partition_mmap_handle_t mapping;
static int active = -1;
static uint32_t revision, upload_total, upload_used, upload_at;
static bool decoding, cancelled;
static lv_image_dsc_t images[2];
extern bool hr_profile_session_valid(void);

static uint32_t get32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
         (uint32_t)p[3] << 24;
}
static void put32(uint8_t *p, uint32_t v) {
  for (unsigned i = 0; i < 4; ++i)
    p[i] = v >> (8 * i);
}
static bool dimensions(unsigned w, unsigned h) {
  /* esp_new_jpeg 1.0.2 block mode requires dimensions divisible by eight. */
  return w && h && w <= MAX_WIDTH && h <= MAX_HEIGHT && !(w % 8) && !(h % 8);
}
static bool valid_bank(unsigned bank) {
  const uint8_t *h = mapped + bank * BANK_SIZE;
  if (memcmp(h, "HRGA", 4) || get32(h + 4) != 1 ||
      get32(h + 28) != hr_crc(h, 28))
    return false;
  unsigned w = get32(h + 12), height = get32(h + 16);
  uint32_t bytes = get32(h + 20);
  return dimensions(w, height) && bytes == w * height * 2u &&
         get32(h + 24) == hr_crc(h + HEADER_SIZE, bytes);
}
static void adopt(unsigned bank) {
  const uint8_t *h = mapped + bank * BANK_SIZE;
  unsigned w = get32(h + 12), height = get32(h + 16);
  images[bank] = (lv_image_dsc_t){.header = {.magic = LV_IMAGE_HEADER_MAGIC,
                                             .cf = LV_COLOR_FORMAT_RGB565,
                                             .w = w,
                                             .h = height,
                                             .stride = w * 2},
                                  .data_size = w * height * 2,
                                  .data = h + HEADER_SIZE};
  active = bank;
  revision = get32(h + 8);
}
bool hr_gallery_init(void) {
  hr_gallery_abort();
  active = -1;
  revision = 0;
  if (mapped)
    esp_partition_munmap(mapping);
  mapped = NULL;
  part = NULL;
  const esp_partition_t *candidate =
      esp_partition_find_first(ESP_PARTITION_TYPE_DATA, 0x43, "gallery_store");
  if (!candidate || candidate->address != 0x400000 ||
      candidate->size != 3u * BANK_SIZE)
    return false;
  if (esp_partition_mmap(candidate, 0, candidate->size, ESP_PARTITION_MMAP_DATA,
                         (const void **)&mapped, &mapping) != ESP_OK) {
    mapped = NULL;
    return false;
  }
  part = candidate;
  if (valid_bank(0))
    adopt(0);
  if (valid_bank(1) &&
      (active < 0 || (int32_t)(get32(mapped + BANK_SIZE + 8) - revision) > 0))
    adopt(1);
  return true;
}
void hr_gallery_abort(void) {
  upload_total = upload_used = 0;
  cancelled = true;
}
void hr_gallery_expire(uint32_t now) {
  if (upload_total && (uint32_t)(now - upload_at) > UPLOAD_TIMEOUT)
    hr_gallery_abort();
}
bool hr_gallery_busy(void) { return upload_total || decoding; }
const lv_image_dsc_t *hr_gallery_image_descriptor(void) {
  return active < 0 ? NULL : &images[active];
}

/* Reject unsupported dimensions/standards before the decoder allocates its
 * workspace. Bound every segment and require a single baseline scan with EOI.
 * Huffman/quantization and entropy correctness remain the decoder's job. */
static bool probe(const uint8_t *p, size_t n, unsigned *w, unsigned *height) {
  if (n < 4 || p[0] != 0xff || p[1] != 0xd8)
    return false;
  bool frame = false;
  size_t at = 2;
  while (at < n) {
    if (p[at++] != 0xff)
      return false;
    while (at < n && p[at] == 0xff)
      ++at;
    if (at == n)
      return false;
    unsigned marker = p[at++];
    if (marker == 0 || marker == 0xd8 || marker == 0xd9 ||
        (marker >= 0xd0 && marker <= 0xd7) || marker == 1)
      return false;
    if (n - at < 2)
      return false;
    size_t len = (size_t)p[at] * 256 + p[at + 1];
    if (len < 2 || len > n - at)
      return false;
    if (marker >= 0xc0 && marker <= 0xcf && marker != 0xc4 && marker != 0xc0)
      return false;
    if (marker == 0xc0) {
      if (frame || len < 8 || p[at + 2] != 8)
        return false;
      unsigned components = p[at + 7];
      if ((components != 1 && components != 3) || len != 8u + components * 3u)
        return false;
      *height = (unsigned)p[at + 3] * 256 + p[at + 4];
      *w = (unsigned)p[at + 5] * 256 + p[at + 6];
      if (!dimensions(*w, *height))
        return false;
      frame = true;
    }
    if (marker == 0xda) {
      if (!frame || len < 6 || (p[at + 2] != 1 && p[at + 2] != 3) ||
          len != 6u + 2u * p[at + 2] || p[at + len - 3] != 0 ||
          p[at + len - 2] != 63 || p[at + len - 1] != 0)
        return false;
      at += len;
      bool entropy = false;
      while (at < n) {
        unsigned byte = p[at++];
        if (byte != 0xff) {
          entropy = true;
          continue;
        }
        while (at < n && p[at] == 0xff)
          ++at;
        if (at == n)
          return false;
        marker = p[at++];
        if (marker == 0 || (marker >= 0xd0 && marker <= 0xd7)) {
          entropy = true;
          continue;
        }
        return marker == 0xd9 && entropy && n - at <= 64;
      }
      return false;
    }
    at += len;
  }
  return false;
}
static uint32_t crc_continue(uint32_t crc, const uint8_t *p, size_t n) {
  while (n--) {
    crc ^= *p++;
    for (unsigned bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ (0xedb88320u & -(crc & 1));
  }
  return crc;
}
static bool session_ok(void) {
  return !cancelled && hr_profile_session_valid();
}
static unsigned decode_commit(size_t n) {
  unsigned w = 0, height = 0;
  if (!probe(mapped + SCRATCH, n, &w, &height))
    return 0x17;
  jpeg_dec_config_t cfg = DEFAULT_JPEG_DEC_CONFIG();
  cfg.output_type = JPEG_PIXEL_FORMAT_RGB565_LE;
  cfg.block_enable = true;
  uint8_t *stripe = aligned_alloc(16, STRIPE_BYTES);
  if (!stripe)
    return 0x1b;
  jpeg_dec_handle_t dec = NULL;
  jpeg_error_t err = jpeg_dec_open(&cfg, &dec);
  if (err != JPEG_ERR_OK) {
    free(stripe);
    return err == JPEG_ERR_NO_MEM ? 0x1b : 0x17;
  }
  jpeg_dec_io_t io = {.inbuf = (uint8_t *)(mapped + SCRATCH), .inbuf_len = n};
  jpeg_dec_header_info_t info = {0};
  unsigned status = 0x17;
  int outlen = 0, count = 0;
  if (jpeg_dec_parse_header(dec, &io, &info) != JPEG_ERR_OK ||
      info.width != w || info.height != height ||
      jpeg_dec_get_outbuf_len(dec, &outlen) != JPEG_ERR_OK ||
      jpeg_dec_get_process_count(dec, &count) != JPEG_ERR_OK || outlen <= 0 ||
      outlen > (int)STRIPE_BYTES || count <= 0 ||
      count > (int)((height + 7) / 8))
    goto done;
  unsigned bank = active == 0 ? 1 : 0;
  if (!session_ok() ||
      esp_partition_erase_range(part, bank * BANK_SIZE, BANK_SIZE) != ESP_OK) {
    status = 0x16;
    goto done;
  }
  io.outbuf = stripe;
  uint32_t offset = 0, bytes = w * height * 2, crc = ~0u;
  for (int i = 0; i < count; ++i) {
    if (!session_ok()) {
      status = 0x16;
      goto done;
    }
    io.out_size = 0;
    err = jpeg_dec_process(dec, &io);
    /* An error with a full-sized final block is still a corrupt image. */
    if (err != JPEG_ERR_OK || io.out_size <= 0 || io.out_size > outlen ||
        (uint32_t)io.out_size > bytes - offset || io.out_size % (w * 2))
      goto done;
    if (!session_ok() ||
        esp_partition_write(part, bank * BANK_SIZE + HEADER_SIZE + offset,
                            stripe, io.out_size) != ESP_OK) {
      status = 0x16;
      goto done;
    }
    crc = crc_continue(crc, stripe, io.out_size);
    offset += io.out_size;
  }
  if (offset != bytes)
    goto done;
  crc = ~crc;
  if (hr_crc(mapped + bank * BANK_SIZE + HEADER_SIZE, bytes) != crc ||
      !session_ok()) {
    status = 0x16;
    goto done;
  }
  uint8_t header[HEADER_SIZE] = {'H', 'R', 'G', 'A'};
  put32(header + 4, 1);
  put32(header + 8, revision + 1);
  put32(header + 12, w);
  put32(header + 16, height);
  put32(header + 20, bytes);
  put32(header + 24, crc);
  put32(header + 28, hr_crc(header, 28));
  /* The sole commit point: every pixel was decoded and read back, and the
   * originating BLE session is still current. A torn header is invalid. */
  if (esp_partition_write(part, bank * BANK_SIZE, header, sizeof header) !=
          ESP_OK ||
      !valid_bank(bank)) {
    status = 0x16;
    goto done;
  }
  adopt(bank);
  status = 1;
done:
  jpeg_dec_close(dec);
  free(stripe);
  return status;
}
unsigned hr_gallery_image(const uint8_t *p, size_t n, uint32_t now) {
  if (!part || decoding)
    return 0x1b;
  if (!p || !n)
    return 0x10;
  if (upload_total && (uint32_t)(now - upload_at) > UPLOAD_TIMEOUT) {
    hr_gallery_abort();
    return 0x11;
  }
  if (!hr_profile_session_valid()) {
    hr_gallery_abort();
    return 0x16;
  }
  upload_at = now;
  if (p[0] == 0) {
    if (n != 5)
      return 0x10;
    if (upload_total)
      return 0x1b;
    uint32_t total = get32(p + 1);
    if (!total || total > BANK_SIZE)
      return 0x15;
    if (esp_partition_erase_range(part, SCRATCH, BANK_SIZE) != ESP_OK)
      return 0x16;
    upload_total = total;
    upload_used = 0;
    cancelled = false;
    return 0;
  }
  if (!upload_total)
    return 0x12;
  if (p[0] == 1) {
    if (n < 2 || n - 1 > upload_total - upload_used) {
      hr_gallery_abort();
      return 0x15;
    }
    if (esp_partition_write(part, SCRATCH + upload_used, p + 1, n - 1) !=
        ESP_OK) {
      hr_gallery_abort();
      return 0x16;
    }
    upload_used += n - 1;
    return 0;
  }
  if (p[0] != 2 || n != 1) {
    hr_gallery_abort();
    return 0x10;
  }
  if (upload_used != upload_total) {
    hr_gallery_abort();
    return 0x1a;
  }
  size_t size = upload_total;
  upload_total = upload_used = 0;
  decoding = true;
  unsigned status = decode_commit(size);
  decoding = false;
  return status;
}

void hr_gallery_detach(void){hr_gallery_abort();active=-1;revision=0;}
