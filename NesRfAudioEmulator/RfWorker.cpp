#include "RfWorker.h"
#include <chrono>
#include <algorithm>
RfWorker::RfWorker():thread(&RfWorker::Run,this){}
RfWorker::~RfWorker(){Stop();}
void RfWorker::SetCapacity(size_t n){std::lock_guard<std::mutex> l(mutex);capacity=std::max(size_t(1),n);}
bool RfWorker::CanSubmit(){std::lock_guard<std::mutex> l(mutex);return !stopping&&jobs.size()+results.size()+inFlight<capacity;}
bool RfWorker::Submit(RfJob job){std::lock_guard<std::mutex> l(mutex);if(stopping||jobs.size()+results.size()+inFlight>=capacity)return false;jobs.push_back({std::move(job),epoch});condition.notify_one();return true;}
bool RfWorker::Take(RfResult& r){std::lock_guard<std::mutex> l(mutex);if(results.empty())return false;r=std::move(results.front());results.pop_front();return true;}
size_t RfWorker::Pending(){std::lock_guard<std::mutex> l(mutex);return jobs.size()+results.size()+inFlight;}
void RfWorker::Flush(){std::lock_guard<std::mutex> l(mutex);++epoch;jobs.clear();results.clear();}
void RfWorker::Stop(){{std::lock_guard<std::mutex> l(mutex);stopping=true;jobs.clear();results.clear();}condition.notify_all();if(thread.joinable())thread.join();}
void RfWorker::Run(){
 RfProcessor receiver;uint64_t current=uint64_t(-1);
 for(;;){Entry entry;{std::unique_lock<std::mutex> l(mutex);condition.wait(l,[&]{return stopping||!jobs.empty();});if(stopping)return;entry=std::move(jobs.front());jobs.pop_front();inFlight=1;}
 if(current!=entry.epoch){receiver.Reset();current=entry.epoch;}
 auto start=std::chrono::steady_clock::now();auto& j=entry.job;
 receiver.Process(j.video.data(),j.audio,j.fps,j.params,j.adjacent.get());
 RfResult r;std::copy(receiver.Frame(),receiver.Frame()+r.video.size(),r.video.begin());r.audio=std::move(j.audio);r.status=receiver.GetStatus();r.milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
 {std::lock_guard<std::mutex> l(mutex);inFlight=0;if(!stopping&&entry.epoch==epoch)results.push_back(std::move(r));}
 }
}
