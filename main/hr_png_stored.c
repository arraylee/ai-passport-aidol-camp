#include "hr_png_stored.h"
#include <stdbool.h>
#include <string.h>
static uint32_t be32(const uint8_t *p) {
  return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 |
         p[3];
}
static uint32_t crc32(const uint8_t *p, size_t n) {
  uint32_t c = UINT32_MAX;
  while (n--) {
    c ^= *p++;
    for (unsigned i = 0; i < 8; i++)
      c = (c >> 1) ^ ((0u - (c & 1u)) & 0xedb88320u);
  }
  return ~c;
}
typedef struct {
  const uint8_t *png;
  size_t size, chunk, pos, left;
} reader_t;
static bool span(reader_t *r, const uint8_t **bytes, size_t *n) {
  while (!r->left) {
    if (r->chunk > r->size || r->size - r->chunk < 12)
      return false;
    size_t len = be32(r->png + r->chunk);
    const uint8_t *kind = r->png + r->chunk + 4;
    if (len > r->size - r->chunk - 12)
      return false;
    r->pos = r->chunk + 8;
    r->chunk += len + 12;
    if (!memcmp(kind, "IDAT", 4))
      r->left = len;
    else if (!memcmp(kind, "IEND", 4))
      return false;
  }
  *bytes = r->png + r->pos;
  *n = r->left;
  return true;
}
static bool byte(reader_t *r, uint8_t *b) {
  const uint8_t *p;
  size_t n;
  if (!span(r, &p, &n))
    return false;
  *b = *p;
  r->pos++;
  r->left--;
  return true;
}
/* Verify every PNG chunk before exposing data or modifying the frame. */
static bool chunks_valid(const uint8_t *png, size_t size) {
  if (!png || size < 33 || memcmp(png, "\x89PNG\r\n\x1a\n", 8))
    return false;
  size_t at = 8;
  bool first = true, idat = false, idat_ended = false;
  while (at <= size && size - at >= 12) {
    size_t n = be32(png + at);
    const uint8_t *kind = png + at + 4;
    if (n > size - at - 12 || crc32(kind, n + 4) != be32(kind + n + 4))
      return false;
    if (first) {
      if (n != 13 || memcmp(kind, "IHDR", 4))
        return false;
      first = false;
    } else if (!memcmp(kind, "IHDR", 4))
      return false;
    if (!memcmp(kind, "IDAT", 4)) {
      if (idat_ended)
        return false;
      idat = true;
    } else if (idat)
      idat_ended = true;
    at += n + 12;
    if (!memcmp(kind, "IEND", 4))
      return n == 0 && idat && at == size;
  }
  return false;
}
int png_stored_stream(const uint8_t *png, size_t size, size_t expected,
                      png_stored_sink_t sink, void *user) {
  if (!chunks_valid(png, size))
    return -1;
  reader_t r = {.png = png, .size = size, .chunk = 8};
  uint8_t cmf, flg;
  if (!byte(&r, &cmf) || !byte(&r, &flg) || (cmf & 15) != 8 || (cmf >> 4) > 7 ||
      ((unsigned)cmf * 256 + flg) % 31 || (flg & 32))
    return -1;
  size_t written = 0;
  uint32_t a = 1, b = 0;
  bool final = false;
  while (!final) {
    uint8_t header, lo, hi, nlo, nhi;
    if (!byte(&r, &header))
      return -1;
    final = (header & 1) != 0;
    unsigned type = (header >> 1) & 3;
    if (type == 3)
      return -1;
    if (type)
      return 1; /* No data emitted by the preflight call. */
    /* Stored blocks discard the five remaining alignment bits. */
    if (!byte(&r, &lo) || !byte(&r, &hi) || !byte(&r, &nlo) || !byte(&r, &nhi))
      return -1;
    unsigned len = (unsigned)lo + ((unsigned)hi << 8),
             inverse = (unsigned)nlo + ((unsigned)nhi << 8);
    if ((len ^ inverse) != 65535u || written > expected ||
        len > expected - written)
      return -1;
    size_t remaining = len;
    while (remaining) {
      const uint8_t *p;
      size_t n;
      if (!span(&r, &p, &n))
        return -1;
      if (n > remaining)
        n = remaining;
      for (size_t i = 0; i < n; i++) {
        a = (a + p[i]) % 65521u;
        b = (b + a) % 65521u;
      }
      if (sink && sink(user, p, n))
        return -2;
      r.pos += n;
      r.left -= n;
      remaining -= n;
      written += n;
    }
  }
  uint8_t sum[4];
  for (unsigned i = 0; i < 4; i++)
    if (!byte(&r, &sum[i]))
      return -1;
  const uint8_t *extra;
  size_t n;
  if (written != expected || be32(sum) != ((b << 16) | a) ||
      span(&r, &extra, &n))
    return -1;
  return 0;
}
