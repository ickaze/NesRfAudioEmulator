#include "TvSound.h"
#include <stdexcept>
#include <iostream>
void check(bool v){if(!v)throw std::runtime_error("Stereo discharge test failed");}
int main(){TvSound sound;for(int n=0;n<240000;++n)sound.Stereo(true,0,1,1);double l=0,r=0,lr=0,mono=0;
 for(int n=0;n<96000;++n){auto p=sound.Stereo(false,0,1,0);check(std::isfinite(p[0])&&std::isfinite(p[1]));l+=p[0]*p[0];r+=p[1]*p[1];lr+=p[0]*p[1];mono+=(p[0]+p[1])*(p[0]+p[1]);}
 check(l>.01&&r/l>.9&&r/l<1.1);check(std::abs(lr/std::sqrt(l*r))<.95);check(mono>l*.1);
 TvSound whine;for(int n=0;n<48000;++n){auto p=whine.Stereo(true,1,0,1);check(p[0]==p[1]);}
 std::cout<<"Stereo energy, phase spread, mono compatibility and centered whine OK\n";
}
