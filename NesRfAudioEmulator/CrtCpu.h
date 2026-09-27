#pragma once
#include "TvState.h"
#include <array>
#include <cstdint>
// Used only when the CRT pixel shader cannot be compiled/created.
inline void CrtCpu(const uint32_t* input,uint32_t* output,const TvState& tv){
 auto smooth=[](float a,float b,float v){float t=std::clamp((v-a)/(b-a),0.f,1.f);return t*t*(3-2*t);};
 float ex=std::max(tv.width,.002f),ey=std::max(tv.height,.002f);
 float cs=std::cos(tv.hue),sn=std::sin(tv.hue),squeeze=std::clamp(1-tv.height,0.f,1.f);
 for(int y=0;y<240;++y)for(int x=0;x<256;++x){
  float qx=(x+.5f)/128-1,qy=(y+.5f)/120-1;
  float vx=qx/ex,vy=qy/ey,k=1+.035f*(vx*vx+vy*vy);vx*=k;vy*=k;
  float edge=(1-smooth(.96f,1.02f,std::abs(vx)))*(1-smooth(.96f,1.02f,std::abs(vy)));
  int sx=std::clamp(int((vx*.5f+.5f)*256),0,255),sy=std::clamp(int((vy*.5f+.5f)*240),0,239);auto p=input[sy*256+sx];
  float r=(p&255)/255.f,g=((p>>8)&255)/255.f,b=((p>>16)&255)/255.f;
  float lum=.299f*r+.587f*g+.114f*b,i=.596f*r-.274f*g-.322f*b,j=.211f*r-.523f*g+.312f*b;
  float a=(i*cs-j*sn)*tv.saturation,c=(i*sn+j*cs)*tv.saturation;
  float rgb[3]={lum+.956f*a+.621f*c,lum-.272f*a-.647f*c,lum-1.106f*a+1.703f*c};
  float glass=1-smooth(.94f,1.f,std::pow(std::abs(qx),8)+std::pow(std::abs(qy),8));
  float gx=qx/std::max(.008f,ex),gy=qy/std::max(.008f,ey);
  float glow=std::exp(-gx*gx-gy*gy)*tv.beam*squeeze*.13f;
  float light=tv.beam*(1+2*squeeze)*edge*(.95f+.05f*std::cos(vy*240*3.14159265f));
  const float white[3]={1,.92f,.79f},base[3]={.018f,.024f,.022f};uint32_t pixel=0xff000000;
  for(int ch=0;ch<3;++ch){float v=std::clamp((rgb[ch]-.5f)*tv.contrast+.5f+tv.brightness,0.f,1.f);v+=(white[ch]-v)*squeeze*.72f;
   v=(base[ch]*(1-.35f*(qx*qx+qy*qy))+v*light+glow)*glass;
   pixel|=uint32_t(std::clamp(v,0.f,1.f)*255+.5f)<<(ch*8);
  }output[y*256+x]=pixel;
 }
}
