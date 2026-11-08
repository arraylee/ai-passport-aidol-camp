#include "hr_ui.h"
#include "hr_controls.h"
#include "hr_game_scene.h"
#include "hr_friend_store.h"
#include "hr_gallery.h"
extern const lv_image_dsc_t hr_gallery_demo, hr_friend_qr_demo, hr_joy,hr_game_joy_idle;
#include "hr_people.h"
#include "hr_profiles.h"
#include "lvgl.h"
#include "src/widgets/label/lv_label_private.h"
#include <stdio.h>
#include <string.h>
LV_FONT_DECLARE(hr_font14);
LV_FONT_DECLARE(hr_font16);
LV_FONT_DECLARE(hr_font20);
LV_FONT_DECLARE(hr_font24);
LV_FONT_DECLARE(hr_font40);
LV_FONT_DECLARE(hr_font80);
static lv_obj_t *game_scene;
static lv_obj_t *title, *count, *target, *status, *help, *help2, *body, *art,
    *qr, *volume_bar, *brand, *avatar_frame, *avatar_image;
static lv_obj_t *names[8], *frames[8], *zones[6], *icons[6], *bars[5][5];
static lv_obj_t *battery_outline, *battery_fill, *battery_tip, *battery_unknown;
static const uint32_t BLUE = 0x9ed8ff, WHITE = 0xf0f5ff, MUTED = 0x91a4bb,
                      RED = 0xff5d70;
static void visible(lv_obj_t *o, bool show) {
  if (show)
    lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
  else
    lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}
static void long_mode(lv_obj_t *o, lv_label_long_mode_t mode) {
  if (lv_label_get_long_mode(o) != mode)
    lv_label_set_long_mode(o, mode);
}
static void text_font(lv_obj_t *o, const lv_font_t *font,
                      lv_style_selector_t selector) {
  if (lv_obj_get_style_text_font(o, selector) != font)
    lv_obj_set_style_text_font(o, font, selector);
}
static void text_align(lv_obj_t *o, lv_text_align_t align,
                       lv_style_selector_t selector) {
  if (lv_obj_get_style_text_align(o, selector) != align)
    lv_obj_set_style_text_align(o, align, selector);
}
static void text_color(lv_obj_t *o, lv_color_t color,
                       lv_style_selector_t selector) {
  if (!lv_color_eq(lv_obj_get_style_text_color(o, selector), color))
    lv_obj_set_style_text_color(o, color, selector);
}
static lv_obj_t *label(int x, int y, int w, const lv_font_t *font) {
  lv_obj_t *o = lv_label_create(lv_screen_active());
  lv_obj_set_pos(o, x, y);
  lv_obj_set_width(o, w);
  text_font(o, font, 0);
  text_color(o, lv_color_hex(WHITE), 0);
  long_mode(o, LV_LABEL_LONG_CLIP);
  return o;
}
static void init(void) {
  lv_obj_t *s = lv_screen_active();
  lv_obj_set_style_bg_color(s, lv_color_hex(0x080f20), 0);
  lv_obj_remove_flag(s, LV_OBJ_FLAG_SCROLLABLE);
  game_scene=hr_game_scene_create(s);
  title = label(12, 10, 180, &hr_font16);
  count = label(196, 10, 32, &hr_font16);
  brand = label(168, 14, 60, &hr_font16);
  battery_outline = lv_obj_create(s);
  battery_fill = lv_obj_create(s);
  battery_tip = lv_obj_create(s);
  battery_unknown = lv_obj_create(s);
  lv_obj_t *battery_parts[] = {battery_outline, battery_fill, battery_tip, battery_unknown};
  for (unsigned i = 0; i < 4; i++) {
    lv_obj_remove_style_all(battery_parts[i]);
    lv_obj_remove_flag(battery_parts[i], LV_OBJ_FLAG_SCROLLABLE);
  }
  lv_obj_set_pos(battery_outline, 136, 16);
  lv_obj_set_size(battery_outline, 22, 14);
  lv_obj_set_style_border_width(battery_outline, 1, 0);
  lv_obj_set_style_radius(battery_outline, 2, 0);
  lv_obj_set_pos(battery_fill, 138, 18);
  lv_obj_set_height(battery_fill, 10);
  lv_obj_set_style_bg_opa(battery_fill, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(battery_fill, 1, 0);
  lv_obj_set_pos(battery_tip, 158, 20);
  lv_obj_set_size(battery_tip, 2, 6);
  lv_obj_set_style_bg_opa(battery_tip, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(battery_tip, 1, 0);
  lv_obj_set_pos(battery_unknown, 144, 22);
  lv_obj_set_size(battery_unknown, 6, 2);
  lv_obj_set_style_bg_opa(battery_unknown, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(battery_unknown, lv_color_hex(MUTED), 0);
  target = label(12, 34, 216, &hr_font24);
  body = label(12, 213, 216, &hr_font16);
  art = lv_image_create(s);
  avatar_frame = lv_obj_create(s);
  lv_obj_remove_style_all(avatar_frame);
  lv_obj_remove_flag(avatar_frame, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(avatar_frame, 12, 56);
  lv_obj_set_size(avatar_frame, 96, 156);
  avatar_image = lv_image_create(avatar_frame);
  for (int i = 0; i < 8; i++) {
    frames[i] = lv_obj_create(s);
    lv_obj_remove_style_all(frames[i]);
    lv_obj_set_style_bg_opa(frames[i], LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(frames[i], lv_color_hex(0x152843), 0);
    lv_obj_set_style_radius(frames[i], 7, 0);
    names[i] = label(50, 64 + 36 * i, 118, &hr_font20);
    if(i<6) {
      zones[i] = label(172, 80 + 36 * i, 56, &hr_font14);
      icons[i] = lv_image_create(s);
    }
  }
  /* Status indices follow peers, while room rows include all/game/back first.
     Keep status labels above every row background, including later rows. */
  for (unsigned i = 0; i < 6; i++)
    lv_obj_move_foreground(zones[i]);
  for (int i = 0; i < 5; i++)
    for (int j = 0; j < 5; j++) {
      bars[i][j] = lv_obj_create(s);
      lv_obj_remove_style_all(bars[i][j]);
      lv_obj_set_style_bg_opa(bars[i][j], LV_OPA_COVER, 0);
    }
  volume_bar = lv_bar_create(s);
  lv_obj_set_pos(volume_bar, 18, 165);
  lv_obj_set_size(volume_bar, 204, 20);
  lv_bar_set_range(volume_bar, 0, 100);
  qr = lv_qrcode_create(s);
  lv_qrcode_set_size(qr, 168);
  lv_qrcode_set_light_color(qr, lv_color_hex(0xffffff));
  lv_qrcode_set_dark_color(qr, lv_color_hex(0));
  lv_obj_set_pos(qr, 28, 58);
  // Quiet zone is part of the white backing, not an overlay on QR modules.
  lv_obj_set_style_border_width(qr, 8, 0);
  lv_obj_set_style_border_color(qr, lv_color_hex(0xffffff), 0);
  status = label(12, 247, 216, &hr_font16);
  long_mode(status, LV_LABEL_LONG_SCROLL_CIRCULAR);
  help = label(12, 275, 216, &hr_font14);
  help2 = label(12, 297, 216, &hr_font14);
}
static bool online(const hr_view_t *v, unsigned id) {
  return hr_online(&v->core, id, v->now) && !hr_conflict(&v->core, id, v->now);
}
static const char *zone(const hr_view_t *v, unsigned id) {
  return hr_conflict(&v->core, id, v->now) ? "冲突"
         : !online(v, id)                  ? "离线"
         : v->page == HR_PAGE_RADIO && !hr_room_member(&v->core, id, v->now)
             ? "未加入"
         : v->core.peers[id].zone == 2 ? "近"
         : v->core.peers[id].zone == 1 ? "中"
                                       : "远";
}
static void text(lv_obj_t *o, const char *s) {
  visible(o, true);
  /* DOT temporarily changes the text buffer; compare its saved bytes too.
     Unchanged telemetry must not restart a long nickname's scrolling. */
  lv_label_t *l = (lv_label_t *)o;
  const char *shown = lv_label_get_text(o);
  bool equal = true;
  for (size_t i = 0;; i++) {
    char c = l->dot_begin != UINT32_MAX && i >= l->dot_begin &&
                     i - l->dot_begin < sizeof l->dot
                 ? l->dot[i - l->dot_begin]
                 : shown[i];
    if (c != s[i]) {
      equal = false;
      break;
    }
    if (!c)
      break;
  }
  if (!equal)
    lv_label_set_text(o, s);
}
static void footer(const char *a, const char *b) {
  text(help, a);
  text(help2, b);
}
static void row(unsigned n, unsigned id, int y, bool selected, bool large) {
  visible(frames[n], selected);
  lv_obj_set_pos(frames[n], 8, y - 2);
  lv_obj_set_size(frames[n], 224, 34);
  text_align(names[n], LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_set_style_radius(frames[n], 7, 0);
  lv_obj_set_style_border_width(frames[n], 2, 0);
  lv_obj_set_style_border_color(frames[n], lv_color_hex(0xEF3340), 0);
  lv_obj_set_style_bg_color(frames[n], lv_color_hex(0x351C31), 0);
  text(names[n], hr_display_name(id));
  text_align(names[n], LV_TEXT_ALIGN_LEFT, 0);
  long_mode(names[n],
            selected ? LV_LABEL_LONG_SCROLL_CIRCULAR : LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_pos(names[n], 52, y + 2);
  lv_obj_set_width(names[n], 116);
  lv_obj_set_height(names[n], large ? 28 : 24);
  text_font(names[n], large ? &hr_font24 : &hr_font20, 0);
  text_color(names[n], lv_color_hex(large ? BLUE : WHITE), 0);
  visible(icons[n], !hr_profile_remote_avatar(id) && hr_profile(id)->small);
  if (hr_profile(id)->small)
    lv_image_set_src(icons[n], hr_profile(id)->small);
  lv_obj_set_pos(icons[n], 14, y);
}
// Draw the original portrait with one uniform scale; do not use old squashed thumbnails.
static void portrait_icon(lv_obj_t *icon, const hr_profile_t *p,
                          int x, int y, unsigned width) {
  const lv_image_dsc_t *src = p->portrait ? p->portrait : p->card;
  visible(icon, src != NULL);
  if (!src) return;
  lv_image_set_src(icon, src);
  lv_image_set_pivot(icon, 0, 0);
  lv_image_set_scale(icon, width * 256 / src->header.w);
  lv_obj_set_pos(icon, x, y);
}
static void button(unsigned n,const char *label,int y,int height,bool selected) {
  visible(frames[n],true);
  lv_obj_set_pos(frames[n],12,y);lv_obj_set_size(frames[n],216,height);
  lv_obj_set_style_radius(frames[n],8,0);
  lv_obj_set_style_border_width(frames[n],selected?2:1,0);
  lv_obj_set_style_border_color(frames[n],lv_color_hex(selected?RED:0x294565),0);
  lv_obj_set_style_bg_color(frames[n],lv_color_hex(selected?0x351C31:0x112139),0);
  text(names[n],label);text_font(names[n],&hr_font20,0);
  text_align(names[n],LV_TEXT_ALIGN_LEFT,0);text_color(names[n],lv_color_hex(WHITE),0);
  lv_obj_set_pos(names[n],24,y+(height-24)/2);lv_obj_set_size(names[n],192,26);
  long_mode(names[n],selected?LV_LABEL_LONG_SCROLL_CIRCULAR:LV_LABEL_LONG_MODE_DOTS);
}
static void back_button(unsigned n,int y) {
  button(n,"返回",y,30,true);text_font(names[n],&hr_font16,0);
  lv_obj_set_pos(names[n],20,y+5);lv_obj_set_height(names[n],20);
  text_align(names[n],LV_TEXT_ALIGN_CENTER,0);
}
static void shape(lv_obj_t *o,int x,int y,int w,int h,uint32_t color,unsigned radius) {
  visible(o,true);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
  lv_obj_set_style_bg_color(o,lv_color_hex(color),0);
  lv_obj_set_style_border_width(o,0,0);lv_obj_set_style_radius(o,radius,0);
}
static void game_screen(const hr_view_t *v) {
  const hr_game_t*g=&v->game;char b[96];
  visible(title,false);
  hr_game_scene_update(game_scene,g,v->now);
  snprintf(b,sizeof b,"%lu分",(unsigned long)g->score);text(count,b);
  lv_obj_set_pos(count,138,16);lv_obj_set_size(count,80,26);
  text_align(count,LV_TEXT_ALIGN_RIGHT,0);text_font(count,&hr_font20,0);
  text_color(count,lv_color_hex(0x173B65),0);
  if(g->phase==HR_GAME_READY)snprintf(b,sizeof b,"准备");
  else if(g->phase==HR_GAME_CLEAR || g->phase==HR_GAME_DONE)snprintf(b,sizeof b,"通关！");
  else snprintf(b,sizeof b,"%u/12",g->coins_taken);
  text(target,b);text_font(target,&hr_font20,0);lv_obj_set_pos(target,20,16);
  lv_obj_set_size(target,70,26);text_color(target,lv_color_hex(0x174976),0);
  bool high_note=false;
  for(unsigned i=0;i<HR_GAME_COINS;i++) {
    int ahead=g->coins[i].x-(int)hr_game_player_x(g);
    if(!g->coins[i].taken && ahead>=-18 && ahead<140){high_note=g->coins[i].high;break;}
  }
  unsigned player_x=hr_game_player_x(g);
  text(body,g->phase==HR_GAME_READY?"哼个舒服的低音，准备出发":
       g->phase==HR_GAME_CLEAR || g->phase==HR_GAME_DONE?"到达终点！太棒啦":
       g->hit_flash?(g->rescue_kind==2?"撞到刺球，扣50分":g->rescue_kind==3?"撞到木桩，扣50分":"落水扣50分，回到木桩"):g->progress>850?"终点就在前方！":
       player_x>(unsigned)g->obstacles[0].x-60 && player_x<(unsigned)g->obstacles[0].x+27?"低音通过刺球":
       player_x>(unsigned)g->obstacles[1].x-80 && player_x<(unsigned)g->obstacles[1].x+28?"高音通过刺球":
       high_note?"升高音，拿闪亮金币":"唱歌前进 · 停声可换气");
  lv_obj_set_pos(body,12,51);lv_obj_set_size(body,216,22);text_font(body,&hr_font16,0);
  text_align(body,LV_TEXT_ALIGN_CENTER,0);text_color(body,lv_color_hex(g->hit_flash?0xFFE0C1:0xD8EAFE),0);
  if(g->phase==HR_GAME_READY) {
   snprintf(b,sizeof b,"%u",hr_game_seconds(g,v->now));text(title,b);
   lv_obj_set_pos(title,50,140);lv_obj_set_size(title,140,72);
   text_align(title,LV_TEXT_ALIGN_CENTER,0);text_font(title,&hr_font80,0);
   text_color(title,lv_color_hex(0xFFE5A0),0);
  }
  if(g->reward_at && (uint32_t)(v->now-g->reward_at)<600 && !g->hit_flash) {
   snprintf(b,sizeof b,"+%u",g->reward_points);text(status,b);
   lv_obj_set_pos(status,96,120-(v->now-g->reward_at)/40);lv_obj_set_width(status,130);text_font(status,&hr_font24,0);
   text_color(status,lv_color_hex(0xFFE5A0),0);text_align(status,LV_TEXT_ALIGN_RIGHT,0);
  }
  if(g->combo>=3 && !g->hit_flash) {
   snprintf(b,sizeof b,"连击 %u",g->combo);text(title,b);lv_obj_set_pos(title,140,86);lv_obj_set_width(title,88);text_align(title,LV_TEXT_ALIGN_RIGHT,0);
   text_font(title,&hr_font16,0);text_color(title,lv_color_hex(0xFFE5A0),0);
  }
  text(help,"下键返回");text_font(help,&hr_font14,0);lv_obj_set_pos(help,60,297);
  text_align(help,LV_TEXT_ALIGN_CENTER,0);lv_obj_set_width(help,120);text_color(help,lv_color_hex(0x183956),0);
}
static void friend_card(const hr_view_t *v) {
  const hr_profile_t *p = hr_friend_profile(v->card_id);
  if (!p) p = hr_profile(v->card_id);
  text(title, v->card_from_pair ? "已成为好友" : "好友名片");
  portrait_icon(icons[5], p, 12, 48, 96);
  text(target, p->name);
  lv_obj_set_pos(target, 116, 55);
  lv_obj_set_width(target, 112);
  text_font(target, &hr_font20, 0);
  text_align(target, LV_TEXT_ALIGN_CENTER, 0);
  /* Reserve a static bio area below the complete portrait. Text is smaller
   * than the name; LVGL wraps by actual glyph width, never a scrolling line. */
  const lv_font_t *font = &hr_font16;
  lv_point_t signature_size;
  lv_text_get_size(&signature_size, p->signature, font, 0, 4, 216, LV_TEXT_FLAG_NONE);
  if (signature_size.y > 70) {
    font = &hr_font14;
    lv_text_get_size(&signature_size, p->signature, font, 0, 4, 216, LV_TEXT_FLAG_NONE);
  }
  int height = signature_size.y < 70 ? signature_size.y : 70;
  text_font(body, font, 0);
  lv_obj_set_style_text_line_space(body, 4, 0);
  lv_obj_set_size(body, 216, height);
  long_mode(body, signature_size.y > 70 ? LV_LABEL_LONG_MODE_DOTS : LV_LABEL_LONG_WRAP);
  text(body, p->signature);
  text_align(body, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_set_pos(body, 12, 210 + (70 - height) / 2);
  visible(art, true);
  lv_image_set_src(art, &hr_friend_qr_demo);
  lv_obj_set_pos(art, 136, 96);
  text(status, "示例二维码");
  lv_obj_set_pos(status, 116, 180);
  lv_obj_set_width(status, 112);
  text_align(status, LV_TEXT_ALIGN_CENTER, 0);
  back_button(4,286);
}
static void home(const hr_view_t *v) {
  const hr_profile_t *p = hr_profile(v->core.id);
  text(title, "Aidol · LIVE");
  lv_obj_set_pos(title, 12, 14);
  lv_obj_set_width(title, 116);
  text_font(title, &hr_font16, 0);
#ifdef HR_SLEEP_DIAGNOSTIC
  text(brand, "TEST 6D10");
#else
  text(brand, "FoloToy");
#endif
  text_align(brand, LV_TEXT_ALIGN_RIGHT, 0);
  /* Aidol's continuous SOC fill; zero and unavailable have distinct colors. */
  int pct = v->battery_percent;
  bool known = pct >= 0 && pct <= 100;
  lv_color_t color = lv_color_hex(!known ? MUTED : pct < 20 ? 0xE0463A :
                                pct < 40 ? 0xF2C14E : WHITE);
  visible(battery_outline, true);
  visible(battery_tip, true);
  visible(battery_fill, known && pct > 0);
  visible(battery_unknown, !known);
  lv_obj_set_style_border_color(battery_outline, color, 0);
  lv_obj_set_style_bg_color(battery_tip, color, 0);
  lv_obj_set_style_bg_color(battery_fill, color, 0);
  if (known && pct > 0)
    lv_obj_set_width(battery_fill, (18 * pct + 99) / 100);
  if (v->phone_connected)
    text(help, "小程序已连接");
  lv_obj_set_pos(help, 12, 216);
  lv_obj_set_width(help, 216);
  if (v->core.speaker) {
    char b[80];
    snprintf(b, sizeof b,
             (v->core.floor_target == 0 || v->core.floor_target == v->core.id)
                 ? (v->receiving ? "收听：%s" : "等语音：%s")
                 : "忙碌：%s",
             hr_display_name(v->core.speaker));
    lv_obj_set_pos(help, 12, 210);
    lv_obj_set_width(help, 216);
    text(help, b);
    long_mode(help, LV_LABEL_LONG_SCROLL_CIRCULAR);
  }
  for (int i = 0; i < 2; i++) {
    visible(bars[0][i], true);
    lv_obj_set_pos(bars[0][i], 12, 42);
    lv_obj_set_size(bars[0][i], i ? 28 : 216, i ? 2 : 1);
    lv_obj_set_style_bg_color(bars[0][i], lv_color_hex(i ? 0xEF3340 : 0x304D72),
                              0);
  }
  visible(avatar_frame, p->portrait != NULL);
  if (p->portrait) {
    lv_image_set_src(avatar_image, p->portrait);
    /* Show the entire original miniapp 96x156 canvas without cropping. */
    lv_obj_set_pos(avatar_image, (96 - (int)p->portrait->header.w) / 2,
                    (156 - (int)p->portrait->header.h) / 2);
  }
  text(target, p->name);
  text_font(target, &hr_font20, 0);
  lv_obj_set_width(target, 108);
  text_align(target, LV_TEXT_ALIGN_LEFT, 0);
  long_mode(target, LV_LABEL_LONG_SCROLL_CIRCULAR);
  char signature[128];
  unsigned n = 0, out = 0, wrap_after = 6;
  const unsigned char *c = (const unsigned char *)p->signature;
  while (*c && out < 118) {
    unsigned bytes = *c < 128 ? 1 : (*c & 0xe0) == 0xc0 ? 2 :
                     (*c & 0xf0) == 0xe0 ? 3 : 4;
    /* Respect explicit line breaks and trim whitespace at line edges. */
    bool space = *c == ' ' || *c == '\t' || *c == '\r' || *c == '\n' ||
                 (bytes == 3 && !memcmp(c, "　", 3));
    if (space) {
      if (n && out && signature[out - 1] != '\n') {
        signature[out++] = '\n';
        n = 0;
      }
      c += bytes;
      continue;
    }
    if (!n) {
      /* Balance each phrase so a long signature does not end in a tiny tail. */
      unsigned length = 0;
      const unsigned char *next = c;
      while (*next) {
        unsigned k = *next < 128 ? 1 : (*next & 0xe0) == 0xc0 ? 2 :
                     (*next & 0xf0) == 0xe0 ? 3 : 4;
        if (*next == ' ' || *next == '\t' || *next == '\r' || *next == '\n' ||
            (k == 3 && !memcmp(next, "　", 3))) break;
        length++;
        bool end = *next == ',' || (k == 3 &&
                    (!memcmp(next, "，", 3) || !memcmp(next, "。", 3)));
        next += k;
        if (end) break;
      }
      /* Keep a short Latin phrase intact when the real 16px glyphs fit. */
      char phrase[128];
      size_t phrase_bytes = (size_t)(next - c);
      lv_point_t phrase_size = {0};
      if (phrase_bytes < sizeof phrase) {
        memcpy(phrase, c, phrase_bytes);
        phrase[phrase_bytes] = 0;
        lv_text_get_size(&phrase_size, phrase, &hr_font16, 0, 6,
                         LV_COORD_MAX, LV_TEXT_FLAG_NONE);
      }
      unsigned lines = phrase_bytes < sizeof phrase && phrase_size.x <= 104
                         ? 1 : (length + 5) / 6;
      wrap_after = lines ? (length + lines - 1) / lines : 6;
    }
    if (n >= wrap_after) {
      while (out && signature[out - 1] == ' ') out--;
      signature[out++] = '\n';
      n = 0;
    }
    bool pause = *c == ',' || (bytes == 3 &&
                  (!memcmp(c, "，", 3) || !memcmp(c, "。", 3)));
    memcpy(signature + out, c, bytes);
    out += bytes;
    c += bytes;
    n++;
    if (pause && *c) {
      signature[out++] = '\n';
      n = 0;
    }
  }
  while (out && (signature[out - 1] == '\n' || signature[out - 1] == ' ')) out--;
  signature[out] = 0;
  unsigned lines = 1;
  for (unsigned i=0; signature[i]; i++) if (signature[i]=='\n') lines++;
  const int line_space = 6;
  unsigned signature_height = lines * hr_font16.line_height +
                              (lines - 1) * line_space;
  if (signature_height > 104) signature_height = 104;
  int group_y = 56 + (156 - hr_font20.line_height - 18 - (int)signature_height) / 2;
  lv_obj_set_pos(target, 124, group_y);
  text(body, signature);
  text_font(body, &hr_font16, 0);
  lv_obj_set_pos(body, 124, group_y + hr_font20.line_height + 18);
  lv_obj_set_style_text_line_space(body, line_space, 0);
  lv_obj_set_size(body, 104, signature_height);
  long_mode(body, LV_LABEL_LONG_SCROLL_CIRCULAR);
  text_align(body, LV_TEXT_ALIGN_LEFT, 0);
  text_color(body, lv_color_hex(0xB0BED2), 0);
  for (int i = 0; i < 3; i++) {
    bool focus = v->home_selection == i;
    visible(frames[i], true);
    lv_obj_set_pos(frames[i], 12 + i * 74, 236);
    lv_obj_set_size(frames[i], 68, 50);
    lv_obj_set_style_radius(frames[i], 7, 0);
    lv_obj_set_style_bg_color(frames[i],
                              lv_color_hex(focus ? 0x351C31 : 0x14223A), 0);
    lv_obj_set_style_border_width(frames[i], focus ? 2 : 1, 0);
    lv_obj_set_style_border_color(frames[i],
                                  lv_color_hex(focus ? 0xEF3340 : 0x304D72), 0);
    text(names[i], i == 2 ? "相册" : i ? "组队" : "好友");
    lv_obj_set_pos(names[i], 16 + i * 74, 251);
    lv_obj_set_width(names[i], 60);
    text_font(names[i], &hr_font20, 0);
    lv_obj_set_height(names[i], 24);
    text_color(names[i], lv_color_hex(0xF2F5FF), 0);
    text_align(names[i], LV_TEXT_ALIGN_CENTER, 0);
    long_mode(names[i], LV_LABEL_LONG_CLIP);
  }
  text(help2, "上／中选择 · 下键进入");
  lv_obj_set_pos(help2, 12, 294);
  text_align(help2, LV_TEXT_ALIGN_CENTER, 0);
}
static void radio_status(const hr_view_t *v) {
  const hr_core_t *c = &v->core;
  const char *s = "按住上键开始说话";
  char b[80];
  uint32_t color = MUTED;
  if (!v->radio_ok)
    s = "无线异常，请重启";
  else if (v->phone_connected)
    s = "小程序已连接，语音暂停";
  else if (!v->audio_ok)
    s = "音频异常，请重启";
  else if (!v->buttons_ok)
    s = "按键异常，请重启";
  else if (hr_conflict(c, c->id, v->now))
    s = c->identity_mode ? "SN重复，请检查身份" : "编号重复，回首页长按中键";
  else if (hr_conflict(c, c->host, v->now))
    s = c->identity_mode ? "SN重复，请检查身份" : "1号编号重复，语音暂停";
  else if (!hr_room_member(c, c->host, v->now))
    s = c->identity_mode ? "等待队友加入队伍" : "等待1号加入队伍";
  else if (v->selection && !hr_room_member(c, v->selection, v->now)) {
    s = "对方离线，松手后短按换人";
    color = RED;
  } else if (c->held && c->blocked) {
    s = "忙碌或超时，松手重试";
    color = RED;
  } else if (hr_can_talk(c, v->now)) {
    s = "正在说话，松手结束";
    color = 0x50dfb4;
  } else if (c->speaker) {
    snprintf(b, sizeof b,
             (c->floor_target == 0 || c->floor_target == c->id)
                 ? (v->receiving ? "收听：%s" : "等语音：%s")
                 : "忙碌：%s",
             hr_display_name(c->speaker));
    s = b;
    color = BLUE;
  } else if (c->held)
    s = "请求发言，请稍候";
  else if (v->up_pressed) {
    s = "继续按住，即将说话";
    color = BLUE;
  } else if (v->selection && !hr_room_member(c, v->selection, v->now))
    s = "对方离线，换人或选全体";
  text(status, s);
  text_color(status, lv_color_hex(color), 0);
}
void hr_ui_render(const hr_view_t *v) {
  if (!title)
    init();
  visible(avatar_frame, false);
  visible(game_scene,false);
  char b[128];
  const hr_core_t *c = &v->core;
  lv_obj_t *items[] = {title, count, target, status, help,
                       help2, body,  art,    qr,     volume_bar, brand,
                       battery_outline, battery_fill, battery_tip, battery_unknown};
  for (unsigned i = 0; i < sizeof items / sizeof items[0]; i++)
    visible(items[i], false);
  for (int i = 0; i < 8; i++) {
    visible(names[i], false);visible(frames[i], false);
    if(i<6) {
      visible(zones[i], false);lv_obj_set_width(zones[i], 56);
      long_mode(zones[i], LV_LABEL_LONG_CLIP);visible(icons[i], false);
    }
  }
  for (int i = 0; i < 5; i++)
    for (int j = 0; j < 5; j++)
      visible(bars[i][j], false);
  static int previous_layout = -1;
  int layout = v->intro_kind ? 200 + v->intro_kind : v->activation_required ? 102 : v->setup ? 100 : v->volume_mode ? 101 : v->page;
  if (layout != previous_layout) {
    previous_layout = layout;
    lv_image_set_scale(art, 256);
    lv_image_set_pivot(art, 0, 0);
    for (unsigned i=0; i<8; i++) {
      text_align(names[i],LV_TEXT_ALIGN_LEFT,0);text_color(names[i],lv_color_hex(WHITE),0);
      lv_obj_set_style_radius(frames[i],7,0);
      if(i<6) {
        lv_image_set_scale(icons[i],256);lv_image_set_pivot(icons[i],0,0);
        lv_obj_set_height(zones[i],LV_SIZE_CONTENT);
        text_font(zones[i],&hr_font14,0);text_align(zones[i],LV_TEXT_ALIGN_LEFT,0);
      }
      for(unsigned j=0;j<5 && i<5;j++){lv_obj_set_style_border_width(bars[i][j],0,0);lv_obj_set_style_radius(bars[i][j],0,0);}
    }
    lv_obj_set_pos(title, 12, 10);
    lv_obj_set_width(title, 180);
    lv_obj_set_height(title, LV_SIZE_CONTENT);
    text_font(title, &hr_font16, 0);
    text_color(title,lv_color_hex(WHITE),0);
    text_font(count,&hr_font16,0);text_color(count,lv_color_hex(WHITE),0);
    lv_obj_set_size(count,32,20);lv_obj_set_pos(count,196,10);
    lv_obj_set_width(target, 216);
    lv_obj_set_height(target,LV_SIZE_CONTENT);
    lv_obj_set_width(body, 216);
    text_font(body, &hr_font16, 0);
    text_color(body, lv_color_hex(WHITE), 0);
    lv_obj_set_style_text_line_space(body, 0, 0);
    lv_obj_set_height(body, LV_SIZE_CONTENT);
    long_mode(body, LV_LABEL_LONG_CLIP);
    lv_obj_set_pos(help, 12, 275);
    lv_obj_set_width(help, 216);
    lv_obj_set_pos(help2, 12, 297);
    text_align(help2, LV_TEXT_ALIGN_LEFT, 0);
    text_align(title, LV_TEXT_ALIGN_LEFT, 0);
    text_align(help, LV_TEXT_ALIGN_LEFT, 0);
    long_mode(title, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_pos(target, 12, 34);
    long_mode(target, LV_LABEL_LONG_SCROLL_CIRCULAR);
    text_align(target, LV_TEXT_ALIGN_LEFT, 0);
    text_font(target, &hr_font24, 0);
    text_color(target,lv_color_hex(WHITE),0);
    text_color(status, lv_color_hex(MUTED), 0);
    text_font(status,&hr_font16,0);text_align(status,LV_TEXT_ALIGN_LEFT,0);
    text_color(help,lv_color_hex(WHITE),0);
    text_align(status, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_pos(status, 12, 247);
    lv_obj_set_width(status, 216);
    lv_obj_set_height(volume_bar, 20);
    text_align(body, LV_TEXT_ALIGN_LEFT, 0);
  }
  if (v->intro_kind) {
    bool welcome = v->intro_kind == HR_INTRO_WELCOME;
    bool sleeping = v->intro_kind == HR_INTRO_SLEEP;
    text(title, "Aidol · FoloToy");
    text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(title, 216);
    text(target, welcome ? "欢迎加入" : sleeping ? "待会儿见" : "进入创造营");
    lv_obj_set_pos(target, 12, 58);
    text_align(target, LV_TEXT_ALIGN_CENTER, 0);
    visible(art, true);
    lv_image_set_src(art, &hr_joy);
    lv_obj_set_pos(art, 80, 108 + ((v->intro_elapsed/150)%2 ? 0 : 2));
    text(body, welcome ? "Aidol 创造营" : sleeping ? "按上键唤醒我" : "用 AI 让想法发生");
    lv_obj_set_pos(body, 12, 210);
    text_font(body, &hr_font20, 0);
    text_align(body, LV_TEXT_ALIGN_CENTER, 0);
    if (welcome) {
      text(help, "让我们一起破格创造！");
      text_align(help, LV_TEXT_ALIGN_CENTER, 0);
    } else {
      for (unsigned i=0; i<2; i++) {
        visible(bars[0][i], true);
        lv_obj_set_pos(bars[0][i], 30, 253);
        unsigned duration = sleeping ? 1200 : HR_BOOT_MS;
        unsigned progress = v->intro_elapsed > duration ? duration : v->intro_elapsed;
        lv_obj_set_size(bars[0][i], i ? 1+179*progress/duration : 180, 6);
        lv_obj_set_style_bg_color(bars[0][i], lv_color_hex(i ? RED : 0x304D72), 0);
      }
    }
    return;
  }
  if (v->activation_required) {
    text(title, "Aidol · FoloToy");
    lv_obj_set_width(title, 216);
    text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    text(target, "待激活");
    lv_obj_set_pos(target, 12, 66);
    text_align(target, LV_TEXT_ALIGN_CENTER, 0);
    visible(art, true);
    lv_image_set_src(art, &hr_joy);
    lv_obj_set_pos(art, 80, 116);
    text(body, v->save_failed ? "保存失败，请重试" : "准备好就出发");
    text_font(body, &hr_font20, 0);
    lv_obj_set_pos(body, 12, 216);
    text_align(body, LV_TEXT_ALIGN_CENTER, 0);
    footer("短按下键，开启创造营", "未启用时不会弹出交友");
    text_align(help, LV_TEXT_ALIGN_CENTER, 0);
    text_align(help2, LV_TEXT_ALIGN_CENTER, 0);
    return;
  }
  if (!v->setup && !v->volume_mode &&
      (v->page == HR_PAGE_HOME || v->page == HR_PAGE_MENU)) {
    home(v);
    return;
  }
  if (v->page != HR_PAGE_RADIO)
    text(title, "AIDOL · LIVE");
  if (v->setup) {
    text(title, "人物资料");
    visible(count, false);
    snprintf(b, sizeof b, "%s", hr_display_name(v->choice));
    text(target, b);
    text_font(target, &hr_font20, 0);
    lv_obj_set_pos(target, 12, 100);
    text_align(target, LV_TEXT_ALIGN_CENTER, 0);
    text(body, "昵称头像在小程序修改\n不同胸卡可用相同资料");
    lv_obj_set_pos(body, 12, 155);
    text_align(body, LV_TEXT_ALIGN_CENTER, 0);
    text(status, v->save_failed ? "保存失败，请重试" : "仅更换本机资料");
    footer("上／中选人物，下键保存", "长按下键返回");
    return;
  }
  if (v->volume_mode) {
    text(target, "播放音量");
    snprintf(b, sizeof b, "%u", v->volume);
    text(body, b);
    lv_obj_set_pos(body, 12, 120);
    text_align(body, LV_TEXT_ALIGN_CENTER, 0);
    visible(volume_bar, true);
    lv_obj_set_pos(volume_bar, 18, 165);
    lv_bar_set_value(volume_bar, v->volume, LV_ANIM_OFF);
    text(status, v->save_failed ? "音量未保存，请重试" : "仅调整本机音量");
    footer("上键加，中键减", "下键返回组队");
    return;
  }
  switch (v->page) {
  case HR_PAGE_HOME:
  case HR_PAGE_MENU:
    home(v);
    return;
  case HR_PAGE_CARD:
    friend_card(v);
    break;
  case HR_PAGE_QR: {
    const char *url = hr_profile(c->id)->url;
    text(target, hr_display_name(c->id));
    if (!*url) {
      text(status, "个人主页未设置");
      footer("昵称已更新，公开链接不沿用", "长按下键返回首页");
      break;
    }
    static unsigned qr_id = 0;
    if (qr_id != c->id) {
      lv_qrcode_update(qr, url, strlen(url));
      qr_id = c->id;
    }
    visible(qr, true);
    text(status, c->id <= 3 ? "微博公开主页" : "B 站公开主页");
    footer("扫码查看公开主页", "长按下键返回首页");
    break;
  }
  case HR_PAGE_LOBBY: {
    text(title, "组队");
    text(target, v->room_joining ? "正在加入" : "一起加入");
    lv_obj_set_pos(target, 12, 58);
    text_font(target, &hr_font24, 0);
    text_align(target, LV_TEXT_ALIGN_CENTER, 0);
    text(body, "【Aidol 直播房间】");
    lv_obj_set_pos(body, 12, 97);
    text_align(body, LV_TEXT_ALIGN_CENTER, 0);
    text_font(body, &hr_font20, 0);
    visible(art, true);
    lv_image_set_src(art, &hr_joy);
    unsigned elapsed = v->room_joining ? v->now-v->room_join_started : 0;
    lv_obj_set_pos(art, 80, 137 + (v->room_joining && (elapsed/150)%2 ? 0 : 2));
    if (v->room_joining) {
      // Six small connection points converge without full-screen layers.
      static const int dx[6] = {-88,-64,64,88,64,-64};
      static const int dy[6] = {0,-48,-48,0,48,48};
      unsigned phase = elapsed > HR_ROOM_JOIN_MS ? HR_ROOM_JOIN_MS : elapsed;
      for (unsigned i=0;i<6;i++) {
        visible(frames[i], true);
        lv_obj_set_style_radius(frames[i], 4, 0);
        lv_obj_set_style_border_width(frames[i], 0, 0);
        lv_obj_set_style_bg_color(frames[i], lv_color_hex(i%2 ? RED : BLUE), 0);
        lv_obj_set_pos(frames[i], 116+dx[i]*(int)(3600-phase)/3600,
                       173+dy[i]*(int)(3600-phase)/3600);
        lv_obj_set_size(frames[i], 8, 8);
      }
      for (unsigned i=0;i<2;i++) {
        visible(bars[0][i], true);
        lv_obj_set_pos(bars[0][i], 42, 240);
        lv_obj_set_size(bars[0][i], i ? 1+155*phase/HR_ROOM_JOIN_MS : 156, 4);
        lv_obj_set_style_bg_color(bars[0][i], lv_color_hex(i ? RED : 0x304D72), 0);
      }
    }
    if (v->phone_connected) text(status, "请先断开小程序");
    footer(v->room_joining ? "正在组队…" : "请先断开小程序", "长按下键返回首页");
    break;
  }
  case HR_PAGE_RADIO: {
    text(title,"直播房间");text_font(title,&hr_font20,0);lv_obj_set_width(title,130);
    uint8_t choices[9];unsigned total=hr_room_choices(v,v->now,choices);
    unsigned peers=total>2?total-3:0;
    snprintf(b,sizeof b,"%u位队友",peers);text(count,b);
    lv_obj_set_pos(count,142,13);lv_obj_set_size(count,86,20);text_align(count,LV_TEXT_ALIGN_RIGHT,0);
    /* All five peers plus all/game/back fit without hidden pages. */
    unsigned height=total<=4?44:total<=6?32:25,signal_row=0;
    for(unsigned n=0;n<total;n++) {
      unsigned id=choices[n];int y=42+(height+2)*n;
      const char *name=id==HR_ROOM_GAME?"玩游戏":id==HR_ROOM_BACK?"返回":id?hr_display_name(id):"和全体队友对讲";
      button(n,name,y,height,id==v->selection);
      lv_obj_set_pos(names[n],24,y+(height-20)/2);lv_obj_set_size(names[n],192,22);
      if(id>=1 && id<=HR_PLAYERS) {
        text_align(names[n],LV_TEXT_ALIGN_LEFT,0);lv_obj_set_pos(names[n],22,y+(height-20)/2);
        lv_obj_set_size(names[n],126,22);text_font(names[n],&hr_font20,0);
        int r=c->peers[id].signal_q8/256;
        int level=r>=-50?5:r>=-60?4:r>=-70?3:r>=-80?2:1;
        if(hr_game_peer_busy(c,id)) {
          text(zones[signal_row],"游戏中");lv_obj_set_pos(zones[signal_row],172,y+(height-16)/2);lv_obj_set_width(zones[signal_row],50);
        } else {
          for(unsigned j=0;j<5;j++) {
            unsigned h=4+j*3;
            shape(bars[signal_row][j],176+j*9,y+(height+16)/2-h,6,h,j<(unsigned)level?BLUE:0x304257,1);
          }
          text(zones[signal_row],zone(v,id));lv_obj_set_pos(zones[signal_row],151,y+(height-16)/2);lv_obj_set_size(zones[signal_row],20,18);
        }
        signal_row++;
      }
    }
    lv_obj_set_pos(status,12,260);lv_obj_set_pos(help,12,282);lv_obj_set_pos(help2,12,300);
    if(total==2){text(status,"还没有队友，可以先玩游戏");text_align(status,LV_TEXT_ALIGN_CENTER,0);}
    else if(v->selection<=HR_PLAYERS)radio_status(v);
    else {text(status,v->selection==HR_ROOM_GAME?"一起挑战，看看谁分数高":"返回首页并离开房间");text_align(status,LV_TEXT_ALIGN_CENTER,0);}
    footer("上／中选择  下键确认", "选队友长按上说话");
    break;
  }
  case HR_PAGE_PAIR: {
    const hr_social_t *s = &c->social;
    text(title, c->social.manual ? "寻找好友 · 确认" : "碰一碰 · 交朋友");
    visible(count, false);
    if (s->candidate) {
      text(target, hr_display_name(s->candidate));
      lv_obj_set_pos(target, 12, 182);
      text_font(target, &hr_font20, 0);
      text_align(target, LV_TEXT_ALIGN_CENTER, 0);
      const hr_profile_t *own = hr_profile(c->id);
      const hr_profile_t *other = hr_friend_profile(s->candidate);
      if (!other)
        other = hr_profile(s->candidate);
      portrait_icon(icons[5], own, 20, 53, 72);
      const lv_image_dsc_t *preview_image = v->pair_preview;
      const lv_image_dsc_t *saved_image = other->portrait;
      bool have_card = preview_image || (saved_image &&
          (hr_friend_profile(s->candidate) || !hr_profile_remote_avatar(s->candidate)));
      visible(art, have_card);
      if (have_card) {
        const lv_image_dsc_t *src = preview_image ? preview_image : saved_image;
        lv_image_set_src(art, src);
        lv_image_set_pivot(art, 0, 0);
        lv_image_set_scale(art, 72 * 256 / src->header.w);
        lv_obj_set_pos(art, 148, 53);
      }
      if (!have_card) {
        visible(frames[5], true);
        lv_obj_set_style_bg_color(frames[5], lv_color_hex(0x152843), 0);
        lv_obj_set_pos(frames[5], 150, 62);
        lv_obj_set_size(frames[5], 72, 104);
        lv_obj_set_style_border_width(frames[5], 1, 0);
        lv_obj_set_style_border_color(frames[5], lv_color_hex(BLUE), 0);
        text(zones[5], "头像");
        lv_obj_set_pos(zones[5], 169, 104);
        lv_obj_set_width(zones[5], 44);
      }
    } else {
      lv_obj_set_pos(target, 12, 34);
      text_align(target, LV_TEXT_ALIGN_LEFT, 0);
      text(target, s->locked_candidate ? hr_display_name(s->locked_candidate) : "靠近一位朋友");
      text(body, s->locked_candidate ? "请重新靠近对方" : "两台靠近，自动发现");
      lv_obj_set_pos(body, 12, 150);
    }
    if (s->pending && !s->saved) {
      visible(volume_bar, true);
      lv_obj_set_pos(volume_bar, 18, 231);
      lv_obj_set_height(volume_bar, 8);
      lv_bar_set_value(volume_bar, v->card_progress, LV_ANIM_OFF);
    }
    if (!s->complete) {
      for (unsigned i = 0; i < 3; i++) {
        visible(bars[0][i], true);
        lv_obj_set_pos(bars[0][i], 114 + i * 10,
                       108 + ((v->now / 180 + i) % 3 == 0 ? 0 : 5));
        lv_obj_set_size(bars[0][i], 7, 7);
        lv_obj_set_style_bg_color(bars[0][i], lv_color_hex(BLUE), 0);
      }
    }
    text(status, s->failed      ? "名片交换失败"
                 : s->complete  ? "已成为好友"
                 : s->saved     ? "本机已存，等待对方"
                 : s->pending   ? "正在交换名片，请稍候"
                 : s->accepted  ? "请双方保持在此页面"
                 : s->candidate ? "双方确认后成为好友"
                                : "正在寻找附近设备");
    lv_obj_set_pos(status, 12, 211);
    text_align(status, LV_TEXT_ALIGN_CENTER, 0);
    visible(frames[4], true);
    lv_obj_set_pos(frames[4], 12, 246);
    lv_obj_set_size(frames[4], 216, 40);
    lv_obj_set_style_border_width(frames[4], 2, 0);
    lv_obj_set_style_border_color(frames[4], lv_color_hex(RED), 0);
    lv_obj_set_style_bg_color(frames[4], lv_color_hex(0x351C31), 0);
    text(names[4], s->failed ? "下键重试" : s->complete ? "名片已保存"
                   : s->pending ? "正在交换名片"
                   : s->saved || s->accepted ? "等待对方确认"
                   : s->candidate ? "下键确认添加好友" : "等待朋友靠近");
    lv_obj_set_pos(names[4], 20, 255);
    lv_obj_set_size(names[4], 200, 26);
    text_font(names[4], &hr_font20, 0);
    text_color(names[4], lv_color_hex(WHITE), 0);
    text_align(names[4], LV_TEXT_ALIGN_CENTER, 0);
    long_mode(names[4], LV_LABEL_LONG_CLIP);
    footer("", s->complete ? "长按下键返回" : "长按下键取消");
    break;
  }
  case HR_PAGE_FRIENDS: {
    text(title,"好友");uint8_t choices[7];unsigned total=0;
    bool searching=c->social.active && c->social.manual;
    if(searching) {
      text(count,"寻找中…");text_font(count,&hr_font14,0);
      lv_obj_set_pos(count,128,12);lv_obj_set_size(count,100,20);
      text_align(count,LV_TEXT_ALIGN_RIGHT,0);text_color(count,lv_color_hex(BLUE),0);
    }
    for(unsigned id=1;id<=HR_PLAYERS;id++)if(c->social.friends&(1u<<id))choices[total++]=id;
    choices[total++]=0;unsigned selected=0;
    for(unsigned i=0;i<total;i++)if(choices[i]==v->friend_selection)selected=i;
    unsigned first=selected/4*4;
    for(unsigned i=first;i<total && i<first+4;i++) {
      unsigned n=i-first,id=choices[i];int y=50+54*n;
      if(!id){button(n,"返回",total==1?220:y,44,!v->friend_selection);continue;}
      row(n,id,y,id==v->friend_selection,false);lv_obj_set_height(frames[n],50);
      lv_obj_set_pos(names[n],52,y+4);lv_obj_set_height(names[n],26);
      const hr_profile_t *fp=hr_friend_profile(id);if(!fp)fp=hr_profile(id);
      text(names[n],fp->name);portrait_icon(icons[n],fp,18,y+2,24);lv_obj_set_width(names[n],170);
      text(zones[n],fp->signature);long_mode(zones[n],LV_LABEL_LONG_MODE_DOTS);
      lv_obj_set_size(zones[n],170,18);lv_obj_set_pos(zones[n],52,y+29);
    }
    if(total==1){text(target,searching?"正在寻找好友":"还没有好友");lv_obj_set_pos(target,12,90);text(body,searching?"双方打开好友并靠近":"重新进入好友开始寻找");lv_obj_set_pos(body,12,135);}
    footer("上／中选择，下键确认", searching?"请对方也打开好友":"");break;
  }
  case HR_PAGE_GALLERY: {
    const lv_image_dsc_t *photo = hr_gallery_image_descriptor();
    visible(count, false);
    if (photo) {
      visible(title, false);
      visible(art, true);
      lv_image_set_src(art, photo);
      lv_obj_set_pos(art, (240 - photo->header.w) / 2,
                     (320 - photo->header.h) / 2);
    } else {
      visible(title, false);
      visible(art, true);
      lv_image_set_src(art, &hr_gallery_demo);
      lv_obj_set_pos(art, 0, 0);
    }
    if(v->gallery_return) {
      lv_obj_move_foreground(frames[4]);lv_obj_move_foreground(names[4]);
      back_button(4,266);text(help2,"下键返回，上／中继续看");
      lv_obj_set_pos(help2,12,299);lv_obj_set_height(help2,18);
    }
    break;
  }
  case HR_PAGE_GAME_MENU:
    text(title,"唱跳挑战");text(target,"高低音挑战");lv_obj_set_pos(target,12,46);
    text_font(target,&hr_font20,0);
    button(0,"开始游戏",94,42,v->game_selection==0);
    button(1,"排行榜",148,42,v->game_selection==1);
    button(2,"返回",202,42,v->game_selection==2);
    footer("唱歌才前进，闪亮金币100分", "上／中选择  下键确认");break;
  case HR_PAGE_GAME_PLAY:
    game_screen(v);break;
  case HR_PAGE_GAME_RESULT:
    text(title,"通关成功！");text_font(title,&hr_font20,0);lv_obj_set_width(title,216);
    text_align(title,LV_TEXT_ALIGN_CENTER,0);text_color(title,lv_color_hex(0xFFD66B),0);
    visible(art,true);lv_image_set_src(art,&hr_game_joy_idle);lv_obj_set_pos(art,92,44);
    snprintf(b,sizeof b,"%lu",(unsigned long)v->game.score);
    text(target,b);text_font(target,&hr_font40,0);lv_obj_set_pos(target,12,108);
    text_align(target,LV_TEXT_ALIGN_CENTER,0);
    snprintf(b,sizeof b,"最高分 %lu",(unsigned long)c->best_score);text(body,b);
    lv_obj_set_pos(body,12,168);text_align(body,LV_TEXT_ALIGN_CENTER,0);
    text(status,v->best_dirty?"最高分保存中…":v->new_best?"新纪录！":"再来一次，挑战自己");
    lv_obj_set_pos(status,12,195);text_align(status,LV_TEXT_ALIGN_CENTER,0);
    button(0,"再玩一次",218,28,v->result_selection==0);
    button(1,"排行榜",250,28,v->result_selection==1);
    button(2,"返回房间",282,28,v->result_selection==2);
    break;
  case HR_PAGE_RANK: {
    text(title,"房间排行榜");uint8_t ids[HR_PLAYERS];unsigned n=hr_ranked(c,v->now,ids),rank=1,previous=0;
    if(!n){text(target,"排行榜为空");lv_obj_set_pos(target,12,100);text_align(target,LV_TEXT_ALIGN_CENTER,0);text(body,"玩一局，留下你的成绩");lv_obj_set_pos(body,12,149);text_align(body,LV_TEXT_ALIGN_CENTER,0);}
    for(unsigned i=0;i<n;i++) {
      unsigned id=ids[i],value=id==c->id?c->best_score:c->peers[id].best_score;
      if(i && value!=previous){rank=i+1;}
      previous=value;
      snprintf(b,sizeof b,"%u. %s",rank,hr_display_name(id));text(names[i],b);
      lv_obj_set_pos(names[i],12,48+30*i);lv_obj_set_size(names[i],150,28);text_font(names[i],&hr_font20,0);
      long_mode(names[i],LV_LABEL_LONG_MODE_DOTS);
      snprintf(b,sizeof b,"%u",value);text(zones[i],b);text_font(zones[i],&hr_font20,0);
      text_align(zones[i],LV_TEXT_ALIGN_RIGHT,0);lv_obj_set_pos(zones[i],164,48+30*i);lv_obj_set_size(zones[i],64,28);
    }
    shape(bars[4][4],12,278,216,32,0x351C31,7);
    lv_obj_set_style_border_width(bars[4][4],2,0);lv_obj_set_style_border_color(bars[4][4],lv_color_hex(RED),0);
    text(help,"返回房间");lv_obj_set_pos(help,20,284);text_font(help,&hr_font16,0);text_align(help,LV_TEXT_ALIGN_CENTER,0);lv_obj_set_width(help,200);break;
  }
  }
  if (v->phone_connected && v->page == HR_PAGE_HOME)
    text(title, "小程序已连接");
  if (c->speaker && !c->game_busy && v->page != HR_PAGE_RADIO) {
    if (v->page == HR_PAGE_HOME || v->page == HR_PAGE_CARD)
      visible(body, false);
    radio_status(v);
  }
}
