#include "hr_png.h"
#include "hr_png_stored.h"
#include "miniz.h"
#include <stdlib.h>
#include <string.h>
typedef struct {
  uint8_t rows[2][384];
  uint16_t pixels[96];
  unsigned bpp, y, col, filter;
  bool have, error;
  hr_png_row_fn row;
  void *user;
} sink_t;
static uint32_t be(const uint8_t *p) {
  return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 |
         p[3];
}
static int paeth(int a, int b, int c) {
  int p = a + b - c, x = abs(p - a), y = abs(p - b), z = abs(p - c);
  return x <= y && x <= z ? a : y <= z ? b : c;
}
static int consume(void *u, const uint8_t *d, size_t n) {
  sink_t *s = u;
  while (n--) {
    if (s->y >= 156)
      return -1;
    if (!s->have) {
      s->filter = *d++;
      s->have = true;
      if (s->filter > 4)
        return -1;
      continue;
    }
    uint8_t *r = s->rows[s->y % 2], *prev = s->rows[1 - s->y % 2];
    unsigned i = s->col, a = i >= s->bpp ? r[i - s->bpp] : 0, b = prev[i],
             c = i >= s->bpp ? prev[i - s->bpp] : 0;
    unsigned v = *d++;
    if (s->filter == 1)
      v += a;
    else if (s->filter == 2)
      v += b;
    else if (s->filter == 3)
      v += (a + b) / 2;
    else if (s->filter == 4)
      v += paeth(a, b, c);
    r[i] = v;
    if (++s->col == 96 * s->bpp) {
      for (unsigned x = 0; x < 96; x++) {
        const uint8_t *p = r + x * s->bpp;
        unsigned alpha = s->bpp == 4 ? p[3] : 255, red = p[0],
                 green = s->bpp == 1 ? p[0] : p[1],
                 blue = s->bpp == 1 ? p[0] : p[2];
        red = (red * alpha + 8 * (255 - alpha) + 127) / 255;
        green = (green * alpha + 15 * (255 - alpha) + 127) / 255;
        blue = (blue * alpha + 32 * (255 - alpha) + 127) / 255;
        s->pixels[x] = ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3);
      }
      if (s->row(s->user, s->y, s->pixels))
        return -1;
      s->y++;
      s->col = 0;
      s->have = false;
    }
  }
  return 0;
}
int hr_png_decode(const uint8_t *p, size_t n, hr_png_row_fn row, void *user) {
  if (!p || n < 33 || memcmp(p, "\x89PNG\r\n\x1a\n", 8) || be(p + 16) != 96 ||
      be(p + 20) != 156 || p[24] != 8 || p[26] || p[27] || p[28])
    return -1;
  unsigned bpp = p[25] == 6 ? 4 : p[25] == 2 ? 3 : p[25] == 0 ? 1 : 0;
  if (!bpp)
    return -1;
  int stored = png_stored_stream(p, n, (96 * bpp + 1) * 156, NULL, NULL);
  if (stored < 0)
    return -1;
  sink_t *s = calloc(1, sizeof *s);
  if (!s)
    return -2;
  s->bpp = bpp;
  s->row = row;
  s->user = user;
  int rc = -1;
  if (!stored)
    rc = png_stored_stream(p, n, (96 * bpp + 1) * 156, consume, s);
  else {
    uint8_t *dict = malloc(32768);
    tinfl_decompressor *dec = malloc(sizeof *dec);
    if (!dict || !dec) {
      free(dict);
      free(dec);
      free(s);
      return -2;
    }
    tinfl_init(dec);
    size_t ofs = 0, total = 0;
    bool done = false, bad = false;
    for (size_t at = 8; at + 12 <= n; at += be(p + at) + 12)
      if (!memcmp(p + at + 4, "IDAT", 4))
        total += be(p + at);
    for (size_t at = 8; at + 12 <= n && !bad; at += be(p + at) + 12) {
      size_t len = be(p + at);
      if (memcmp(p + at + 4, "IDAT", 4))
        continue;
      const uint8_t *in = p + at + 8;
      total -= len;
      if (done && len) {
        bad = true;
        break;
      }
      do {
        size_t used = len, out = 32768 - ofs;
        tinfl_status st =
            tinfl_decompress(dec, in, &used, dict, dict + ofs, &out,
                             TINFL_FLAG_PARSE_ZLIB_HEADER |
                                 (total ? TINFL_FLAG_HAS_MORE_INPUT : 0));
        in += used;
        len -= used;
        if (consume(s, dict + ofs, out)) {
          bad = true;
          break;
        }
        ofs = (ofs + out) & 32767;
        if (st == TINFL_STATUS_DONE) {
          done = true;
          if (len || total)
            bad = true;
          break;
        }
        if (st < 0) {
          bad = true;
          break;
        }
        if (st == TINFL_STATUS_NEEDS_MORE_INPUT) {
          if (len || !total)
            bad = true;
          break;
        }
        if (!used && !out) {
          bad = true;
          break;
        }
      } while (true);
    }
    rc = done && !bad ? 0 : -1;
    free(dict);
    free(dec);
  }
  if (s->y != 156 || s->col || s->have)
    rc = -1;
  free(s);
  return rc;
}
