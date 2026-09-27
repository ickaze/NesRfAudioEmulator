#include "RfWorker.h"
#include <chrono>
#include <thread>
#include <iostream>
#include <stdexcept>
void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
RfJob job(float mark,bool rf=false){RfJob j;j.video.assign(256*240,0xff3579acu);j.audio.assign(800,mark);j.params.enabled=rf;return j;}
bool wait(RfWorker& w,RfResult& r){for(int i=0;i<1000;++i){if(w.Take(r))return true;std::this_thread::sleep_for(std::chrono::milliseconds(1));}return false;}
int main(){try{
 RfWorker worker;for(int i=0;i<4;++i)check(worker.Submit(job(float(i))),"submit first 4");check(!worker.Submit(job(9)),"queue must be bounded");check(worker.Pending()<=4,"pending limit");
 for(int i=0;i<4;++i){RfResult r;check(wait(worker,r),"result timeout");check(r.audio.size()==800&&r.audio[0]==float(i),"FIFO/audio preservation");check(r.video[0]==0xff3579acu,"video preservation");}
 worker.Submit(job(.2f,true));worker.Flush();check(worker.Submit(job(.8f)),"submit after flush");RfResult result;check(wait(worker,result),"flush result timeout");check(result.audio[0]==.8f,"stale epoch returned");
 worker.SetCapacity(1);check(worker.Submit(job(.4f)),"low latency submit");
 check(!worker.CanSubmit()&&!worker.Submit(job(.5f)),"one frame limit includes processing and completed result");
 check(wait(worker,result)&&result.audio[0]==.4f,"low latency result");check(worker.CanSubmit(),"admission restored after take");
 worker.SetCapacity(4);for(int i=0;i<4;++i)check(worker.Submit(job(float(i))),"expanded queue");check(!worker.CanSubmit(),"expanded bound");
 worker.SetCapacity(1);check(!worker.CanSubmit(),"shrinking blocks admission");
 for(int i=0;i<4;++i)check(wait(worker,result)&&result.audio[0]==float(i),"resize preserves FIFO");
 check(worker.CanSubmit(),"shrunk queue ready");
 worker.Stop();check(!worker.Submit(job(0)),"stop admission");worker.Stop();
 std::cout<<"Queue bound, FIFO, epoch flush and idempotent stop OK\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
