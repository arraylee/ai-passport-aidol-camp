#pragma once
#include "hr_core.h"
static inline const char *hr_name(unsigned id) {
  static const char *const names[HR_PLAYERS + 1] = {
      "未选择",  "1号设备", "2号设备", "3号设备",
      "4号设备", "5号设备", "6号设备"};
  return names[id <= HR_PLAYERS ? id : 0];
}
