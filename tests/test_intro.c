#include "hr_intro.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
 hr_intro_t s;
 hr_intro_init(&s,false,0);assert(s.kind==HR_INTRO_BOOT);
 assert(!hr_intro_tick(&s,1399,true));
 assert(hr_intro_tick(&s,1400,false)==HR_INTRO_START_AUDIO);
 assert(s.kind==HR_INTRO_WELCOME);
 assert(!hr_intro_tick(&s,6599,true));
 assert(hr_intro_tick(&s,6600,true)==HR_INTRO_SAVE_WELCOME);
 assert(s.kind==HR_INTRO_NONE&&s.welcomed);
 assert(!hr_intro_tick(&s,9000,true));
 hr_intro_init(&s,true,10000);
 assert(!hr_intro_tick(&s,11400,false)&&s.kind==HR_INTRO_NONE);
 // Failed/missing audio times out without falsely marking the device welcomed.
 hr_intro_init(&s,false,0);hr_intro_tick(&s,1400,false);
 assert(!hr_intro_tick(&s,8899,false)&&s.kind==HR_INTRO_WELCOME);
 assert(!hr_intro_tick(&s,8900,false)&&s.kind==HR_INTRO_NONE&&!s.welcomed);
 hr_intro_init(&s,false,0xffffff00u);
 assert(hr_intro_tick(&s,0xffffff00u+1400u,false)==HR_INTRO_START_AUDIO);
 assert(hr_intro_tick(&s,0xffffff00u+6600u,true)==HR_INTRO_SAVE_WELCOME);
 puts("PASS once welcome, minimum display, audio completion, failure timeout and wraparound");
}
