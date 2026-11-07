#pragma once
#include "hr_core.h"
#define HR_IDENTITY_WIRE_VERSION 2
#define HR_IDENTITY_WIRE_MAX 212
#define HR_ROSTER_BYTES 54
/* Only the Wi-Fi callback source MAC is authoritative. The extra addresses
 * carry references; source role numbers are hints, never peer identities. */
typedef struct { hr_packet_t p; uint8_t target[6], speaker[6]; } hr_identity_frame_t;
void hr_identity_enable(hr_core_t *c);
void hr_identity_set_sn(hr_core_t *c,const char *public_sn);
size_t hr_identity_pack(const hr_core_t *c,const hr_packet_t *p,uint8_t out[HR_IDENTITY_WIRE_MAX]);
bool hr_identity_unpack(hr_identity_frame_t *f,const uint8_t *wire,size_t n);
bool hr_identity_route(hr_core_t *c,hr_identity_frame_t *f,const uint8_t source[6],uint32_t now);
void hr_identity_elect(hr_core_t *c,uint32_t now);
void hr_roster_encode(const hr_core_t *c,uint8_t out[HR_ROSTER_BYTES]);
bool hr_roster_decode(hr_core_t *c,const uint8_t in[HR_ROSTER_BYTES]);
