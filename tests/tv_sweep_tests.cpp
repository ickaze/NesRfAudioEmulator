#include "TvSound.h"
#include "TvState.h"
#include <stdexcept>
#include <cmath>
void check(bool x){if(!x)throw std::runtime_error("Sweep continuity failed");}
int main(){TvState tv;TvSound s;auto step=[&]{tv.Tick(.01f);for(int i=0;i<480;++i)check(std::isfinite(s.Sample(tv.on,1,0,tv.width)));};
 float last=s.Frequency();for(int k=0;k<150;++k){step();check(s.Frequency()>=last);last=s.Frequency();}check(last>15720);
 tv.on=false;for(int k=0;k<15;++k){step();check(s.Frequency()<last);last=s.Frequency();}
 float before=s.Frequency();tv.on=true;check(s.Frequency()==before);for(int k=0;k<80;++k)step();check(s.Frequency()>before);
 tv.on=false;double tail=0;for(int k=0;k<500;++k)step();for(int i=0;i<48000;++i){auto x=s.Sample(false,1,0,0);tail+=x*x;}check(tail<1e-6);check(s.Frequency()<8510);
}
