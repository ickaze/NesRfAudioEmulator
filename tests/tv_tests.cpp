#include "TvState.h"
#include <cassert>
#include <iostream>
#include <stdexcept>
static void check(bool v){if(!v)throw std::runtime_error("TV transition failed");}
int main(){TvState s;check(s.beam==0);for(int i=0;i<600;++i)s.Tick(1.f/60);check(s.beam>.98&&s.sound>.99);s.on=false;for(int i=0;i<8;++i)s.Tick(1.f/60);check(s.height<s.width&&s.beam>0&&s.sound<.5);float h=s.height,w=s.width,b=s.beam;s.on=true;check(s.height==h&&s.width==w&&s.beam==b);s.Tick(1.f/60);check(s.height>h&&s.width>w&&s.beam>=b);s.on=false;for(int i=0;i<900;++i)s.Tick(1.f/60);check(s.beam<1e-6&&s.sound<1e-6);s.on=true;for(int i=0;i<600;++i)s.Tick(1.f/60);check(s.beam>.98);std::cout<<"Warm-up, collapse, reversal, fade and reheat OK\n";}
