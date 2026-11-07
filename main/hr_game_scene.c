#include "hr_game_scene.h"
#include <stdint.h>
#include "hr_game_pose.h"
LV_FONT_DECLARE(hr_font20);
extern const lv_image_dsc_t hr_game_joy_idle,hr_game_joy_push,hr_game_joy_tuck,hr_game_joy_land,
 hr_game_platform,hr_game_wave,hr_game_spike0,hr_game_spike1,hr_game_flag,
 hr_game_coin,hr_game_glint,hr_game_sky;
/* Snapshot is copied under the existing LVGL lock. No framebuffer/layer/transform. */
static hr_game_t snapshot;
static uint32_t scene_now;
static hr_jump_animation_t jump_animation;
static unsigned jump_pose;
static int level_y(int height){return 262-height;}
static void rect(lv_layer_t*l,int x,int y,int w,int h,uint32_t color,unsigned r) {
 if(w<=0||h<=0||x>=240||x+w<=0)return;
 lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);
 d.bg_color=lv_color_hex(color);d.radius=r;
 lv_area_t a={x,y,x+w-1,y+h-1};lv_draw_rect(l,&d,&a);
}
static void triangle(lv_layer_t*l,int x1,int y1,int x2,int y2,int x3,int y3,uint32_t color) {
 lv_draw_triangle_dsc_t d;lv_draw_triangle_dsc_init(&d);d.color=lv_color_hex(color);
 d.p[0]=(lv_point_precise_t){x1,y1};d.p[1]=(lv_point_precise_t){x2,y2};d.p[2]=(lv_point_precise_t){x3,y3};
 lv_draw_triangle(l,&d);
}
static void facet(lv_layer_t*l,int x,int y,int w,int h,uint32_t body,uint32_t edge) {
 triangle(l,x+20,y,x+w,y+18,x+w-20,y+h,edge);
 triangle(l,x+20,y,x+w-20,y+h,x,y+h-18,0x25486D);
 triangle(l,x+22,y+5,x+w-5,y+22,x+w-22,y+h-5,body);
 triangle(l,x+22,y+5,x+w-22,y+h-5,x+5,y+h-22,body);
}
static void sprite(lv_layer_t*l,const lv_image_dsc_t*s,int x,int y,int w) {
 int width=w?w:s->header.w;
 if(x>=240||x+width<=0)return;
 lv_draw_image_dsc_t d;lv_draw_image_dsc_init(&d);d.src=s;d.tile=w!=0;
 lv_area_t a={x,y,x+width-1,y+(int)s->header.h-1};d.image_area=a;
 lv_draw_image(l,&d,&a);
}
static void draw(lv_event_t*e) {
 const hr_game_t*g=&snapshot;lv_layer_t*l=lv_event_get_layer(e);
 unsigned scroll=g->progress,phase=scene_now/120;
 // Deep navy player column; chrome stage facets stay in the far/right layer.
 sprite(l,&hr_game_sky,0,0,240);
 int drift=(int)(scroll/24)%16;
 facet(l,132-drift,93,70,109,0x143357,0x8CAAC7);
 facet(l,200-drift,151,65,108,0x0B2647,0x7294B5);
 facet(l,-40,100,42,135,0x102948,0x274A6B);
 rect(l,110,252,130,1,0x183D65,0);
 rect(l,130,256,110,2,0x285881,0);
 sprite(l,&hr_game_glint,159-drift,99,0);
 sprite(l,&hr_game_glint,216-drift/2,174,0);
 lv_draw_label_dsc_t name;lv_draw_label_dsc_init(&name);
 name.font=&hr_font20;name.color=lv_color_hex(0x82A5C5);name.text="Aidol";
 lv_area_t name_area={145-drift,141,202-drift,165};lv_draw_label(l,&name,&name_area);

 for(unsigned i=0;i<HR_GAME_PLATFORMS;i++) {
  const hr_game_platform_t*p=&hr_game_platforms[i];int x=p->x-(int)scroll,y=level_y(p->height);
  if(x>=240||x+p->width<=0)continue;
  int stem=p->width*45/100;if(stem>82)stem=82;
  int wood=x+(p->width-stem)/2;
  rect(l,wood,y+5,stem,303-y,0x244461,5);
  rect(l,wood+4,y+10,5,294-y,0x9FB5CB,2);
  rect(l,wood+stem-7,y+7,4,300-y,0x15324F,2);
  rect(l,wood+stem/2,y+24,2,17,0x15324F,1);
  rect(l,wood+stem/3,y+54,2,21,0x557B9D,1);
  sprite(l,&hr_game_platform,x,y,p->width);
 }
 for(unsigned i=0;i<HR_GAME_COINS;i++) {
  const hr_coin_t*c=&g->coins[i];if(!c->taken) {
   int cx=c->x-(int)scroll,cy=level_y(c->height);
   if(c->high) {
    rect(l,cx-11,cy-11,22,22,0xAE587B,11);
    rect(l,cx-15,cy-1,5,2,0xFFEB9A,1);rect(l,cx+10,cy-1,5,2,0xFFEB9A,1);
    rect(l,cx-1,cy-15,2,5,0xFFEB9A,1);
   }
   sprite(l,&hr_game_coin,cx-6,cy-8+(int)((phase+i)%4==0),0);
  }
 }
 for(unsigned i=0;i<2;i++) {
  const hr_obstacle_t*o=&g->obstacles[i];if(!o->hit)
   sprite(l,phase%2?&hr_game_spike0:&hr_game_spike1,o->x-(int)scroll-16,level_y(o->height)-16,0);
 }
 // Final platform has an unmistakable chequered flag, visible before the finish.
 sprite(l,&hr_game_flag,1054-(int)scroll,level_y(50)-44,0);
 int y=level_y(g->height)-56;
 if(g->hit_flash) {
  lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);d.bg_opa=LV_OPA_TRANSP;
  d.border_width=2;d.border_color=lv_color_hex(0xFFAE9E);d.radius=LV_RADIUS_CIRCLE;
  lv_area_t a={24,y-4,84,y+60};lv_draw_rect(l,&d,&a);
 }
 const lv_image_dsc_t *poses[]={&hr_game_joy_idle,&hr_game_joy_push,&hr_game_joy_tuck,&hr_game_joy_land};
 int bob=(g->phase==HR_GAME_CLEAR||g->phase==HR_GAME_DONE)?(int)(phase%4<2?0:3):0;
 int joy_x=26;
 if(g->phase==HR_GAME_CLEAR||g->phase==HR_GAME_DONE) {
  unsigned clear=scene_now-g->clear_at;
  if(clear>HR_GAME_CLEAR_MS)clear=HR_GAME_CLEAR_MS;
  joy_x+=(int)(34*clear/HR_GAME_CLEAR_MS);
 }
 sprite(l,poses[jump_pose],joy_x,y-bob,0);
 // White crests + two blue water layers, tile directly from Flash.
 rect(l,0,302,240,18,0x164775,0);
 sprite(l,&hr_game_wave,-(int)(scene_now/100)%32,292,272);
 rect(l,0,314,240,6,0x0B2444,0);
 if(g->phase==HR_GAME_CLEAR || g->phase==HR_GAME_DONE) {
  const uint32_t colors[]={0xD8E9FA,0xA5C6E8,0x4F9CE8,0xFFE39A};
  for(unsigned i=0;i<18;i++) {
   int x=(i*47+scene_now/80)%236,yc=86+(i*29+scene_now/14)%167;
   rect(l,x,yc,i%2?3:6,i%2?7:3,colors[i%4],1);
  }
 }
 // HUD and course progress stay above the scene; all opaque, no group fading.
 rect(l,12,10,82,34,0xEEF5FF,12);rect(l,108,10,120,34,0xEEF5FF,12);
 sprite(l,&hr_game_coin,119,19,0);
 rect(l,14,77,212,4,0x19395D,2);
 rect(l,14,77,1+212*scroll/HR_GAME_COURSE,4,0x7ABCF4,2);
 rect(l,222,96,5,62,0x213C61,2);
 int meter=g->height*60/HR_GAME_MAX_HEIGHT;if(meter<0)meter=0;if(meter>60)meter=60;
 rect(l,223,157-meter,3,meter,g->pitch_hz*100>=g->base_hz*165u?0xFFE290:0xA6DBFF,1);
 rect(l,52,293,136,24,0xEEF5FF,10);
 if(g->phase==HR_GAME_READY) {
  // Opaque card drawn into existing partial buffer: no transform/fade layer.
  rect(l,52,106,140,140,0x173B63,24);
  rect(l,50,100,140,140,0x0C2444,24);
  rect(l,85,112,70,4,0xB6D3EE,2);
 }
}
lv_obj_t *hr_game_scene_create(lv_obj_t*parent) {
 lv_obj_t*s=lv_obj_create(parent);lv_obj_remove_style_all(s);
 lv_obj_remove_flag(s,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_size(s,240,320);
 lv_obj_set_pos(s,0,0);lv_obj_add_event_cb(s,draw,LV_EVENT_DRAW_MAIN,NULL);
 lv_obj_add_flag(s,LV_OBJ_FLAG_HIDDEN);lv_obj_move_background(s);return s;
}
void hr_game_scene_update(lv_obj_t*scene,const hr_game_t*game,uint32_t now) {
 jump_pose=hr_jump_pose(&jump_animation,game,now);
 snapshot=*game;scene_now=now;lv_obj_remove_flag(scene,LV_OBJ_FLAG_HIDDEN);lv_obj_invalidate(scene);
}
