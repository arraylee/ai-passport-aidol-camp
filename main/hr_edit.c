#include "hr_edit.h"
#include "cJSON.h"
#include "hr_core.h"
#include <math.h>
#include <string.h>
static uint32_t get32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
         (uint32_t)p[3] << 24;
}
static void put32(uint8_t *p, uint32_t v) {
  for (int i = 0; i < 4; i++)
    p[i] = (uint8_t)(v >> (8 * i));
}
static bool text_valid(const char *s, size_t n, bool multiline) {
  for (size_t i = 0; i < n;) {
    uint8_t a = (uint8_t)s[i++];
    if (multiline && a == '\n')
      continue;
    if (a < 32 || a == 127)
      return false;
    if (a < 128)
      continue;
    unsigned k;
    uint32_t cp, min;
    if (a >= 0xc2 && a <= 0xdf) {
      k = 1;
      cp = a & 31;
      min = 128;
    } else if (a >= 0xe0 && a <= 0xef) {
      k = 2;
      cp = a & 15;
      min = 2048;
    } else if (a >= 0xf0 && a <= 0xf4) {
      k = 3;
      cp = a & 7;
      min = 65536;
    } else
      return false;
    if (k > n - i)
      return false;
    while (k--) {
      uint8_t b = (uint8_t)s[i++];
      if ((b & 192) != 128)
        return false;
      cp = (cp << 6) | (b & 63);
    }
    if (cp < min || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
      return false;
  }
  return true;
}
bool hr_utf8_valid(const char *s, size_t n) {
  return text_valid(s, n, false);
}
void hr_edit_encode(const hr_edit_t *p, uint8_t b[HR_EDIT_HEADER]) {
  memset(b, 0, HR_EDIT_HEADER);
  memcpy(b, "HRE1", 4);
  put32(b + 4, p->revision);
  b[8] = p->id;
  b[9] = p->avatar;
  memcpy(b + 16, p->name, 49);
  memcpy(b + 65, p->intro, 85);
  memcpy(b + 150, p->title, 49);
  put32(b + 216, p->image_crc);
  put32(b + 220, hr_crc(b, 220));
}
bool hr_edit_decode(hr_edit_t *p, const uint8_t b[HR_EDIT_HEADER]) {
  if (memcmp(b, "HRE1", 4) || get32(b + 220) != hr_crc(b, 220) || b[8] < 1 ||
      b[8] > 6 || b[9] > 1 || !memchr(b + 16, 0, 49) ||
      !memchr(b + 65, 0, 85) || !memchr(b + 150, 0, 49))
    return false;
  hr_edit_t q = {.id = b[8],
                 .avatar = b[9],
                 .revision = get32(b + 4),
                 .image_crc = get32(b + 216)};
  memcpy(q.name, b + 16, 49);
  memcpy(q.intro, b + 65, 85);
  memcpy(q.title, b + 150, 49);
  if (!q.name[0] || !hr_utf8_valid(q.name, strlen(q.name)) ||
      !text_valid(q.intro, strlen(q.intro), true) ||
      !hr_utf8_valid(q.title, strlen(q.title)))
    return false;
  *p = q;
  return true;
}
unsigned hr_edit_config(hr_edit_t *p, const uint8_t *data, size_t n) {
  if (n && data[n - 1] == 0)
    n--;
  if (!n || n > 256 || memchr(data, 0, n))
    return 0x13;
  char buf[257];
  memcpy(buf, data, n);
  buf[n] = 0;
  // cJSON strings cannot represent an embedded NUL: reject its JSON escape.
  for (size_t i = 0; i < n; i++)
    if (buf[i] == '\\') {
      if (i + 5 < n && !memcmp(buf + i + 1, "u0000", 5))
        return 0x14;
      i++;
    }
  const char *end = NULL;
  cJSON *j = cJSON_ParseWithLengthOpts(buf, n + 1, &end, 1);
  if (!cJSON_IsObject(j)) {
    cJSON_Delete(j);
    return 0x13;
  }
  hr_edit_t q = *p;
  unsigned status = 1;
  bool name = false, intro = false;
  for (cJSON *v = j->child; v; v = v->next) {
    for (cJSON *w = j->child; w != v; w = w->next)
      if (!strcmp(v->string, w->string))
        status = 0x14;
    char *dst = NULL;
    size_t cap = 0;
    if (!strcmp(v->string, "nickname") || !strcmp(v->string, "name")) {
      if (name)
        status = 0x14;
      name = true;
      dst = q.name;
      cap = 49;
    } else if (!strcmp(v->string, "intro") || !strcmp(v->string, "subtitle")) {
      if (intro)
        status = 0x14;
      intro = true;
      dst = q.intro;
      cap = 85;
    }
    if (!strcmp(v->string, "title")) {
      dst = q.title;
      cap = 49;
    }
    if (dst) {
      if (!cJSON_IsString(v) || strlen(v->valuestring) >= cap ||
          !text_valid(v->valuestring, strlen(v->valuestring), dst == q.intro) ||
          (dst == q.name && !v->valuestring[0]))
        status = 0x14;
      else {
        unsigned chars = 0;
        for (const unsigned char *s = (const unsigned char *)v->valuestring; *s;
             s++)
          if ((*s & 192) != 128)
            chars++;
        if (dst == q.intro && chars > 28)
          status = 0x14;
        else
          strcpy(dst, v->valuestring);
      }
    } else if (!strcmp(v->string, "img_mode")) {
      if (!cJSON_IsString(v) || (strcmp(v->valuestring, "avatar") &&
                                 strcmp(v->valuestring, "fullscreen")))
        status = 0x14;
    } else if (!strcmp(v->string, "time")) {
      if (!cJSON_IsNumber(v) || !isfinite(v->valuedouble) ||
          v->valuedouble < 0 || v->valuedouble > 4102444800.)
        status = 0x14;
    } else if (!strcmp(v->string, "avatar_name")) {
      status = 0x1d;
    } else
      status = 0x18;
  }
  cJSON_Delete(j);
  if (status == 1)
    *p = q;
  return status;
}
unsigned hr_frame_push(hr_frame_t *f, const uint8_t *d, size_t n,
                       uint32_t now) {
  if (f->used && (uint32_t)(now - f->at) > 2000) {
    memset(f, 0, sizeof *f);
    return 0x11;
  }
  if (!n || n > sizeof f->bytes - f->used) {
    memset(f, 0, sizeof *f);
    return 0x15;
  }
  memcpy(f->bytes + f->used, d, n);
  f->used += n;
  f->at = now;
  if (f->bytes[0] != 1) {
    memset(f, 0, sizeof *f);
    return 0x10;
  }
  if (f->used < 4)
    return 0;
  f->wanted = 4 + f->bytes[2] + ((size_t)f->bytes[3] << 8);
  if (f->wanted > 260) {
    memset(f, 0, sizeof *f);
    return 0x15;
  }
  if (f->used > f->wanted) {
    memset(f, 0, sizeof *f);
    return 0x10;
  }
  return f->used == f->wanted ? 1 : 0;
}
void hr_edit_wire_encode(const hr_edit_t *p, uint8_t out[164]) {
  memset(out, 0, 164);
  out[0] = 1;
  out[1] = p->avatar;
  memcpy(out + 2, p->name, 49);
  memcpy(out + 51, p->intro, 85);
}
bool hr_edit_wire_decode(hr_edit_t *p, const uint8_t in[164]) {
  if (in[0] != 1 || in[1] > 1 || !memchr(in + 2, 0, 49) ||
      !memchr(in + 51, 0, 85))
    return false;
  hr_edit_t q = {.avatar = in[1]};
  memcpy(q.name, in + 2, 49);
  memcpy(q.intro, in + 51, 85);
  if (!q.name[0] || !hr_utf8_valid(q.name, strlen(q.name)) ||
      !text_valid(q.intro, strlen(q.intro), true))
    return false;
  *p = q;
  return true;
}
