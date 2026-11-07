#pragma once
/* Hand-authored low/middle/high humming guide, not a transcription of a song. */
static unsigned reference_voice(unsigned x) {
 if(x<145)return 180;
 if(x<205)return 225;
 if(x<305)return 250;
 if(x<362)return 330;
 if(x<465)return 280;
 if(x<560)return 220;
 if(x<630)return 245;
 if(x<822)return 330;
 if(x<875)return 220;
 if(x<1005)return 330;
 return 180;
}
