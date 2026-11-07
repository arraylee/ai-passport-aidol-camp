#include "hr_welcome.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char**argv) {
 assert(argc==2); FILE*f=fopen(argv[1],"rb");assert(f);
 uint8_t bytes[37644];assert(fread(bytes,1,sizeof bytes,f)==sizeof bytes);fclose(f);
 hr_welcome_t s;assert(hr_welcome_begin(&s,bytes,sizeof bytes));
 int16_t full[75264],block[HR_SAMPLES];size_t total=0,n;
 while((n=hr_welcome_next(&s,block))) {assert(total+n<=75264);memcpy(full+total,block,n*2);total+=n;for(size_t k=n;k<HR_SAMPLES;k++)assert(!block[k]);}
 assert(total==75264&&hr_crc(full,sizeof full)==0x55eeebe8u);
 assert(!hr_welcome_next(&s,block));
 for(unsigned i=0;i<12;i++){uint8_t old=bytes[i];bytes[i]^=0x80;assert(!hr_welcome_begin(&s,bytes,sizeof bytes));bytes[i]=old;}
 assert(!hr_welcome_begin(&s,bytes,sizeof bytes-1));
 assert(!hr_welcome_begin(&s,NULL,0));assert(!hr_welcome_next(&s,block));
 puts("PASS original Aidol audio: 75264 bit-identical decoded samples, bounded 320-sample frames, invalid headers and EOF");
}
