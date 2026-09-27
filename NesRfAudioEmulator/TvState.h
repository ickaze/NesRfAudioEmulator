#pragma once
#include <algorithm>
#include <cmath>
// Continuous state: switching power never resets temperature or deflection.
struct TvState {
 bool on=true; float heat=0, beam=0, width=0, height=0, sound=0;
 float contrast=1,brightness=0,hue=0,saturation=1;
 static float approach(float v,float target,float dt,float tau){return target+(v-target)*std::exp(-dt/tau);}
 void Tick(float dt){
  dt=std::clamp(dt,0.f,.1f);
  heat=approach(heat,on?1.f:0.f,dt,on?1.8f:7.f);
  width=approach(width,on?1.f:0.f,dt,on?.22f:.32f);
  height=approach(height,on?1.f:0.f,dt,on?.3f:.075f);
  float emission=on?std::clamp((heat-.18f)/.65f,0.f,1.f):0.f;
  beam=approach(beam,emission,dt,on?.25f:.48f);
  sound=approach(sound,on?1.f:0.f,dt,on?.45f:.14f);
 }
};
