#include "hr_controls.h"
#include <assert.h>
#include <stdio.h>
static void peer(hr_view_t*v,unsigned id,unsigned now) {
 v->core.peers[id].seen=true;v->core.peers[id].last=now;
 v->core.peers[id].hello_at=now;v->core.peers[id].social_flags=16;
}
static void default_team(void) {
 hr_view_t v={0};uint8_t mac[6]={2,0,0,0,0,1};
 hr_init(&v.core,1,mac,10,0);hr_room_enable(&v.core,0);
 hr_view_home(&v,0);assert(v.page==HR_PAGE_HOME&&v.home_selection==1);
 hr_handle_key(&v,HR_KEY_DOWN,1);assert(v.page==HR_PAGE_LOBBY&&v.room_joining);
 hr_handle_key(&v,HR_KEY_DOWN_HOLD,2);assert(v.page==HR_PAGE_HOME&&v.home_selection==1);
 hr_handle_key(&v,HR_KEY_UP,3);assert(!v.home_selection);
 hr_handle_key(&v,HR_KEY_DOWN,4);assert(v.page==HR_PAGE_FRIENDS);
 hr_handle_key(&v,HR_KEY_DOWN,5);assert(v.page==HR_PAGE_HOME&&v.home_selection==1);
 hr_handle_key(&v,HR_KEY_CLICK,6);assert(v.home_selection==2);
 hr_handle_key(&v,HR_KEY_DOWN,7);assert(v.page==HR_PAGE_GALLERY);
 hr_handle_key(&v,HR_KEY_DOWN,8);assert(v.gallery_return);
 hr_handle_key(&v,HR_KEY_DOWN,9);assert(v.page==HR_PAGE_HOME&&v.home_selection==1);
 hr_handle_key(&v,HR_KEY_DOWN,10);assert(v.page==HR_PAGE_LOBBY&&v.room_joining);
}
int main(void) {
 default_team();
 hr_view_t v={.volume=90,.page=HR_PAGE_HOME};uint8_t m[6]={1};
 hr_init(&v.core,1,m,11,0);hr_room_enable(&v.core,0);v.core.hardware_ok=true;
 v.activation_required=true;
 for(int event=HR_KEY_NONE;event<=HR_KEY_END;event++)assert(hr_handle_key(&v,event,0)==(event==HR_KEY_DOWN?HR_EFFECT_ACTIVATE:HR_EFFECT_NONE));
 v.activation_required=false;v.core.social.friends=1u<<2;
 hr_handle_key(&v,HR_KEY_DOWN,1);assert(v.page==HR_PAGE_FRIENDS&&v.friend_selection==2);
 hr_handle_key(&v,HR_KEY_DOWN,2);assert(v.page==HR_PAGE_CARD);
 hr_handle_key(&v,HR_KEY_DOWN,3);assert(v.page==HR_PAGE_HOME);
 v.home_selection=0;hr_handle_key(&v,HR_KEY_DOWN,4);hr_handle_key(&v,HR_KEY_CLICK,5);assert(!v.friend_selection);
 hr_handle_key(&v,HR_KEY_DOWN,6);assert(v.page==HR_PAGE_HOME);
 v.home_selection=1;hr_handle_key(&v,HR_KEY_DOWN,10);
 assert(v.page==HR_PAGE_LOBBY&&v.room_joining&&!v.core.room_joined);
 hr_handle_key(&v,HR_KEY_NONE,10+HR_ROOM_JOIN_MS-1);assert(v.room_joining);
 hr_handle_key(&v,HR_KEY_NONE,10+HR_ROOM_JOIN_MS);assert(v.page==HR_PAGE_RADIO&&v.core.room_joined);
 hr_handle_key(&v,HR_KEY_NONE,2000);assert(v.selection==HR_ROOM_GAME);
 hr_handle_key(&v,HR_KEY_UP_HOLD,2001);assert(!v.core.held);
 peer(&v,2,2100);v.selection=2;hr_handle_key(&v,HR_KEY_UP_HOLD,2100);assert(v.core.held&&v.core.target==2);
 hr_handle_key(&v,HR_KEY_DOWN,2101);assert(!v.volume_mode);
 hr_handle_key(&v,HR_KEY_END,2102);hr_handle_key(&v,HR_KEY_DOWN,2103);assert(v.volume_mode);
 hr_handle_key(&v,HR_KEY_CLICK,2104);assert(v.volume==80);
 hr_handle_key(&v,HR_KEY_DOWN,2105);assert(!v.volume_mode);
 v.selection=HR_ROOM_GAME;hr_handle_key(&v,HR_KEY_DOWN,2200);assert(v.page==HR_PAGE_GAME_MENU&&v.core.game_busy&&v.core.room_joined);
 v.phone_connected=true;hr_handle_key(&v,HR_KEY_DOWN,2210);assert(v.page==HR_PAGE_GAME_MENU&&v.core.game_busy);
 v.phone_connected=false;v.core.hardware_ok=false;hr_handle_key(&v,HR_KEY_DOWN,2220);assert(v.page==HR_PAGE_GAME_MENU);
 v.core.hardware_ok=true;
 hr_handle_key(&v,HR_KEY_DOWN,2300);assert(v.page==HR_PAGE_GAME_PLAY);
 v.game.phase=HR_GAME_DONE; // Completion navigation is independent of a wall-clock deadline.
 v.game.score=120;hr_handle_key(&v,HR_KEY_NONE,26000);
 assert(v.page==HR_PAGE_GAME_RESULT&&v.new_best&&v.best_dirty&&v.core.best_score==120);
 v.best_dirty=false;v.phone_connected=true;hr_handle_key(&v,HR_KEY_DOWN,26050);assert(v.page==HR_PAGE_GAME_RESULT);
 v.phone_connected=false;hr_handle_key(&v,HR_KEY_DOWN,26100);assert(v.page==HR_PAGE_GAME_PLAY);
 v.game.phase=HR_GAME_DONE;v.game.score=100;hr_handle_key(&v,HR_KEY_NONE,26200);
 assert(!v.new_best&&!v.best_dirty&&v.core.best_score==120);
 hr_handle_key(&v,HR_KEY_CLICK,26300);assert(v.result_selection==1);
 hr_handle_key(&v,HR_KEY_DOWN,26400);assert(v.page==HR_PAGE_RANK&&v.core.game_busy&&v.core.room_joined);
 uint8_t ranks[HR_PLAYERS];assert(hr_ranked(&v.core,26400,ranks)==1&&ranks[0]==v.core.id&&v.core.best_score==120);
 hr_handle_key(&v,HR_KEY_DOWN,26500);
 assert(v.page==HR_PAGE_RADIO&&!v.core.game_busy&&v.core.room_joined&&v.core.next_hello==26500);
 // Result selection wraps over all three actions in both directions.
 v.page=HR_PAGE_GAME_RESULT;v.core.game_busy=true;v.result_selection=0;
 hr_handle_key(&v,HR_KEY_CLICK,26501);assert(v.result_selection==1);
 hr_handle_key(&v,HR_KEY_CLICK,26502);assert(v.result_selection==2);
 hr_handle_key(&v,HR_KEY_CLICK,26503);assert(v.result_selection==0);
 hr_handle_key(&v,HR_KEY_UP,26504);assert(v.result_selection==2);
 hr_handle_key(&v,HR_KEY_DOWN,26505);assert(v.page==HR_PAGE_RADIO&&!v.core.game_busy&&v.core.room_joined);
 v.selection=HR_ROOM_GAME;hr_handle_key(&v,HR_KEY_DOWN,26506);
 v.game_selection=1;hr_handle_key(&v,HR_KEY_DOWN,26507);assert(v.page==HR_PAGE_RANK&&v.core.game_busy);
 hr_handle_key(&v,HR_KEY_DOWN,26508);assert(v.page==HR_PAGE_RADIO&&!v.core.game_busy&&v.core.room_joined);
 v.selection=HR_ROOM_GAME;hr_handle_key(&v,HR_KEY_DOWN,26509);assert(v.page==HR_PAGE_GAME_MENU);
 // Completion is saved before the celebration ends, even when returning early.
 v.game_selection=0;hr_handle_key(&v,HR_KEY_DOWN,26510);v.game.phase=HR_GAME_CLEAR;v.game.score=200;
 hr_handle_key(&v,HR_KEY_DOWN,26520);assert(v.page==HR_PAGE_GAME_MENU&&v.core.best_score==200&&v.best_dirty);
 v.best_dirty=false;hr_handle_key(&v,HR_KEY_NONE,26530);assert(v.core.best_score==200&&!v.best_dirty);
 v.game_selection=2;hr_handle_key(&v,HR_KEY_DOWN,26600);assert(v.page==HR_PAGE_RADIO&&!v.core.game_busy&&v.core.room_joined);
 v.selection=HR_ROOM_BACK;hr_handle_key(&v,HR_KEY_DOWN,26700);assert(v.page==HR_PAGE_HOME&&!v.core.room_joined);
 // Joining resumes automatically after a phone connection is closed.
 hr_handle_key(&v,HR_KEY_DOWN,26800);v.phone_connected=true;hr_handle_key(&v,HR_KEY_NONE,26810);assert(!v.room_joining);
 v.phone_connected=false;hr_handle_key(&v,HR_KEY_NONE,26820);assert(v.room_joining&&v.room_join_started==26820);
 hr_handle_key(&v,HR_KEY_DOWN_HOLD,26821);
 // Reentry repeats animation without another confirmation.
 hr_handle_key(&v,HR_KEY_DOWN,27000);assert(v.room_joining);
 hr_handle_key(&v,HR_KEY_DOWN_HOLD,27001);hr_handle_key(&v,HR_KEY_NONE,30000);assert(v.page==HR_PAGE_HOME&&!v.core.room_joined);
 v.home_selection=2;hr_handle_key(&v,HR_KEY_DOWN,31000);assert(v.page==HR_PAGE_GALLERY&&!v.gallery_return);
 hr_handle_key(&v,HR_KEY_DOWN,31001);assert(v.gallery_return&&v.page==HR_PAGE_GALLERY);
 hr_handle_key(&v,HR_KEY_CLICK,31002);assert(!v.gallery_return);
 hr_handle_key(&v,HR_KEY_UP_HOLD,31003);assert(v.gallery_return);
 hr_handle_key(&v,HR_KEY_END,31004);assert(v.gallery_return&&v.page==HR_PAGE_GALLERY);
 hr_handle_key(&v,HR_KEY_UP,31005);assert(!v.gallery_return);
 hr_handle_key(&v,HR_KEY_DOWN_HOLD,31006);assert(v.gallery_return);
 hr_handle_key(&v,HR_KEY_DOWN,31007);assert(v.page==HR_PAGE_HOME);
 v.core.social.friends=0;v.home_selection=0;hr_handle_key(&v,HR_KEY_DOWN,32000);
 assert(v.page==HR_PAGE_FRIENDS&&!v.friend_selection);hr_handle_key(&v,HR_KEY_DOWN,32001);assert(v.page==HR_PAGE_HOME);
 puts("PASS auto-join/rejoin/cancel, explicit returns, photo confirm/cancel, game busy/menu/rank/result/high score");
}
