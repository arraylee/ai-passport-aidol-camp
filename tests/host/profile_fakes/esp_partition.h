#pragma once
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#define ESP_OK 0
#define ESP_PARTITION_TYPE_DATA 1
#define ESP_PARTITION_MMAP_DATA 0
typedef struct {unsigned address,size;} esp_partition_t;
typedef int esp_partition_mmap_handle_t;
const esp_partition_t *esp_partition_find_first(int,int,const char*);
int esp_partition_mmap(const esp_partition_t*,unsigned,unsigned,int,const void**,esp_partition_mmap_handle_t*);
int esp_partition_write(const esp_partition_t*,unsigned,const void*,size_t);
int esp_partition_erase_range(const esp_partition_t*,unsigned,unsigned);
