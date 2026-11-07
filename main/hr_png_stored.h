#pragma once
#include <stddef.h>
#include <stdint.h>
/* No dictionary or heap allocation. Validate once with sink=NULL before writing
 * a frame. Returns 0 for a verified stored-DEFLATE PNG, 1 if compressed DEFLATE
 * requires the regular decoder, -1 malformed/checksum/length, -2 sink failure.
 */
typedef int (*png_stored_sink_t)(void *, const uint8_t *, size_t);
int png_stored_stream(const uint8_t *png, size_t size, size_t expected,
                      png_stored_sink_t sink, void *user);
