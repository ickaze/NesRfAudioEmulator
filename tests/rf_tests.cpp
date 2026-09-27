#include "Rf.h"
#include <stdexcept>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <memory>
static void check(bool v,const char* text){if(!v)throw std::runtime_error(text);}
static double energy(const std::vector<float>& a){double r=0;for(float v:a){check(std::isfinite(v),"nonfinite sample");r+=double(v)*v;}return r/std::max(size_t(1),a.size());}
int main(){try{
 std::array<uint32_t,256*240> image{};
 for(int y=0;y<240;++y)for(int x=0;x<256;++x)image[y*256+x]=0xff000000u|(uint32_t(x)<<0)|(uint32_t(y)<<8)|(uint32_t((x+y)&255)<<16);
 std::vector<float> tone(800);for(size_t i=0;i<tone.size();++i)tone[i]=.15f*float(std::sin(6.283185307179586*1000.*i/48000.));
 RfProcessor digital;RfParams p;p.enabled=false;auto a=tone;digital.Process(image.data(),a,60.0988,p);
 check(a==tone,"digital audio changed");check(std::equal(image.begin(),image.end(),digital.Frame()),"digital video changed");
 p.enabled=true;p.signal=1;p.noise=0;p.contact=0;p.detune=0;p.ghost=0;p.mix=0;p.syncLeak=0;p.hJitter=0;p.colorJitter=0;p.cutoff=18000;
 RfProcessor clean,shift;auto q=p;q.hHold=.95f;q.vHold=.7f;
 for(int n=0;n<4;++n){a=tone;auto b=tone;clean.Process(image.data(),a,60.0988,p);shift.Process(image.data(),b,60.0988,q);check(a==b,"receiver-only sync changed sound with leakage disabled");}
 check(energy(a)>.004&&energy(a)<.025,"clean RF tone gain outside bounds");check(!std::equal(image.begin(),image.end(),clean.Frame()),"RF video not processed");check(!std::equal(clean.Frame(),clean.Frame()+image.size(),shift.Frame()),"sync offsets did not affect image");
 RfProcessor pictureA,pictureB;std::array<uint32_t,256*240> black{},white{};black.fill(0xff000000);white.fill(0xffffffff);q=p;q.mix=1;q.syncLeak=.2f;
 double difference=0;
 for(int n=0;n<3;++n){a.assign(800,0);std::vector<float>b(800);pictureA.Process(black.data(),a,60.0988,q);pictureB.Process(white.data(),b,60.0988,q);for(size_t i=0;i<a.size();++i)difference+=std::abs(a[i]-b[i]);}
 check(difference>.01,"picture brightness did not affect buzz");
 // Measure settled brightness leakage at the muffled-TV cutoff, with no hiss.
 double brightness[3]{};
 for(int level=0;level<3;++level){
  auto dark=std::make_unique<RfProcessor>(),bright=std::make_unique<RfProcessor>();
  q=p;q.mix=level*.5f;q.cutoff=6500;
  for(int frame=0;frame<12;++frame){
   a.assign(800,0);std::vector<float>b(800);dark->Process(black.data(),a,60.0988,q);bright->Process(white.data(),b,60.0988,q);
   if(frame>=4)for(size_t i=0;i<a.size();++i)brightness[level]+=(a[i]-b[i])*(a[i]-b[i]);
  }
  brightness[level]=std::sqrt(brightness[level]/6400.);
 }
 std::cout<<"Brightness difference RMS (mix 0/0.5/1): "<<brightness[0]<<" / "<<brightness[1]<<" / "<<brightness[2]<<"\n";
 check(brightness[0]<1e-7&&brightness[1]>.005&&brightness[2]>brightness[1]*1.5,"brightness leakage control too weak or non-monotonic");

 RfProcessor loss;q=p;q.contact=1;q.noise=.2f;q.recovery=.0003f;unsigned drops=0;double e=0;
 auto start=std::chrono::steady_clock::now();
 for(int n=0;n<120;++n){a.assign(800,0);loss.Process(image.data(),a,60.0988,q);drops+=loss.GetStatus().dropoutLines;e+=energy(a);}
 check(drops>0&&e>.00001,"contact impairment not reflected in audio/status");
 std::cout<<"120 RF frames: "<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<" ms\n";
 q.signal=0;q.detune=1;q.ghost=.8f;q.ghostDelay=48;q.bandwidth=1;q.hJitter=1;q.hHold=-1;q.vHold=-1;q.colorJitter=1;q.syncLeak=1;q.mix=1;q.cutoff=400;
 for(double fps:{50.,60.0988}){a=tone;loss.Process(image.data(),a,fps,q);check(a.size()==tone.size(),"sample count changed");energy(a);}
 auto interference=std::make_unique<RfInterference>();interference->active=true;interference->video.fill(0xff00ff00);interference->audio=tone;interference->videoLevel=.7f;interference->audioLevel=.8f;
 auto baseline=std::make_unique<RfProcessor>(),mixed=std::make_unique<RfProcessor>();
 q=p;a.assign(800,0);std::vector<float> mixedAudio(800);baseline->Process(image.data(),a,60.0988,q);mixed->Process(image.data(),mixedAudio,60.0988,q,interference.get());
 check(energy(mixedAudio)>energy(a)+.0001,"adjacent audio not mixed");check(!std::equal(baseline->Frame(),baseline->Frame()+image.size(),mixed->Frame()),"adjacent video not mixed");
 interference->active=false;baseline->Reset();mixed->Reset();a=tone;mixedAudio=tone;baseline->Process(image.data(),a,60.0988,q);mixed->Process(image.data(),mixedAudio,60.0988,q,interference.get());check(a==mixedAudio,"disabled adjacent source altered audio");
 interference->active=true;q.enabled=false;a=tone;mixed->Process(image.data(),a,60.0988,q,interference.get());check(a==tone,"digital adjacent bypass");
 q.enabled=false;a=tone;loss.Process(image.data(),a,60.0988,q);check(a==tone,"return to digital not transparent");check(std::equal(image.begin(),image.end(),loss.Frame()),"return to digital image not exact");
 std::cout<<"Digital bypass, clean tone, isolated sync, picture buzz, shared dropout, extremes and mode switch OK\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
