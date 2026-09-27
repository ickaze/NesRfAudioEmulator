#include "Rf.h"
#include <fstream>
#include <string>
#include <algorithm>
void ppm(const char* name,const uint32_t* data){std::ofstream f(name,std::ios::binary);f<<"P6\n256 240\n255\n";for(int i=0;i<256*240;++i){char rgb[]={char(data[i]&255),char((data[i]>>8)&255),char((data[i]>>16)&255)};f.write(rgb,3);}}
int main(){std::array<uint32_t,256*240> image;unsigned colors[]={0xffffffff,0xff00ffff,0xffffff00,0xff00ff00,0xffff00ff,0xff0000ff,0xffff0000,0xff101010};
for(int y=0;y<240;++y)for(int x=0;x<256;++x){uint32_t c=colors[x/32];if(y>100&&y<160)c=((x/4+y/4)%2)?0xffeeeeee:0xff111111;if(y>=160){int v=x;c=0xff000000|v|(v<<8)|(v<<16);if(y>210&&(x/12)%2==0)c=0xff0000ff;}image[y*256+x]=c;}ppm("digital.ppm",image.data());
for(int mode=0;mode<3;++mode){RfProcessor receiver;RfParams p;p.enabled=true;if(mode==1){p.signal=.6f;p.contact=.6f;p.noise=.12f;p.detune=.15f;p.recovery=.001f;}if(mode==2){p.signal=.5f;p.noise=.1f;p.hJitter=.2f;p.hHold=.75f;p.vHold=.2f;p.colorJitter=.35f;}
unsigned best=0;std::array<uint32_t,256*240> result;for(int i=0;i<45;++i){std::vector<float> a(800);receiver.Process(image.data(),a,60.0988,p);if(mode!=1||receiver.GetStatus().dropoutLines>=best){best=receiver.GetStatus().dropoutLines;std::copy(receiver.Frame(),receiver.Frame()+result.size(),result.begin());}}
std::string file="rf"+std::to_string(mode)+".ppm";ppm(file.c_str(),result.data());}}
