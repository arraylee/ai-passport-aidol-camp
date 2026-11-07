#pragma once
#include "hr_ui.h"
bool hr_ble_init(unsigned id);
bool hr_ble_connected(void);
void hr_ble_poll(hr_view_t *view);

bool hr_ble_upload_busy(void);
void hr_ble_pause(void);
void hr_ble_resume(void);

bool hr_ble_take_reset(void);
bool hr_management_busy(void);

const char *hr_ble_sn(void);
