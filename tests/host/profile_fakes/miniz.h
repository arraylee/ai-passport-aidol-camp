#pragma once
#include <stddef.h>
#include <stdint.h>
typedef struct {uint8_t state[10992];} tinfl_decompressor;
typedef int tinfl_status;
#define TINFL_FLAG_PARSE_ZLIB_HEADER 1
#define TINFL_FLAG_HAS_MORE_INPUT 2
#define TINFL_STATUS_DONE 0
#define TINFL_STATUS_HAS_MORE_OUTPUT 2
#define TINFL_STATUS_NEEDS_MORE_INPUT 1
static inline void tinfl_init(tinfl_decompressor *d){(void)d;}
static inline int tinfl_decompress(tinfl_decompressor *d,const uint8_t *in,size_t *in_n,uint8_t *base,uint8_t *out,size_t *out_n,uint32_t flags){(void)d;(void)in;(void)in_n;(void)base;(void)out;(void)out_n;(void)flags;return -1;}
