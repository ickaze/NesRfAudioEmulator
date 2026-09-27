#include "TvSound.h"
#include <cassert>
int main(){TvSound a,b;double on=0,off=0,tail=0;for(int i=0;i<240000;++i){float x=a.Sample(true,1,0);if(i>192000)on+=x*x;b.Sample(true,0,1);}for(int i=0;i<960000;++i){float x=a.Sample(false,1,0,0),y=b.Sample(false,0,1);if(i<48000)off+=y*y;if(i>912000)tail+=y*y;assert(std::isfinite(x)&&std::isfinite(y));}assert(on>1&&off>.01&&tail<off*.01);TvSound silent;for(int i=0;i<10000;++i)assert(silent.Sample(i<5000,0,0)==0);}
