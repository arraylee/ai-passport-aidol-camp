#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
enum {HR_CONFIG_IDLE,HR_CONFIG_ERASING,HR_CONFIG_APPLY,HR_CONFIG_DONE,HR_CONFIG_ERROR};
typedef struct {uint8_t state,role,action,error,region;uint32_t txn,offset,completed;} hr_config_t;
typedef struct {
 bool (*mark)(uint8_t role,uint8_t action,uint32_t txn);
 bool (*erase)(unsigned region,uint32_t offset);
 bool (*settings)(uint8_t role);
 bool (*clear)(void);
} hr_config_ops_t;
unsigned hr_config_begin(hr_config_t*s,const hr_config_ops_t*ops,const uint8_t mac[6],uint32_t nonce,const uint8_t*p,size_t n);
void hr_config_recover(hr_config_t*s,uint8_t role,uint8_t action,uint32_t txn);
void hr_config_tick(hr_config_t*s,const hr_config_ops_t*ops);
bool hr_config_finish(hr_config_t*s,const hr_config_ops_t*ops,bool applied);
unsigned hr_config_percent(const hr_config_t*s);
