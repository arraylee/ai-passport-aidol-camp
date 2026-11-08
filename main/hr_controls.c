#include "hr_controls.h"
void hr_view_home(hr_view_t *v,uint32_t now) {
 hr_social_leave(&v->core);hr_room_join(&v->core,false,now);
 v->page=HR_PAGE_HOME;v->home_selection=1;v->card_from_pair=false;v->room_joining=false;
 v->volume_mode=false;v->gallery_return=false;
}
unsigned hr_room_choices(const hr_view_t*v,uint32_t now,uint8_t items[9]) {
 uint8_t peers[HR_PLAYERS];unsigned peers_n=0,n=0;
 for(unsigned id=1;id<=HR_PLAYERS;id++)
  if(id!=v->core.id && hr_room_member(&v->core,id,now))peers[peers_n++]=id;
 if(peers_n)items[n++]=0;
 items[n++]=HR_ROOM_GAME;items[n++]=HR_ROOM_BACK;
 for(unsigned i=0;i<peers_n;i++)items[n++]=peers[i];
 return n;
}
static void next_item(uint8_t *selected,const uint8_t *items,unsigned n,int dir) {
 unsigned at=0;while(at<n && items[at]!=*selected)at++;
 if(at==n)at=0;else at=(at+n+dir)%n;
 *selected=items[at];
}
static void game_menu(hr_view_t*v,uint32_t now) {
 hr_release(&v->core,now);v->core.game_busy=true;v->core.next_hello=now;
 v->page=HR_PAGE_GAME_MENU;v->game_selection=0;
}
static void game_return_room(hr_view_t*v,uint32_t now) {
 v->page=HR_PAGE_RADIO;v->core.game_busy=false;v->core.next_hello=now;
 v->selection=HR_ROOM_GAME;
}
int hr_handle_key(hr_view_t *v,int event,uint32_t now) {
 if(v->intro_kind)return HR_EFFECT_NONE;
 if(v->activation_required)return !v->phone_connected && event==HR_KEY_DOWN?HR_EFFECT_ACTIVATE:HR_EFFECT_NONE;
 if(v->room_joining) {
  if(v->page!=HR_PAGE_LOBBY || v->phone_connected)v->room_joining=false;
  else if((uint32_t)(now-v->room_join_started)>=HR_ROOM_JOIN_MS) {
   v->room_joining=false;hr_room_join(&v->core,true,now);v->page=HR_PAGE_RADIO;
   v->selection=0;v->room_empty=true;event=HR_KEY_NONE;
  }
 }
 if(v->page==HR_PAGE_LOBBY && !v->room_joining && !v->phone_connected) {
  v->room_joining=true;v->room_join_started=now;
 }
 if(v->page==HR_PAGE_PAIR && v->core.social.complete && !v->core.social.failed && v->core.social.candidate) {
  v->card_id=v->core.social.candidate;v->card_from_pair=true;v->page=HR_PAGE_CARD;event=HR_KEY_NONE;
 }
 if(v->page==HR_PAGE_GAME_PLAY && v->game.phase>=HR_GAME_CLEAR && !v->game_scored) {
  v->game_scored=true;
  v->new_best=!v->core.best_valid || v->game.score>v->core.best_score;
  if(v->new_best){v->core.best_score=v->game.score;v->core.best_valid=true;v->best_dirty=true;}
 }
 if(v->page==HR_PAGE_GAME_PLAY && v->game.phase==HR_GAME_DONE) {
  v->page=HR_PAGE_GAME_RESULT;v->result_selection=0;
  event=HR_KEY_NONE;
 }
 uint8_t options[9];unsigned count=0,peers=0;
 if(v->page==HR_PAGE_RADIO) {
  count=hr_room_choices(v,now,options);peers=count>2?count-3:0;
  bool found=false;for(unsigned i=0;i<count;i++)if(options[i]==v->selection)found=true;
  if(!v->core.held && !v->up_pressed && event!=HR_KEY_UP_HOLD && !found)v->selection=options[0];
  v->room_empty=peers==0;
 }
 if(event==HR_KEY_NONE)return HR_EFFECT_NONE;
 if(event==HR_KEY_END){hr_release(&v->core,now);return HR_EFFECT_NONE;}
 if(v->core.held)return HR_EFFECT_NONE;
 if(v->setup) {
  if(v->phone_connected)return HR_EFFECT_NONE;
  if(event==HR_KEY_UP)v->choice=v->choice<=1?HR_PLAYERS:v->choice-1;
  if(event==HR_KEY_CLICK)v->choice=v->choice>=HR_PLAYERS?1:v->choice+1;
  if(event==HR_KEY_DOWN_HOLD && v->core.id)v->setup=false;
  if(event==HR_KEY_DOWN && v->choice)return HR_EFFECT_SAVE_ID;
  return HR_EFFECT_NONE;
 }
 if(v->volume_mode) {
  if(event==HR_KEY_DOWN || event==HR_KEY_DOWN_HOLD)v->volume_mode=false;
  if(event==HR_KEY_UP || event==HR_KEY_CLICK) {
   int n=v->volume+(event==HR_KEY_UP?10:-10);v->volume=n<0?0:n>100?100:n;
   return HR_EFFECT_VOLUME_CHANGED;
  }
  return HR_EFFECT_NONE;
 }
 if(v->page==HR_PAGE_GALLERY) {
  if(!v->gallery_return)v->gallery_return=true;
  else if(event==HR_KEY_DOWN)hr_view_home(v,now);
  else if(event==HR_KEY_UP || event==HR_KEY_CLICK || event==HR_KEY_UP_HOLD || event==HR_KEY_CENTER_HOLD)v->gallery_return=false;
  return HR_EFFECT_NONE;
 }
 if(event==HR_KEY_DOWN_HOLD){hr_view_home(v,now);return HR_EFFECT_NONE;}
 if(v->page==HR_PAGE_RADIO) {
  if(event==HR_KEY_UP_HOLD && peers && v->selection<=HR_PLAYERS)hr_press(&v->core,v->selection,now);
  if(event==HR_KEY_DOWN) {
   if(v->selection==HR_ROOM_GAME)game_menu(v,now);
   else if(v->selection==HR_ROOM_BACK)hr_view_home(v,now);
   else v->volume_mode=true;
  }
  if(event==HR_KEY_UP || event==HR_KEY_CLICK)next_item(&v->selection,options,count,event==HR_KEY_CLICK?1:-1);
 } else if(v->page==HR_PAGE_HOME || v->page==HR_PAGE_MENU) {
  if(event==HR_KEY_UP || event==HR_KEY_CLICK)v->home_selection=(v->home_selection+(event==HR_KEY_CLICK?1:2))%3;
  if(event==HR_KEY_DOWN) {
   hr_social_leave(&v->core);
   v->page=v->home_selection==2?HR_PAGE_GALLERY:v->home_selection==1?HR_PAGE_LOBBY:HR_PAGE_FRIENDS;
   if(v->page==HR_PAGE_LOBBY) {
    hr_room_join(&v->core,false,now);v->room_joining=!v->phone_connected;v->room_join_started=now;
   }
   v->gallery_return=false;
   if(v->page==HR_PAGE_FRIENDS) {
    v->friend_selection=0;for(unsigned id=1;id<=HR_PLAYERS;id++)if(v->core.social.friends&(1u<<id)){v->friend_selection=id;break;}
    if(!v->phone_connected && v->core.hardware_ok) {
     hr_social_enter(&v->core);v->core.social.manual=true;
    }
   }
  }
  if(event==HR_KEY_CENTER_HOLD && !v->phone_connected){v->setup=true;v->choice=v->core.id;v->save_failed=false;}
 } else if(v->page==HR_PAGE_PAIR) {
  if(event==HR_KEY_DOWN)hr_social_confirm(&v->core,now);
 } else if(v->page==HR_PAGE_CARD) {
  if(event==HR_KEY_DOWN)hr_view_home(v,now);
 } else if(v->page==HR_PAGE_FRIENDS) {
  count=0;for(unsigned id=1;id<=HR_PLAYERS;id++)if(v->core.social.friends&(1u<<id))options[count++]=id;
  options[count++]=0;
  if(event==HR_KEY_UP || event==HR_KEY_CLICK)next_item(&v->friend_selection,options,count,event==HR_KEY_CLICK?1:-1);
  if(event==HR_KEY_DOWN) {
   if(!v->friend_selection)hr_view_home(v,now);
   else {hr_social_leave(&v->core);v->card_from_pair=false;v->card_id=v->friend_selection;v->page=HR_PAGE_CARD;}
  }
 } else if(v->page==HR_PAGE_GAME_MENU) {
  if(event==HR_KEY_UP || event==HR_KEY_CLICK)v->game_selection=(v->game_selection+(event==HR_KEY_CLICK?1:2))%3;
  if(event==HR_KEY_DOWN) {
   if(v->game_selection==0) {
    if(!v->phone_connected && v->core.hardware_ok){hr_game_start(&v->game,now);v->page=HR_PAGE_GAME_PLAY;v->new_best=false;v->game_scored=false;}
   }
   else if(v->game_selection==1)v->page=HR_PAGE_RANK;
   else game_return_room(v,now);
  }
 } else if(v->page==HR_PAGE_GAME_PLAY) {
  if(event==HR_KEY_DOWN)game_menu(v,now);
 } else if(v->page==HR_PAGE_GAME_RESULT) {
  if(event==HR_KEY_UP || event==HR_KEY_CLICK)v->result_selection=(v->result_selection+(event==HR_KEY_CLICK?1:2))%3;
  if(event==HR_KEY_DOWN) {
   if(!v->result_selection) {
    if(!v->phone_connected && v->core.hardware_ok){hr_game_start(&v->game,now);v->page=HR_PAGE_GAME_PLAY;v->new_best=false;v->game_scored=false;}
   }
   else if(v->result_selection==1)v->page=HR_PAGE_RANK;
   else game_return_room(v,now);
  }
 } else if(v->page==HR_PAGE_RANK && event==HR_KEY_DOWN)game_return_room(v,now);
 return HR_EFFECT_NONE;
}
