#include "hr_profile_store.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "hr_core.h"
#include "hr_png.h"
#include "src/misc/lv_text_private.h"
#include <stdio.h>
#include <string.h>
static const esp_partition_t *part;
static const uint8_t *mapped;
static esp_partition_mmap_handle_t mapping;
static hr_edit_t current;
static int active = -1;
static unsigned selected;
static uint32_t upload_total, upload_used, upload_at;
static lv_image_dsc_t cards[2], smalls[2], portraits[2];
static const hr_profile_t *base(unsigned id) {
  extern const hr_profile_t *hr_profile_base(unsigned);
  return hr_profile_base(id);
}
static bool valid_bank(unsigned bank, hr_edit_t *p) {
  const uint8_t *b = mapped + bank * HR_EDIT_BANK;
  return hr_edit_decode(p, b) &&
         (!p->avatar ||
          hr_crc(b + HR_EDIT_FULL_OFFSET,
                 HR_EDIT_IMAGE_END - HR_EDIT_FULL_OFFSET) == p->image_crc);
}
static void adopt(int bank) {
  if (selected)
    hr_profile_clear(selected);
  active = bank;
  if (bank < 0) {
    memset(&current, 0, sizeof current);
    current.id = selected;
    snprintf(current.name, 49, "%s", base(selected)->name);
    snprintf(current.intro, 85, "%s", base(selected)->signature);
    return;
  }
  hr_edit_decode(&current, mapped + bank * HR_EDIT_BANK);
  hr_profile_set_title(selected, current.title);
  hr_profile_override(selected, current.name, current.intro,
                      current.avatar ? &cards[bank] : NULL,
                      current.avatar ? &smalls[bank] : NULL);
  if (current.avatar)
    hr_profile_set_portrait(selected, &portraits[bank]);
}
bool hr_profile_store_init(unsigned id) {
  part =
      esp_partition_find_first(ESP_PARTITION_TYPE_DATA, 0x41, "profile_store");
  if (!part || part->address != 0x380000 || part->size != 0x30000 ||
      esp_partition_mmap(part, 0, part->size, ESP_PARTITION_MMAP_DATA,
                         (const void **)&mapped, &mapping) != ESP_OK)
    return false;
  for (int b = 0; b < 2; b++) {
    portraits[b] = (lv_image_dsc_t){.header = {.magic = LV_IMAGE_HEADER_MAGIC,
                                               .cf = LV_COLOR_FORMAT_RGB565,
                                               .w = 96,
                                               .h = 156,
                                               .stride = 192},
                                    .data_size = 29952,
                                    .data = mapped + b * HR_EDIT_BANK +
                                            HR_EDIT_FULL_OFFSET};
    cards[b] = (lv_image_dsc_t){.header = {.magic = LV_IMAGE_HEADER_MAGIC,
                                           .cf = LV_COLOR_FORMAT_RGB565,
                                           .w = 96,
                                           .h = 132,
                                           .stride = 192},
                                .data_size = 25344,
                                .data = mapped + b * HR_EDIT_BANK +
                                        HR_EDIT_CARD_OFFSET};
    smalls[b] = (lv_image_dsc_t){.header = {.magic = LV_IMAGE_HEADER_MAGIC,
                                            .cf = LV_COLOR_FORMAT_RGB565,
                                            .w = 32,
                                            .h = 32,
                                            .stride = 64},
                                 .data_size = 2048,
                                 .data = mapped + b * HR_EDIT_BANK +
                                         HR_EDIT_SMALL_OFFSET};
  }
  hr_profile_store_select(id);
  return true;
}
void hr_profile_store_select(unsigned id) {
  if (selected)
    hr_profile_clear(selected);
  selected = id;
  hr_profile_abort();
  int best = -1;
  hr_edit_t a, b;
  if (mapped) {
    bool va = valid_bank(0, &a) && a.id == id,
         vb = valid_bank(1, &b) && b.id == id;
    if (va)
      best = 0;
    if (vb && (!va || (int32_t)(b.revision - a.revision) > 0))
      best = 1;
  }
  adopt(best);
}
const hr_edit_t *hr_profile_current(void) { return &current; }
void hr_profile_abort(void) { upload_total = upload_used = 0; }
static bool write_at(unsigned bank, unsigned offset, const void *p, size_t n) {
  return esp_partition_write(part, bank * HR_EDIT_BANK + offset, p, n) ==
         ESP_OK;
}
static bool commit(unsigned bank, hr_edit_t *p) {
  extern bool hr_profile_session_valid(void);
  if (!hr_profile_session_valid())
    return false;
  uint8_t header[HR_EDIT_HEADER];
  p->id = selected;
  p->revision = current.revision + 1;
  if (p->avatar)
    p->image_crc = hr_crc(mapped + bank * HR_EDIT_BANK + HR_EDIT_FULL_OFFSET,
                          HR_EDIT_IMAGE_END - HR_EDIT_FULL_OFFSET);
  hr_edit_encode(p, header);
  hr_edit_t verify;
  if (!write_at(bank, 0, header, sizeof header) || !valid_bank(bank, &verify))
    return false;
  adopt(bank);
  ESP_LOGI("profile", "saved revision=%lu id=%u avatar=%u",
           (unsigned long)current.revision, current.id, current.avatar);
  return true;
}
unsigned hr_profile_config(const uint8_t *data, size_t n) {
  if (!part || !selected)
    return 0x1b;
  if (upload_total)
    return 0x1b;
  hr_edit_t p = current;
  unsigned s = hr_edit_config(&p, data, n);
  if (s != 1)
    return s;
  // Validate font coverage before committing a value that could display as
  // boxes.
  LV_FONT_DECLARE(hr_font16);
  for (int field = 0; field < 3; field++) {
    const char *text = field == 2 ? p.title : field ? p.intro : p.name;
    for (uint32_t at = 0; text[at];) {
      uint32_t cp = lv_text_encoded_next(text, &at);
      lv_font_glyph_dsc_t g;
      if (!lv_font_get_glyph_dsc(&hr_font16, &g, cp, 0))
        return 0x14;
    }
  }
  if (!strcmp(p.name, current.name) && !strcmp(p.intro, current.intro) &&
      !strcmp(p.title, current.title))
    return 1;
  unsigned bank = active == 0 ? 1 : 0;
  if (esp_partition_erase_range(part, bank * HR_EDIT_BANK, HR_EDIT_BANK) !=
      ESP_OK)
    return 0x16;
  if (p.avatar) {
    uint8_t block[512];
    for (unsigned at = HR_EDIT_FULL_OFFSET; at < HR_EDIT_IMAGE_END;
         at += sizeof block) {
      memcpy(block, mapped + active * HR_EDIT_BANK + at, sizeof block);
      if (!write_at(bank, at, block, sizeof block))
        return 0x16;
    }
  }
  return commit(bank, &p) ? 1 : 0x16;
}
static int row(void *user, unsigned y, const uint16_t pixels[96]) {
  unsigned bank = (unsigned)(uintptr_t)user;
  if (!write_at(bank, HR_EDIT_FULL_OFFSET + y * 192, pixels, 192))
    return -1;
  uint16_t out[96];
  for (unsigned dst = 0; dst < 132; dst++)
    if (dst * 156 / 132 == y) {
      for (unsigned x = 0; x < 96; x++)
        out[x] = x >= 7 && x < 88 ? pixels[(x - 7) * 96 / 81] : 0x0864;
      if (!write_at(bank, HR_EDIT_CARD_OFFSET + dst * 192, out, 192))
        return -1;
    }
  for (unsigned dst = 0; dst < 32; dst++)
    if (dst * 156 / 32 == y) {
      for (unsigned x = 0; x < 32; x++)
        out[x] = x >= 6 && x < 25 ? pixels[(x - 6) * 96 / 19] : 0x0864;
      if (!write_at(bank, HR_EDIT_SMALL_OFFSET + dst * 64, out, 64))
        return -1;
    }
  return 0;
}
unsigned hr_profile_image(const uint8_t *p, size_t n, uint32_t now) {
  if (!part || !selected)
    return 0x1b;
  if (!n)
    return 0x10;
  if (upload_total && (uint32_t)(now - upload_at) > 10000) {
    hr_profile_abort();
    return 0x11;
  }
  upload_at = now;
  if (p[0] == 0) {
    if (n != 5)
      return 0x10;
    if (upload_total)
      return 0x1b;
    uint32_t total = (uint32_t)p[1] | (uint32_t)p[2] << 8 |
                     (uint32_t)p[3] << 16 | (uint32_t)p[4] << 24;
    if (!total || total > 65536)
      return 0x15;
    if (esp_partition_erase_range(part, 0x20000, 65536) != ESP_OK)
      return 0x16;
    upload_total = total;
    upload_used = 0;
    return 0;
  }
  if (!upload_total)
    return 0x12;
  if (p[0] == 1) {
    if (n < 2 || n - 1 > upload_total - upload_used) {
      hr_profile_abort();
      return 0x15;
    }
    if (esp_partition_write(part, 0x20000 + upload_used, p + 1, n - 1) !=
        ESP_OK) {
      hr_profile_abort();
      return 0x16;
    }
    upload_used += n - 1;
    return 0;
  }
  if (p[0] != 2 || n != 1) {
    hr_profile_abort();
    return 0x10;
  }
  if (upload_used != upload_total) {
    hr_profile_abort();
    return 0x1a;
  }
  unsigned bank = active == 0 ? 1 : 0;
  size_t size = upload_total;
  hr_profile_abort();
  if (esp_partition_erase_range(part, bank * HR_EDIT_BANK, HR_EDIT_BANK) !=
      ESP_OK)
    return 0x16;
  int rc = hr_png_decode(mapped + 0x20000, size, row, (void *)(uintptr_t)bank);
  if (rc)
    return rc == -2 ? 0x1b : 0x17;
  hr_edit_t edited = current;
  edited.avatar = true;
  return commit(bank, &edited) ? 1 : 0x16;
}

void hr_profile_expire(uint32_t now) {
  if (upload_total && (uint32_t)(now - upload_at) > 10000)
    hr_profile_abort();
}

bool hr_profile_upload_busy(void) { return upload_total != 0; }

void hr_profile_store_detach(void){hr_profile_abort();adopt(-1);}
