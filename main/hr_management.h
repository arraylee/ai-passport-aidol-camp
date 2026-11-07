#pragma once
#include "hr_ui.h"
#define HR_MANAGEMENT_BYTES 228
void hr_management_init(const char*sn);
void hr_management_connection(bool connected);
unsigned hr_management_command(hr_view_t*v,const uint8_t*p,size_t n);
bool hr_management_poll(hr_view_t*v);
bool hr_management_busy(void);
void hr_management_read(uint8_t out[HR_MANAGEMENT_BYTES]);
