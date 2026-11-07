#include "hr_controls.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
 hr_view_t v={.page=HR_PAGE_RADIO,.selection=3,.volume=90};uint8_t mac[6]={1};
 hr_init(&v.core,1,mac,123,0);v.core.hardware_ok=true;
 for(unsigned id=2;id<=6;id++){v.core.peers[id].seen=true;v.core.peers[id].last=1000;}
 hr_handle_key(&v,HR_KEY_UP_HOLD,1000);assert(v.core.held&&v.core.target==3);
 hr_handle_key(&v,HR_KEY_CLICK,1001);hr_handle_key(&v,HR_KEY_DOWN,1002);assert(v.selection==3&&!v.volume_mode);
 hr_handle_key(&v,HR_KEY_END,1003);assert(!v.core.held);
 hr_handle_key(&v,HR_KEY_UP,1004);assert(v.selection==2);
 hr_handle_key(&v,HR_KEY_UP,1005);assert(v.selection==HR_ROOM_BACK);
 hr_handle_key(&v,HR_KEY_UP,1006);assert(v.selection==HR_ROOM_GAME);
 hr_handle_key(&v,HR_KEY_UP,1007);assert(v.selection==0);
 hr_handle_key(&v,HR_KEY_UP,1008);assert(v.selection==6);
 hr_handle_key(&v,HR_KEY_DOWN,1009);assert(v.volume_mode);
 assert(hr_handle_key(&v,HR_KEY_CLICK,1010)==HR_EFFECT_VOLUME_CHANGED&&v.volume==80);
 hr_handle_key(&v,HR_KEY_DOWN,1011);assert(!v.volume_mode);
 hr_handle_key(&v,HR_KEY_DOWN_HOLD,1012);assert(v.page==HR_PAGE_HOME);
 hr_handle_key(&v,HR_KEY_CENTER_HOLD,1013);assert(v.setup&&v.choice==1);
 hr_handle_key(&v,HR_KEY_CLICK,1014);assert(v.choice==2);
 assert(hr_handle_key(&v,HR_KEY_DOWN,1015)==HR_EFFECT_SAVE_ID);
 hr_handle_key(&v,HR_KEY_DOWN_HOLD,1016);assert(!v.setup);
 // A private target disappearing while held never becomes ALL.
 hr_init(&v.core,2,mac,200,2000);v.core.hardware_ok=true;v.page=HR_PAGE_RADIO;v.selection=3;
 v.core.peers[1].seen=true;v.core.peers[1].last=2000;
 hr_handle_key(&v,HR_KEY_UP_HOLD,2001);assert(v.core.held&&v.core.blocked&&v.core.target==3);
 hr_handle_key(&v,HR_KEY_END,2002);hr_handle_key(&v,HR_KEY_NONE,2003);assert(v.selection==0);
 hr_handle_key(&v,HR_KEY_CLICK,2004);assert(v.selection==HR_ROOM_GAME);
 hr_handle_key(&v,HR_KEY_CLICK,2005);assert(v.selection==HR_ROOM_BACK);
 hr_handle_key(&v,HR_KEY_CLICK,2006);assert(v.selection==1);
 v.selection=0;hr_handle_key(&v,HR_KEY_CLICK,2007);assert(v.selection==HR_ROOM_GAME);
 hr_handle_key(&v,HR_KEY_DOWN,2008);assert(v.page==HR_PAGE_GAME_MENU);
 v.game_selection=2;hr_handle_key(&v,HR_KEY_DOWN,2009);assert(v.page==HR_PAGE_RADIO&&v.selection==HR_ROOM_GAME);
 hr_handle_key(&v,HR_KEY_CLICK,2010);assert(v.selection==HR_ROOM_BACK);
 hr_handle_key(&v,HR_KEY_DOWN,2011);assert(v.page==HR_PAGE_HOME);
 puts("PASS target-lock, offline private isolation, room action navigation, volume and setup");
}
