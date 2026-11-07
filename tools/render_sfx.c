/* Host preview of the exact production synthesizer; no firmware asset is loaded. */
#include "hr_sfx.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv) {
 if(argc!=3)return 2;
 unsigned kind=(unsigned)atoi(argv[1]),left=hr_sfx_samples(kind);
 FILE *f=fopen(argv[2],"wb");if(!f||!left)return 3;
 hr_sfx_player_t p={0};hr_sfx_begin(&p,kind);int16_t pcm[320];
 while(left){unsigned n=left>320?320:left;if(!hr_sfx_render(&p,pcm,n)||fwrite(pcm,2,n,f)!=n)return 4;left-=n;}
 return fclose(f);
}
