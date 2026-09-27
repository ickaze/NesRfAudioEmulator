#include "CrtCpu.h"
#include <stdexcept>
#include <iostream>
static void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
int main(){
 std::array<uint32_t,256*240> src{},a{},b{};src.fill(0xff204080);TvState t;t.heat=t.beam=t.width=t.height=t.sound=1;
 CrtCpu(src.data(),a.data(),t);auto center=a[120*256+128];check((center&255)>((center>>16)&255),"RGB order");
 t.saturation=0;CrtCpu(src.data(),b.data(),t);auto gray=b[120*256+128];check(std::abs(int(gray&255)-int((gray>>16)&255))<=3,"saturation");
 t.brightness=.25;CrtCpu(src.data(),b.data(),t);check((b[120*256+128]&255)>(gray&255),"brightness");
 t.on=false;for(int i=0;i<15;++i)t.Tick(1.f/60);CrtCpu(src.data(),b.data(),t);
 check((b[120*256+128]&255)>(b[60*256+128]&255),"collapse toward center");
 for(int i=0;i<900;++i)t.Tick(1.f/60);CrtCpu(src.data(),b.data(),t);check((b[120*256+128]&255)<10,"off glow expired");
 t.on=true;for(int i=0;i<600;++i)t.Tick(1.f/60);CrtCpu(src.data(),b.data(),t);check((b[120*256+128]&255)>30,"reheat");
 std::cout<<"CPU CRT color, brightness, collapse, extinction and reheat OK\n";
}
