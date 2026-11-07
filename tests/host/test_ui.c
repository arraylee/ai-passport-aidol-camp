#include "hr_friend_store.h"
#include "hr_gallery.h"
#include "hr_profiles.h"
static const hr_profile_t *saved[7];
const hr_profile_t *hr_friend_profile(unsigned id) { return saved[id] ? saved[id] : hr_profile(id); }
const lv_image_dsc_t *hr_gallery_image_descriptor(void) { return NULL; }
#include "hr_ui.h"
#include "hr_controls.h"
#include "../voice_route.h"
#include "lvgl.h"
#include "src/misc/lv_text_private.h"
#include "src/widgets/label/lv_label_private.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
LV_FONT_DECLARE(hr_font14);
LV_FONT_DECLARE(hr_font16);
LV_FONT_DECLARE(hr_font20);
LV_FONT_DECLARE(hr_font80);
extern const lv_image_dsc_t hr_game_joy_idle,hr_game_joy_push,hr_game_joy_tuck,hr_game_joy_land,
 hr_game_platform,hr_game_wave,hr_game_spike0,hr_game_spike1,hr_game_flag,
 hr_game_coin,hr_game_glint,hr_game_sky;

static uint8_t pixels[240 * 320 * 3];
static const char *directory;
static unsigned checks;
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *data) {
  for (int y = a->y1; y <= a->y2; y++)
    for (int x = a->x1; x <= a->x2; x++) {
      uint16_t p = data[0] | data[1] << 8;
      data += 2;
      assert(x >= 0 && x < 240 && y >= 0 && y < 320);
      unsigned at = (y * 240 + x) * 3;
      pixels[at] = ((p >> 11) & 31) * 255 / 31;
      pixels[at + 1] = ((p >> 5) & 63) * 255 / 63;
      pixels[at + 2] = (p & 31) * 255 / 31;
    }
  lv_display_flush_ready(d);
}
static void validate(lv_obj_t *o) {
  if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN))
    return;
  if (lv_obj_check_type(o, &lv_label_class)) {
    const char *t = lv_label_get_text(o);
    const lv_font_t *f = lv_obj_get_style_text_font(o, 0);
    lv_point_t size;
    bool wraps=lv_label_get_long_mode(o)==LV_LABEL_LONG_WRAP;
    lv_text_get_size(&size, t, f, 0, wraps?lv_obj_get_style_text_line_space(o,0):0,
                     wraps?lv_obj_get_width(o):10000, LV_TEXT_FLAG_NONE);
    lv_area_t b;
    lv_obj_get_coords(o, &b);
    if ((size.x > lv_obj_get_width(o) &&
         lv_label_get_long_mode(o) != LV_LABEL_LONG_SCROLL_CIRCULAR &&
         lv_label_get_long_mode(o) != LV_LABEL_LONG_MODE_DOTS) ||
        (size.y > lv_obj_get_height(o) && lv_label_get_long_mode(o) != LV_LABEL_LONG_SCROLL_CIRCULAR) || b.x1 < 8 || b.x2 >= 232 || b.y1 < 7 ||
        b.y2 >= 319) {
      fprintf(stderr, "CLIPPED %s: text %d x %d box %d,%d-%d,%d\n", t, size.x,
              size.y, b.x1, b.y1, b.x2, b.y2);
      exit(1);
    }
    for (uint32_t at = 0; t[at];) {
      uint32_t cp = lv_text_encoded_next(t, &at);
      if (cp == 10 || cp == 13)
        continue;
      lv_font_glyph_dsc_t glyph;
      assert(lv_font_get_glyph_dsc(f, &glyph, cp, 0));
    }
    checks++;
  }
  for (unsigned i = 0; i < lv_obj_get_child_count(o); i++)
    validate(lv_obj_get_child(o, i));
}
static lv_obj_t *find_label(const char *text) {
  for (unsigned i = 0; i < lv_obj_get_child_count(lv_screen_active()); i++) {
    lv_obj_t *o = lv_obj_get_child(lv_screen_active(), i);
    if (lv_obj_check_type(o, &lv_label_class) &&
        !lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN) &&
        !strcmp(lv_label_get_text(o), text))
      return o;
  }
  return NULL;
}
static void shot(hr_view_t *v, const char *name) {
  hr_ui_render(v);
  lv_obj_update_layout(lv_screen_active());
  lv_refr_now(NULL);
  validate(lv_screen_active());
  char path[1024];
  snprintf(path, sizeof path, "%s/%s.ppm", directory, name);
  FILE *f = fopen(path, "wb");
  assert(f);
  fprintf(f, "P6\n240 320\n255\n");
  assert(fwrite(pixels, 1, sizeof pixels, f) == sizeof pixels);
  fclose(f);
}
int main(int argc, char **argv) {
  assert(argc == 2);
  directory = argv[1];
  const lv_image_dsc_t *sprites[]={&hr_game_joy_idle,&hr_game_joy_push,&hr_game_joy_tuck,&hr_game_joy_land,
    &hr_game_platform,&hr_game_wave,&hr_game_spike0,&hr_game_spike1,&hr_game_flag,
    &hr_game_coin,&hr_game_glint,&hr_game_sky};
  for(unsigned i=0;i<sizeof sprites/sizeof sprites[0];i++) {
    assert(((uintptr_t)sprites[i]->data&3u)==0);
    assert((sprites[i]->header.stride&1u)==0);
  }

  mkdir(directory, 0755);
  hr_profile_remote(1,"Other hardware","",false);
  assert(!hr_profile(1)->portrait&&!hr_profile(1)->small&&!hr_profile(1)->card);
  hr_profile_clear(1);
  hr_profile_remote_reset(2);
  assert(!strcmp(hr_profile(1)->name,"设备")&&!hr_profile(1)->portrait);
  for(unsigned i=1;i<=6;i++)hr_profile_clear(i);
  lv_init();
  lv_display_t *d = lv_display_create(240, 320);
  lv_display_set_color_format(d, LV_COLOR_FORMAT_RGB565);
  static uint8_t draw[240 * 20 * 2];
  lv_display_set_buffers(d, draw, NULL, sizeof draw,
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(d, flush);
  hr_view_t v = {.battery_percent = -1,
                 .volume = 60,
                 .choice = 1,
                 .setup = true,
                 .radio_ok = true,
                 .audio_ok = true,
                 .buttons_ok = true,
                 .now = 1000};
  uint8_t m[6] = {2, 0, 0, 0, 0, 2};
  hr_init(&v.core, 0, m, 222, 0);
  shot(&v, "01-setup");
  hr_init(&v.core, 2, m, 222, 0);
  v.setup = false;
  v.page = HR_PAGE_RADIO;
  for (int i = 1; i <= 6; i++) {
    v.core.peers[i].seen = true;
    v.core.peers[i].last = 1000;
    v.core.peers[i].zone = i % 3;
    v.core.peers[i].samples = 10;
    v.core.peers[i].signal_q8 = (-45 - i * 6) * 256;
    v.core.peers[i].trend = i % 3 - 1;
    v.core.peers[i].trend_at = 1000;
  }
  shot(&v, "02-home");
  /* Six room members: every actual peer remains visible even on game/back. */
  hr_room_enable(&v.core,v.now);hr_room_join(&v.core,true,v.now);
  for(unsigned id=1;id<=6;id++)if(id!=v.core.id)v.core.peers[id].social_flags=16;
  const char *room_names[]={"","喵喵","胡彦斌","王珞丹","小狮日记","手工耿","LKs"};
  for(unsigned id=1;id<=6;id++)hr_profile_remote(id,room_names[id],"",false);
  const unsigned room_choices[]={0,HR_ROOM_GAME,HR_ROOM_BACK,1,3,4,5,6};
  for(unsigned k=0;k<8;k++) {
    v.selection=room_choices[k];shot(&v,"02b-room-all-members");
    for(unsigned id=1;id<=6;id++)if(id!=v.core.id)assert(find_label(hr_display_name(id)));
    assert(find_label("和全体队友对讲")&&find_label("玩游戏")&&find_label("返回"));
    int all_y=lv_obj_get_y(find_label("和全体队友对讲"));
    int game_y=lv_obj_get_y(find_label("玩游戏")),back_y=lv_obj_get_y(find_label("返回"));
    assert(all_y<game_y && game_y<back_y);
    for(unsigned id=1;id<=6;id++)if(id!=v.core.id)
      assert(back_y<lv_obj_get_y(find_label(hr_display_name(id))));

  }
  /* A peer entering a game must have visible pixels, not merely an unhidden
     label underneath a later-created row background. Exercise live changes. */
  v.selection=0;
  for(unsigned k=3;k<8;k++) {
    unsigned id=room_choices[k];
    v.core.peers[id].social_flags|=64;
    shot(&v,"02c-room-game-status");
    lv_obj_t *busy=find_label("游戏中");assert(busy);
    lv_area_t area;lv_obj_get_coords(busy,&area);
    unsigned ink=0;
    for(int y=area.y1;y<=area.y2;y++)for(int x=area.x1;x<=area.x2;x++) {
      unsigned at=(y*240+x)*3;
      if(pixels[at]>140 && pixels[at+1]>140 && pixels[at+2]>140)ink++;
    }
    fprintf(stderr,"room peer=%u game-status visible pixels=%u\n",id,ink);
    assert(ink>30);
    v.core.peers[id].social_flags&=~64;
    shot(&v,"02d-room-game-return");assert(!find_label("游戏中"));
  }
  for(unsigned id=1;id<=6;id++)hr_profile_clear(id);
  v.core.room_enabled=false;v.core.room_joined=false;v.selection=0;
  for(unsigned id=1;id<=6;id++)v.core.peers[id].social_flags=0;

  v.selection = 3;
  v.nearest = hr_nearest(&v.core, v.now, 0);
  shot(&v, "03-target");
  hr_press(&v.core, 3, 1000);
  v.core.speaker = 2;
  v.core.floor_request = v.core.request;
  v.core.floor_at = 1000;
  v.core.floor_client_at = 1000;
  shot(&v, "04-speaking");
  hr_release(&v.core, 1000);
  v.core.speaker = 4;
  v.core.floor_target = 0;
  v.receiving = true;
  shot(&v, "05-listening");
  v.core.floor_target = 1;
  shot(&v, "06-busy");
  v.core.held = true;
  v.core.blocked = true;
  shot(&v, "07-blocked");
  v.core.held = false;
  v.core.speaker = 0;
  v.now = 5000;
  shot(&v, "08-offline");
  v.now = 1000;
  v.core.conflict_until = 4000;
  shot(&v, "09-conflict");
  v.setup = true;
  v.save_failed = true;
  shot(&v, "10-save-error");
  v.setup = false;
  v.save_failed = false;
  v.core.conflict_until = 0;
  for (int self = 1; self <= 6; self++) {
    v.core.id = self;
    v.selection = self == 2 ? 3 : 2;
    for (int j = 1; j <= 6; j++)
      v.core.peers[j].signal_q8 = -45 * 256;
    char name[30];
    snprintf(name, sizeof name, "11-near-self-%d", self);
    for (int frame = 0; frame < 20; frame++)
      hr_ui_render(&v);
    shot(&v, name);
  }
  v.core.id = 1;
  v.selection = 2;
  shot(&v, "12-miaomiao-focus");
  v.volume_mode = true;
  v.volume = 100;
  shot(&v, "13-volume");
  v.volume_mode = false;
  v.selection = 0;
  shot(&v, "14-all");
  v.volume_mode = false;
  for (int self = 1; self <= 6; self++) {
    v.core.id = self;
    v.page = HR_PAGE_HOME;
    char name[50];
    snprintf(name, sizeof name, "20-card-%d", self);
    shot(&v, name);
    if (self > 1) {
      v.page = HR_PAGE_QR;
      snprintf(name, sizeof name, "21-qr-%d", self);
      shot(&v, name);
    }
    v.page = HR_PAGE_GALLERY;
    v.gallery = self;
    snprintf(name, sizeof name, "22-gallery-%d", self);
    shot(&v, name);
  }
  v.core.id = 1;
  v.page = HR_PAGE_MENU;
  for (int i = 0; i < 6; i++) {
    v.menu = i;
    shot(&v, "23-menu");
  }
  v.page = HR_PAGE_PAIR;
  shot(&v, "24-pair-empty");
  v.core.social.candidate = 4;
  shot(&v, "25-pair-found");
  assert(find_label("下键确认添加好友"));
  v.pair_preview=hr_profile(1)->portrait;
  shot(&v, "touch-preview-v0415");
  assert(find_label("下键确认添加好友"));
  assert(!find_label("头像"));
  v.pair_preview=NULL;

  assert(!find_label("上／中选人，双方下键确认"));
  assert(!find_label("长中看好友  长下回首页"));
  v.core.social.accepted = true;
  shot(&v, "26-pair-confirmed");
  assert(find_label("等待对方确认")&&!find_label("下键确认添加好友"));
  v.core.social.saved = true;
  shot(&v, "27-pair-wait");
  v.core.social.complete = true;
  shot(&v, "28-pair-success");
  assert(find_label("名片已保存"));
  v.core.social.failed = true;
  shot(&v, "29-pair-failed");
  v.core.social.friends = 0x7c;
  v.page = HR_PAGE_FRIENDS;
  v.friend_selection = 4;
  shot(&v, "30-friends");
  v.page = HR_PAGE_CARD;
  v.card_id = 3;
  shot(&v, "31-friend-card");
  assert(find_label("示例二维码")&&find_label("好友名片"));
  v.page = HR_PAGE_RADIO;
  v.nearest = 0;
  v.selection = 4;
  for (int i = 1; i <= 6; i++) {
    v.core.peers[i].zone = 1;
    v.core.peers[i].signal_q8 = -65 * 256;
  }
  shot(&v, "32-equal-medium");
  v.core.peers[4].zone = 2;
  v.core.peers[4].signal_q8 = -40 * 256;
  v.nearest = 4;
  shot(&v, "33-nearest");
  v.up_pressed = true;
  shot(&v, "34-press-feedback");
  v.core.id = 1;
  v.page = HR_PAGE_HOME;
  shot(&v, "home-v0414-miaomiao");
  /* Battery must be visible immediately left of the FoloToy brand. */
  lv_obj_t *battery_outline=NULL;
  for(unsigned i=0;i<lv_obj_get_child_count(lv_screen_active());i++) {
    lv_obj_t *o=lv_obj_get_child(lv_screen_active(),i);
    if(!lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN) && lv_obj_get_x(o)==136 &&
       lv_obj_get_y(o)==16 && lv_obj_get_width(o)==22 && lv_obj_get_height(o)==14)
      battery_outline=o;
  }
  assert(battery_outline);
  lv_obj_t *battery_fill=NULL,*battery_tip=NULL,*battery_unknown=NULL;
  for(unsigned i=0;i<lv_obj_get_child_count(lv_screen_active());i++) {
    lv_obj_t *o=lv_obj_get_child(lv_screen_active(),i);
    if(lv_obj_get_x(o)==138 && lv_obj_get_y(o)==18) battery_fill=o;
    if(lv_obj_get_x(o)==158 && lv_obj_get_y(o)==20) battery_tip=o;
    if(lv_obj_get_x(o)==144 && lv_obj_get_y(o)==22) battery_unknown=o;
  }
  assert(battery_fill&&battery_tip&&battery_unknown);
  lv_obj_t *brand=find_label("FoloToy");
  assert(brand&&lv_obj_get_x(brand)>=168&&lv_obj_get_width(brand)==60);
  const int socs[]={-1,0,1,19,20,39,40,50,100,101};
  for(unsigned k=0;k<sizeof socs/sizeof socs[0];k++) {
    int pct=socs[k];v.battery_percent=pct;
    char name[40];snprintf(name,sizeof name,"battery-v072-%d",pct);shot(&v,name);
    bool known=pct>=0&&pct<=100;
    uint32_t color=!known?0x91a4bb:pct<20?0xE0463A:pct<40?0xF2C14E:0xf0f5ff;
    assert(lv_color_eq(lv_obj_get_style_border_color(battery_outline,0),lv_color_hex(color)));
    assert(lv_color_eq(lv_obj_get_style_bg_color(battery_tip,0),lv_color_hex(color)));
    assert(lv_obj_has_flag(battery_fill,LV_OBJ_FLAG_HIDDEN)==!(known&&pct>0));
    assert(lv_obj_has_flag(battery_unknown,LV_OBJ_FLAG_HIDDEN)==known);
    if(known&&pct>0)assert(lv_obj_get_width(battery_fill)==(18*pct+99)/100);
    assert(!lv_obj_has_flag(battery_outline,LV_OBJ_FLAG_HIDDEN));
  }
  v.page=HR_PAGE_FRIENDS;shot(&v,"battery-v072-hidden-friends");
  assert(lv_obj_has_flag(battery_outline,LV_OBJ_FLAG_HIDDEN));
  assert(lv_obj_has_flag(battery_tip,LV_OBJ_FLAG_HIDDEN));
  assert(lv_obj_has_flag(battery_fill,LV_OBJ_FLAG_HIDDEN));
  assert(lv_obj_has_flag(battery_unknown,LV_OBJ_FLAG_HIDDEN));
  v.page=HR_PAGE_HOME;v.battery_percent=86;v.home_selection=1;shot(&v,"battery-v072-home-final");v.home_selection=0;
  assert(find_label("FoloToy\n简单开放\n人人可造"));
  static uint8_t joy_profile_pixels[96*156*2];
  char joy_profile_path[1024];
  snprintf(joy_profile_path,sizeof joy_profile_path,
           "%s/../../../tools/device_console/presets/6-profile.bin",directory);
  FILE *joy_profile=fopen(joy_profile_path,"rb");
  assert(joy_profile && fseek(joy_profile,4096,SEEK_SET)==0);
  assert(fread(joy_profile_pixels,1,sizeof joy_profile_pixels,joy_profile)==sizeof joy_profile_pixels);
  fclose(joy_profile);
  static const lv_image_dsc_t joy_profile_image={.header={.magic=LV_IMAGE_HEADER_MAGIC,
      .cf=LV_COLOR_FORMAT_RGB565,.w=96,.h=156,.stride=192},
      .data_size=sizeof joy_profile_pixels,.data=joy_profile_pixels};
  hr_profile_override(1, "京东3C数码",
                      "谁敢横刀立马 唯我3C数码 战斗战斗 只做第一", NULL, NULL);
  hr_profile_set_portrait(1,&joy_profile_image);
  shot(&v, "home-jd-name-fit");
  lv_obj_t *jd_name=find_label("京东3C数码");
  assert(jd_name);
  lv_point_t jd_size;
  lv_text_get_size(&jd_size,"京东3C数码",&hr_font20,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
  assert(jd_size.x<=lv_obj_get_content_width(jd_name));
  for(unsigned tick=0;tick<60;tick++) {
    hr_ui_render(&v);lv_tick_inc(100);lv_timer_handler();
    lv_obj_update_layout(lv_screen_active());
    assert(((lv_label_t *)jd_name)->offset.x==0);
    assert(lv_obj_get_height(jd_name)==hr_font20.line_height);
  }
  hr_profile_clear(1);
  hr_profile_override(1, "喵喵喵喵喵喵喵喵喵喵喵喵喵喵喵喵",
                      "我是自定义签名我是自定义签名我是自定义签名这里结束",
                      NULL, NULL);
  hr_profile_set_title(1, "我也可以修改头衔和名字");
  v.core.id = 1;
  v.page = HR_PAGE_HOME;
  v.phone_connected = true;
  v.core.speaker = 0;
  shot(&v, "35-custom-long-profile");
  // Repeated telemetry redraws must not restart long-name scrolling.
  bool scrolled = false;
  for (unsigned tick = 0; tick < 60; tick++) {
    hr_ui_render(&v);
    lv_tick_inc(100);
    lv_timer_handler();
    lv_obj_update_layout(lv_screen_active());
    for (unsigned j = 0; j < lv_obj_get_child_count(lv_screen_active()); j++) {
      lv_obj_t *o = lv_obj_get_child(lv_screen_active(), j);
      if (lv_obj_check_type(o, &lv_label_class) &&
          !strcmp(lv_label_get_text(o), hr_display_name(1)) &&
          ((lv_label_t *)o)->offset.x != 0)
        scrolled = true;
    }
  }
  assert(scrolled);

  v.core.speaker = 2;
  shot(&v, "36-custom-incoming");
  v.core.speaker = 0;
  v.phone_connected = false;
  hr_profile_override(2, "小猫来自直播间刚刚改了名字", "支持普通中文", NULL,
                      NULL);
  v.page = HR_PAGE_RADIO;
  v.selection = 2;
  shot(&v, "37-custom-peer");
  v.page = HR_PAGE_HOME;
  v.phone_connected = false;
  v.core.speaker = 0;
  hr_profile_clear(1);
  v.home_selection = 0;
  shot(&v, "40-aidol-friend");
  v.home_selection = 1;
  shot(&v, "41-aidol-radio");
  v.core.speaker = 2;
  v.core.floor_target = 3;
  shot(&v, "42-home-private-call");
  char busy[80];
  snprintf(busy, sizeof busy, "忙碌：%s", hr_display_name(2));
  assert(find_label(busy));
  v.core.speaker = 0;
  v.page = HR_PAGE_PAIR;
  v.core.social.candidate = 2;
  hr_ui_render(&v);
  lv_obj_update_layout(lv_screen_active());
  v.core.social.candidate = 0;
  shot(&v, "43-friend-left");
  lv_obj_t *empty = find_label("靠近一位朋友");
  assert(empty && lv_obj_get_y(empty) == 34);

  hr_profile_clear(2);
  v.page = HR_PAGE_LOBBY;
  hr_room_enable(&v.core, v.now);
  shot(&v, "44-lobby-before");
  assert(!find_label(hr_display_name(2)) && !find_label(hr_display_name(6)));
  assert(find_label("一起加入") && find_label("【Aidol 直播房间】"));
  v.room_joining = true;
  shot(&v, "44b-lobby-joining");
  assert(find_label("正在加入") && !find_label(hr_display_name(2)));
  v.room_joining = false;
  hr_room_join(&v.core, true, v.now);
  v.page = HR_PAGE_RADIO;
  shot(&v, "45-lobby-joined");
  assert(find_label("还没有队友，可以先玩游戏") && find_label("玩游戏") && find_label("返回") && !find_label("和全体队友对讲"));
  assert(!find_label(hr_display_name(2)) && !find_label(hr_display_name(6)));
  v.core.peers[2].social_flags |= 16;
  v.core.peers[2].hello_at = v.now;
  shot(&v, "45b-lobby-member");
  assert(find_label(hr_display_name(2)) && !find_label(hr_display_name(6)));
  v.page = HR_PAGE_RADIO;
  shot(&v, "45c-radio-members");
  assert(find_label(hr_display_name(2)) && !find_label(hr_display_name(6)));
  assert(!find_label("2/6"));
  for(unsigned kind=HR_INTRO_BOOT;kind<=HR_INTRO_SLEEP;kind++) {
    v.intro_kind=kind;
    for(unsigned frame=0;frame<30;frame++) {
      v.intro_elapsed=frame*100;
      char name[64];snprintf(name,sizeof name,"50-intro-%u",kind);
      shot(&v,name);
    }
  }
  v.intro_kind=HR_INTRO_NONE;
  v.page=HR_PAGE_LOBBY;v.room_joining=true;v.room_join_started=1000;
  for(unsigned frame=0;frame<18;frame++) {v.now=1000+frame*100;shot(&v,"51-joining");}
  v.room_joining=false;
  v.page = HR_PAGE_HOME;
  v.home_selection = 2;
  shot(&v, "46-home-album");
  assert(!find_label("AI PASSPORT") && !find_label("签名"));
  hr_view_home(&v,v.now);
  shot(&v,"46b-home-default-team");
  assert(v.home_selection==1);
  lv_obj_t *team=find_label("组队");assert(team);
  bool team_border=false;
  for(unsigned i=0;i<lv_obj_get_child_count(lv_screen_active());i++) {
    lv_obj_t *o=lv_obj_get_child(lv_screen_active(),i);
    if(!lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN)&&!lv_obj_check_type(o,&lv_label_class)&&
       lv_obj_get_style_border_width(o,0)==2&&
       lv_obj_get_x(o)<=lv_obj_get_x(team)&&
       lv_obj_get_x(o)+lv_obj_get_width(o)>=lv_obj_get_x(team)+lv_obj_get_width(team)&&
       lv_obj_get_y(o)<=lv_obj_get_y(team)&&
       lv_obj_get_y(o)+lv_obj_get_height(o)>=lv_obj_get_y(team)+lv_obj_get_height(team))
      team_border=true;
  }
  assert(team_border);
  v.page = HR_PAGE_PAIR;
  v.core.social = (hr_social_t){
      .active = true, .accepted = true, .candidate = 2, .pending = true};
  v.card_progress = 50;
  shot(&v, "47-friend-transfer");
  v.page=HR_PAGE_HOME;v.activation_required=true;
  shot(&v,"52-inactive");
  assert(find_label("待激活") && !find_label("组队") && !find_label("好友"));
  v.activation_required=false;
  shot(&v,"53-activated");
  assert(!find_label("待激活") && find_label("好友"));
  /* A real square-avatar profile with a longer, public signature. */
  static uint8_t photo_pixels[96 * 156 * 2];
  char asset_path[1024];
  snprintf(asset_path, sizeof asset_path,
           "%s/../../../assets/profiles/huyanbin-live-portrait.rgb565", directory);
  FILE *asset = fopen(asset_path, "rb");
  assert(asset && fread(photo_pixels, 1, sizeof photo_pixels, asset) == sizeof photo_pixels);
  fclose(asset);
  static lv_image_dsc_t photo = {.header = {.magic=LV_IMAGE_HEADER_MAGIC,
      .cf=LV_COLOR_FORMAT_RGB565,.w=96,.h=156,.stride=192},
      .data_size=sizeof photo_pixels,.data=photo_pixels};
  v.core.id=2;
  hr_profile_override(2, "胡彦斌", "做你想做的事，成为你想成为的人！", NULL, NULL);
  hr_profile_set_portrait(2, &photo);
  v.home_selection=0;
  shot(&v,"54-square-huyanbin");
  unsigned full_portraits=0;
  for(unsigned i=0;i<lv_obj_get_child_count(lv_screen_active());i++) {
    lv_obj_t *parent=lv_obj_get_child(lv_screen_active(),i);
    if(lv_obj_has_flag(parent,LV_OBJ_FLAG_HIDDEN))continue;
    for(unsigned j=0;j<lv_obj_get_child_count(parent);j++) {
      lv_obj_t *child=lv_obj_get_child(parent,j);
      if(lv_obj_check_type(child,&lv_image_class)&&lv_image_get_src(child)==&photo) {
        assert(lv_obj_get_width(parent)==96&&lv_obj_get_height(parent)==156);
        assert(lv_obj_get_x(child)==0&&lv_obj_get_y(child)==0);
        assert(lv_image_get_scale_x(child)==256&&lv_image_get_scale_y(child)==256);
        full_portraits++;
      }
    }
  }
  assert(full_portraits==1);
  lv_obj_t *bio=find_label("做你想做\n的事，\n成为你想成\n为的人！");
  assert(bio && lv_obj_get_width(bio)==104);
  assert(lv_obj_get_x(bio)==124 && lv_obj_get_style_text_align(bio,0)==LV_TEXT_ALIGN_LEFT);
  assert(lv_obj_get_style_text_font(bio,0)==&hr_font16);
  assert(lv_obj_get_style_text_font(find_label("胡彦斌"),0)==&hr_font20);
  assert(lv_obj_get_style_text_line_space(bio,0)==6);
  assert(lv_obj_get_y(bio)+lv_obj_get_height(bio)<=228);
  v.page=HR_PAGE_LOBBY;
  shot(&v,"55-square-return-lobby");
  v.page=HR_PAGE_HOME;
  shot(&v,"56-square-return-home");
  // Reproduce a saved card with its real 64x104 portrait and legacy squashed 32x32 thumb.
  static uint8_t saved_pixels[64*104*2], bad_thumb[32*32*2];
  for (unsigned y=0;y<104;y++) for(unsigned x=0;x<64;x++)
    memcpy(saved_pixels+(y*64+x)*2,photo_pixels+((y*156/104)*96+x*96/64)*2,2);
  static lv_image_dsc_t saved_image={.header={.magic=LV_IMAGE_HEADER_MAGIC,
      .cf=LV_COLOR_FORMAT_RGB565,.w=64,.h=104,.stride=128},.data_size=sizeof saved_pixels,.data=saved_pixels};
  static lv_image_dsc_t bad_image={.header={.magic=LV_IMAGE_HEADER_MAGIC,
      .cf=LV_COLOR_FORMAT_RGB565,.w=32,.h=32,.stride=64},.data_size=sizeof bad_thumb,.data=bad_thumb};
  static hr_profile_t friend_fixture[7];
  for(unsigned id=2;id<=6;id++) {
    friend_fixture[id]=*hr_profile(id);friend_fixture[id].portrait=&saved_image;
    friend_fixture[id].card=&saved_image;friend_fixture[id].small=&bad_image;saved[id]=&friend_fixture[id];
  }
  v.page=HR_PAGE_CARD;v.card_id=2;
  shot(&v, "57-friend-huyanbin");
  assert(find_label("胡彦斌")&&find_label("示例二维码"));
  /* A long real bio remains static, fully visible and clear of portrait/return. */
  friend_fixture[3].name="王珞丹";
  friend_fixture[3].signature="博主只在这里瞎胡闹，正经事请找@王珞丹工作室";
  static uint8_t wang_pixels[96*156*2],wang_saved_pixels[64*104*2];
  snprintf(asset_path,sizeof asset_path,"%s/../../../tools/device_console/presets/3-profile.bin",directory);
  asset=fopen(asset_path,"rb");assert(asset&&fseek(asset,4096,SEEK_SET)==0);
  assert(fread(wang_pixels,1,sizeof wang_pixels,asset)==sizeof wang_pixels);fclose(asset);
  for(unsigned y=0;y<104;y++)for(unsigned x=0;x<64;x++)
    memcpy(wang_saved_pixels+(y*64+x)*2,wang_pixels+((y*156/104)*96+x*96/64)*2,2);
  static lv_image_dsc_t wang_image={.header={.magic=LV_IMAGE_HEADER_MAGIC,
    .cf=LV_COLOR_FORMAT_RGB565,.w=64,.h=104,.stride=128},.data_size=sizeof wang_saved_pixels,.data=wang_saved_pixels};
  friend_fixture[3].portrait=&wang_image;
  v.card_id=3;shot(&v,"57b-friend-wangluodan-long-signature");
  friend_fixture[3].portrait=&saved_image;
  lv_obj_t *long_bio=find_label(friend_fixture[3].signature);assert(long_bio);
  assert(lv_label_get_long_mode(long_bio)==LV_LABEL_LONG_WRAP);
  assert(lv_obj_get_style_text_font(long_bio,0)==&hr_font16);
  assert(lv_obj_get_y(long_bio)>=210);
  assert(lv_obj_get_y(long_bio)+lv_obj_get_height(long_bio)<=280);
  v.card_id=2;
  v.page=HR_PAGE_PAIR;v.core.id=1;v.core.social=(hr_social_t){.candidate=2};
  shot(&v, "58-confirm-huyanbin");
  v.core.social.pending=true;v.core.social.accepted=true;v.card_progress=50;
  shot(&v, "59-exchange-huyanbin");
  assert(find_label("正在交换名片"));
  v.core.social=(hr_social_t){.locked_candidate=2};
  shot(&v, "60-prompt-peer-away");
  assert(find_label("胡彦斌")&&find_label("请重新靠近对方"));
  assert(!find_label("下键确认添加好友"));
  v.page=HR_PAGE_CARD;v.card_id=2;v.card_from_pair=true;
  shot(&v,"61-added-friend-detail");
  assert(find_label("已成为好友")&&find_label("返回"));
  v.card_from_pair=false;
  v.page=HR_PAGE_FRIENDS;v.core.social.friends=0x7c;
  for(unsigned selected=2;selected<=6;selected++) {
    v.friend_selection=selected;
    char name[64];snprintf(name,sizeof name,"62-friends-selected-%u",selected);shot(&v,name);
    unsigned photos=0;lv_obj_t *focus=NULL;
    for(unsigned i=0;i<lv_obj_get_child_count(lv_screen_active());i++) {
      lv_obj_t *o=lv_obj_get_child(lv_screen_active(),i);
      if(lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN))continue;
      if(lv_obj_check_type(o,&lv_image_class)) {
        assert(lv_image_get_src(o)==&saved_image);
        assert(lv_image_get_scale_x(o)==96&&lv_image_get_scale_y(o)==96);
        photos++;
      } else if(lv_obj_get_style_border_width(o,0)==2)focus=o;
      else if(lv_obj_check_type(o,&lv_label_class)&&lv_obj_get_x(o)==52 &&
              lv_obj_get_style_text_font(o,0)==&hr_font14) {
        assert(lv_obj_get_height(o)==18);
        assert(lv_label_get_long_mode(o)==LV_LABEL_LONG_MODE_DOTS);
      }
    }
    assert(photos==(selected==6?1:4)&&focus);
    int top=50+54*((selected-2)%4);
    assert(lv_obj_get_y(focus)==top-2&&lv_obj_get_height(focus)==50);
    assert(top+2+39<=lv_obj_get_y(focus)+lv_obj_get_height(focus)-2);
    assert(top+29+hr_font14.line_height<=lv_obj_get_y(focus)+lv_obj_get_height(focus)-2);
  }
  v.core.social.active=v.core.social.manual=true;
  shot(&v,"manual-v0710-friends-full");
  assert(find_label("寻找中…")&&find_label("请对方也打开好友"));
  v.core.social.friends=0;shot(&v,"manual-v0710-friends-empty");
  assert(find_label("正在寻找好友")&&find_label("双方打开好友并靠近"));
  v.page=HR_PAGE_PAIR;v.core.social.candidate=v.core.social.locked_candidate=2;
  shot(&v,"manual-v0710-confirm");
  assert(find_label("寻找好友 · 确认")&&find_label("下键确认添加好友"));
  v.core.social=(hr_social_t){0};
  v.core.game_busy=true;v.core.speaker=0;v.volume_mode=false;v.core.room_joined=true;
  v.page=HR_PAGE_GAME_MENU;v.game_selection=0;
  shot(&v,"70-game-menu");assert(find_label("开始游戏")&&find_label("排行榜")&&find_label("返回"));
  assert(find_label("高低音挑战"));
  for(unsigned selection=1;selection<3;selection++){v.game_selection=selection;shot(&v,"70b-game-menu-selection");}
  v.page=HR_PAGE_GAME_PLAY;hr_game_start(&v.game,1000);v.now=2000;
  shot(&v,"71-game-ready");
  for(unsigned second=0;second<3;second++) {
    v.now=1000+second*1000;char digit[2]={(char)('3'-second),0};
    char name[40];snprintf(name,sizeof name,"71-countdown-%s",digit);shot(&v,name);
    lv_obj_t *countdown=find_label(digit);assert(countdown);
    assert(lv_obj_get_style_text_font(countdown,0)==&hr_font80);
    assert(hr_font80.line_height>=55); // Latin digits occupy less than the em size.
    assert(lv_obj_get_width(countdown)>=120);
    assert(lv_obj_get_x(countdown)+lv_obj_get_width(countdown)/2==120);
    assert(lv_obj_get_y(countdown)>=100 && lv_obj_get_y(countdown)<160);
  }
  hr_game_tick(&v.game,3999,180);
  unsigned pause=0,pause_left=0;const unsigned rests[]={155,345,645,845};
  for(unsigned frame=0;frame<700;frame++) {
    unsigned x=hr_game_player_x(&v.game);
    if(pause<4 && !pause_left && x>=rests[pause]){pause++;pause_left=30;}
    unsigned hz=pause_left?0:reference_voice(x);if(pause_left)pause_left--;
    v.now=4000+frame*40;hr_game_tick(&v.game,v.now,hz);
    shot(&v,"72-game-running");
    char full[64];snprintf(full,sizeof full,"90-game-full-%03u",frame);shot(&v,full);
    if(frame>=100 && frame<160) {
      char path[64];snprintf(path,sizeof path,"72-motion-%03u",frame);shot(&v,path);
    }
  }
  assert(find_label("下键返回"));
  hr_game_t completed_game=v.game;uint32_t completed_now=v.now;
  hr_game_start(&v.game,200000);v.game.phase=HR_GAME_RUNNING;v.now=202960;shot(&v,"joy-v073-idle");
  v.game.phase=HR_GAME_RUNNING;v.game.grounded=false;v.game.height=37;
  v.now=203000;shot(&v,"joy-v073-push");
  v.game.height=90;v.now=203160;shot(&v,"joy-v073-tuck");
  v.game.height=80;v.now=203200;shot(&v,"joy-v073-descend");
  v.game.grounded=true;v.game.landings=1;v.game.ground_at=203240;v.game.height=30;
  v.now=203240;shot(&v,"joy-v073-land");
  v.game.hit_flash=true;shot(&v,"joy-v073-damage");
  v.game=completed_game;v.now=completed_now;
  shot(&v,"72z-finish");assert(v.game.coins_taken==12 && !v.game.rescues);
  v.page=HR_PAGE_GAME_RESULT;v.game.score=570;v.core.best_score=570;v.core.best_valid=true;v.new_best=true;
  shot(&v,"73-game-result");assert(find_label("570")&&find_label("新纪录！"));
  for(unsigned choice=0;choice<3;choice++) {
    v.result_selection=choice;shot(&v,"73b-game-result-options");
    lv_obj_t *again=find_label("再玩一次"),*rank=find_label("排行榜"),*back=find_label("返回房间");
    assert(again&&rank&&back);
    assert(lv_obj_get_y(again)+lv_obj_get_height(again)<lv_obj_get_y(rank));
    assert(lv_obj_get_y(rank)+lv_obj_get_height(rank)<lv_obj_get_y(back));
  }
  v.page=HR_PAGE_RANK;v.core.best_valid=false;
  for(unsigned id=1;id<=6;id++)v.core.peers[id].score_valid=false;
  shot(&v,"74-rank-empty");assert(find_label("排行榜为空")&&find_label("返回房间"));
  v.core.best_valid=true;v.core.best_score=500;
  for(unsigned id=2;id<=6;id++) {
    v.core.peers[id].seen=true;v.core.peers[id].last=v.core.peers[id].hello_at=v.now;
    v.core.peers[id].social_flags=16;v.core.peers[id].score_valid=true;v.core.peers[id].best_score=100+id*100;
  }
  shot(&v,"75-rank-six");assert(find_label("700"));
  v.page=HR_PAGE_RADIO;v.core.game_busy=false;v.selection=HR_ROOM_GAME;
  shot(&v,"76-room-game-action");assert(find_label("玩游戏"));
  v.selection=HR_ROOM_BACK;shot(&v,"77-room-return");assert(find_label("返回"));
  v.page=HR_PAGE_GALLERY;v.gallery_return=false;shot(&v,"78-photo-clean");assert(!find_label("返回"));
  v.gallery_return=true;shot(&v,"79-photo-return");assert(find_label("返回"));
  v.page=HR_PAGE_FRIENDS;v.friend_selection=0;shot(&v,"80-friends-return");assert(find_label("返回"));
  v.page=HR_PAGE_HOME;shot(&v,"81-game-back-home");
  lv_mem_monitor_t mem;
  lv_mem_monitor(&mem);
  assert(mem.free_size > 4096);
  printf("LVGL free %u max-used %u\n", (unsigned)mem.free_size,
         (unsigned)mem.max_used);
  printf("PASS native 240x320 screens / %u label bounds and glyph checks\n",
         checks);
}
