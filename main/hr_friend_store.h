#pragma once
#include "hr_edit.h"
#include "hr_core.h"
#include "hr_profiles.h"
#define HR_FRIEND_BANK 16384u
#define HR_FRIEND_IMAGE_BYTES (64u * 104u * 2u)
#define HR_FRIEND_IMAGE_OFFSET 256u
#define HR_FRIEND_THUMB_OFFSET (HR_FRIEND_IMAGE_OFFSET + HR_FRIEND_IMAGE_BYTES)
bool hr_friend_store_init(unsigned owner);
void hr_friend_store_abort(void);
bool hr_friend_store_begin(unsigned peer, uint32_t session,
                           const uint8_t wire[164], uint32_t snapshot_crc);
bool hr_friend_store_append(unsigned offset, const uint8_t *data, size_t n);
bool hr_friend_store_finish(void);
/* Only already verified/adopted snapshots; avoids erasing a matching card. */
bool hr_friend_store_matches(unsigned peer, const uint8_t wire[164], uint32_t crc);
const hr_profile_t *hr_friend_profile(unsigned id);
/* Incremental CRC uses initial ~0u and final xor ~0u, like hr_crc. */
uint32_t hr_friend_crc_update(uint32_t crc, const void *data, size_t n);

void hr_friend_store_bind(const hr_core_t *c);
