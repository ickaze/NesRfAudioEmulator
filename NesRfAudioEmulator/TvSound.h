#pragma once
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <array>
class TvSound {
 float lastTone=0;std::array<float,53> spread{};size_t spreadIndex=0;
 double phase=0;float sweep=0,drive=0,charge=0,release=0,pop=0,previousNoise=0,whineGain=0,staticGain=0;bool wasOn=false;uint32_t random=0x194014;
 float Noise(){random^=random<<13;random^=random>>17;random^=random<<5;return float(random>>8)/8388608.f-1;}
public:
 float Frequency()const{return 8500.f+(15734.26f-8500.f)*std::sqrt(sweep);}
 float Sample(bool on,float whine,float discharge,float deflection=1.f){
  if(wasOn&&!on)release=std::max(release,charge);wasOn=on;
  // Follow the same horizontal deflection used by the CRT image. Smooth
  // timer/block boundaries at sample rate; never reset phase on a reversal.
  sweep+=(std::clamp(deflection,0.f,1.f)-sweep)*.0026f;
  drive+=(sweep-drive)*.0026f;
  charge+=(float(on)-charge)*(on?.000035f:.000014f);
  release*=on?.998f:.999975f;
  whineGain+=(whine-whineGain)*.002f;staticGain+=(discharge-staticGain)*.002f;
  phase+=6.283185307179586*Frequency()/48000.;if(phase>6.283185307179586)phase-=6.283185307179586;
  float n=Noise();if(!on&&release>.0001f&&Noise()>1-release*.00016f)pop=std::min(1.f,pop+release*(.65f+.25f*Noise()));
  pop*=.994f;float high=n-previousNoise;previousNoise=n;
  lastTone=.012f*whineGain*drive*float(std::sin(phase));
  return lastTone+.42f*staticGain*(pop*high+release*.20f*n);
 }
 std::array<float,2> Stereo(bool on,float whine,float discharge,float deflection){
  float mono=Sample(on,whine,discharge,deflection),noise=mono-lastTone;
  // Stable all-pass: equal-energy phase dispersion, only on discharge audio.
  float right=spread[spreadIndex]-.6f*noise;spread[spreadIndex]=noise+.6f*right;spreadIndex=(spreadIndex+1)%spread.size();
  return {mono,lastTone+right};
 }
};
