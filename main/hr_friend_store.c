#include "hr_friend_store.h"
#include "esp_partition.h"
#include "hr_core.h"
#include <string.h>

static const esp_partition_t *part;
static const uint8_t *mapped;
static esp_partition_mmap_handle_t mapping;
static unsigned owner_id;
static uint8_t owner_uid[6], peer_uids[HR_PLAYERS+1][6];
static int active[HR_PLAYERS + 1];
static hr_profile_t profiles[HR_PLAYERS + 1];
static hr_edit_t edits[HR_PLAYERS + 1];
static lv_image_dsc_t images[HR_PLAYERS + 1], thumbs[HR_PLAYERS + 1];
static struct {
  bool active;
  unsigned peer, bank, used;
  uint32_t session, token, revision;
  uint8_t wire[164];
  hr_edit_t edit;
} transaction;
static uint32_t get32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
         (uint32_t)p[3] << 24;
}
static void put32(uint8_t *p, uint32_t n) {
  for (unsigned i = 0; i < 4; i++)
    p[i] = n >> (i * 8);
}
uint32_t hr_friend_crc_update(uint32_t crc, const void *data, size_t n) {
  const uint8_t *p = data;
  while (n--) {
    crc ^= *p++;
    for (unsigned i = 0; i < 8; i++)
      crc = (crc >> 1) ^ (0xedb88320u & -(crc & 1));
  }
  return crc;
}
static unsigned address(unsigned peer, unsigned bank) {
  return ((peer - 1) * 2 + bank) * HR_FRIEND_BANK;
}
static bool canonical(const uint8_t wire[164], hr_edit_t *edit) {
  uint8_t copy[164];
  if (!hr_edit_wire_decode(edit, wire))
    return false;
  hr_edit_wire_encode(edit, copy);
  return !memcmp(copy, wire, sizeof copy);
}
static bool valid(unsigned peer, unsigned bank, hr_edit_t *edit) {
  const uint8_t *b = mapped + address(peer, bank);
  if (memcmp(b, "HFC2", 4) || b[4] != owner_id || b[5] != peer || b[6] != 2 ||
      memcmp(b+196,owner_uid,6)||memcmp(b+202,peer_uids[peer],6)||
      memcmp(b + 252, "DONE", 4) || get32(b + 248) != hr_crc(b, 248) ||
      !canonical(b + 32, edit))
    return false;
  unsigned bytes = edit->avatar ? HR_FRIEND_IMAGE_BYTES : 0;
  uint32_t crc = hr_friend_crc_update(~0u, b + 32, 164);
  crc = hr_friend_crc_update(crc, b + HR_FRIEND_IMAGE_OFFSET, bytes);
  if (~crc != get32(b + 16) ||
      (edit->avatar &&
       hr_crc(b + HR_FRIEND_THUMB_OFFSET, 2048) != get32(b + 20)))
    return false;
  edit->revision = get32(b + 8);
  return true;
}
void hr_friend_store_bind(const hr_core_t *c) {
  bool owner_changed=memcmp(owner_uid,c->mac,6)!=0;
  memcpy(owner_uid,c->mac,6);
  for(unsigned i=1;i<=HR_PLAYERS;i++) {
    if(owner_changed||memcmp(peer_uids[i],c->peers[i].mac,6)) {
      active[i]=-1;memset(&profiles[i],0,sizeof profiles[i]);
    }
    memcpy(peer_uids[i],c->peers[i].mac,6);
  }
}
static void adopt(unsigned peer, unsigned bank, const hr_edit_t *edit) {
  active[peer] = bank;
  edits[peer] = *edit;
  const uint8_t *b = mapped + address(peer, bank);
  images[peer] = (lv_image_dsc_t){.header = {.magic = LV_IMAGE_HEADER_MAGIC,
                                             .cf = LV_COLOR_FORMAT_RGB565,
                                             .w = 64,
                                             .h = 104,
                                             .stride = 128},
                                  .data_size = HR_FRIEND_IMAGE_BYTES,
                                  .data = b + HR_FRIEND_IMAGE_OFFSET};
  thumbs[peer] = (lv_image_dsc_t){.header = {.magic = LV_IMAGE_HEADER_MAGIC,
                                             .cf = LV_COLOR_FORMAT_RGB565,
                                             .w = 32,
                                             .h = 32,
                                             .stride = 64},
                                  .data_size = 2048,
                                  .data = b + HR_FRIEND_THUMB_OFFSET};
  profiles[peer] =
      (hr_profile_t){.name = edits[peer].name,
                     .signature = edits[peer].intro,
                     .url = "",
                     .card = edit->avatar ? &images[peer] : NULL,
                     .portrait = edit->avatar ? &images[peer] : NULL,
                     .small = edit->avatar ? &thumbs[peer] : NULL};
}
void hr_friend_store_abort(void) {
  memset(&transaction, 0, sizeof transaction);
}
bool hr_friend_store_init(unsigned owner) {
  hr_friend_store_abort();
  owner_id = owner;
  memset(profiles, 0, sizeof profiles);
  for (unsigned id = 0; id <= HR_PLAYERS; id++)
    active[id] = -1;
  if (owner < 1 || owner > HR_PLAYERS)
    return false;
  if (!mapped) {
    part =
        esp_partition_find_first(ESP_PARTITION_TYPE_DATA, 0x42, "friend_store");
    if (!part || part->address != 0x3b0000 || part->size != 0x30000 ||
        esp_partition_mmap(part, 0, part->size, ESP_PARTITION_MMAP_DATA,
                           (const void **)&mapped, &mapping) != ESP_OK) {
      part = NULL;
      mapped = NULL;
      return false;
    }
  }
  for (unsigned id = 1; id <= HR_PLAYERS; id++) {
    if (id == owner)
      continue;
    hr_edit_t a, b;
    bool va = valid(id, 0, &a), vb = valid(id, 1, &b);
    if (vb && (!va || (int32_t)(b.revision - a.revision) > 0))
      adopt(id, 1, &b);
    else if (va)
      adopt(id, 0, &a);
  }
  return true;
}
const hr_profile_t *hr_friend_profile(unsigned id) {
  return id >= 1 && id <= HR_PLAYERS && profiles[id].name ? &profiles[id]
                                                          : NULL;
}
bool hr_friend_store_matches(unsigned peer, const uint8_t wire[164], uint32_t crc) {
  if (!mapped || peer < 1 || peer > HR_PLAYERS || active[peer] < 0)
    return false;
  const uint8_t *b = mapped + address(peer, (unsigned)active[peer]);
  return get32(b + 16) == crc && !memcmp(b + 32, wire, 164);
}
bool hr_friend_store_begin(unsigned peer, uint32_t session,
                           const uint8_t wire[164], uint32_t snapshot_crc) {
  hr_friend_store_abort();
  static const uint8_t zero[6];
  if (!part || !memcmp(owner_uid,zero,6) || peer>HR_PLAYERS || !memcmp(peer_uids[peer],zero,6) || owner_id < 1 || owner_id > HR_PLAYERS || peer < 1 ||
      peer > HR_PLAYERS || peer == owner_id ||
      !canonical(wire, &transaction.edit))
    return false;
  transaction.peer = peer;
  transaction.bank = active[peer] == 0 ? 1 : 0;
  transaction.revision = active[peer] < 0 ? 1 : edits[peer].revision + 1;
  transaction.session = session;
  transaction.token = snapshot_crc;
  memcpy(transaction.wire, wire, 164);
  if (esp_partition_erase_range(part, address(peer, transaction.bank),
                                HR_FRIEND_BANK) != ESP_OK)
    return false;
  transaction.active = true;
  return true;
}
static bool write_at(unsigned offset, const void *data, size_t n) {
  return esp_partition_write(
             part, address(transaction.peer, transaction.bank) + offset, data,
             n) == ESP_OK;
}
bool hr_friend_store_append(unsigned offset, const uint8_t *data, size_t n) {
  if (!transaction.active || !transaction.edit.avatar ||
      offset != transaction.used || offset > HR_FRIEND_IMAGE_BYTES || !n ||
      n > HR_FRIEND_IMAGE_BYTES - offset)
    return false;
  if (!write_at(HR_FRIEND_IMAGE_OFFSET + offset, data, n)) {
    hr_friend_store_abort();
    return false;
  }
  transaction.used += n;
  return true;
}
bool hr_friend_store_finish(void) {
  if (!transaction.active ||
      transaction.used != (transaction.edit.avatar ? HR_FRIEND_IMAGE_BYTES : 0))
    return false;
  const uint8_t *b = mapped + address(transaction.peer, transaction.bank);
  uint32_t crc = hr_friend_crc_update(~0u, transaction.wire, 164);
  crc = hr_friend_crc_update(crc, b + HR_FRIEND_IMAGE_OFFSET, transaction.used);
  if (~crc != transaction.token) {
    hr_friend_store_abort();
    return false;
  }
  if (transaction.edit.avatar) {
    uint8_t row[64];
    for (unsigned y = 0; y < 32; y++) {
      for (unsigned x = 0; x < 32; x++)
        memcpy(row + x * 2,
               b + HR_FRIEND_IMAGE_OFFSET + ((y * 104 / 32) * 64 + x * 2) * 2,
               2);
      if (!write_at(HR_FRIEND_THUMB_OFFSET + y * 64, row, sizeof row)) {
        hr_friend_store_abort();
        return false;
      }
    }
  }
  uint8_t header[256] = {0};
  memcpy(header, "HFC2", 4);
  header[4] = owner_id;
  header[5] = transaction.peer;
  header[6] = 2;
  put32(header + 8, transaction.revision);
  put32(header + 12, transaction.session);
  put32(header + 16, transaction.token);
  put32(header + 20,
        transaction.edit.avatar ? hr_crc(b + HR_FRIEND_THUMB_OFFSET, 2048) : 0);
  memcpy(header + 32, transaction.wire, 164);
  memcpy(header+196,owner_uid,6);memcpy(header+202,peer_uids[transaction.peer],6);
  put32(header + 248, hr_crc(header, 248));
  memcpy(header + 252, "DONE", 4);
  hr_edit_t verified;
  bool ok = write_at(0, header, 252) && write_at(252, header + 252, 4) &&
            valid(transaction.peer, transaction.bank, &verified);
  if (ok)
    adopt(transaction.peer, transaction.bank, &verified);
  hr_friend_store_abort();
  return ok;
}
