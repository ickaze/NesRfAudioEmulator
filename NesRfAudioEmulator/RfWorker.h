#pragma once
#include "Rf.h"
#include <mutex>
#include <condition_variable>
#include <deque>
#include <thread>
#include <memory>
struct RfJob {
 std::vector<uint32_t> video=std::vector<uint32_t>(256*240);
 std::vector<float> audio;
 double fps=60;
 RfParams params;
 std::shared_ptr<RfInterference> adjacent;
};
struct RfResult {
 std::vector<uint32_t> video=std::vector<uint32_t>(256*240);
 std::vector<float> audio;
 RfProcessor::Status status;
 double milliseconds=0;
};
class RfWorker {
public:
 RfWorker();~RfWorker();
 void SetCapacity(size_t n);
 bool CanSubmit();bool Submit(RfJob job);bool Take(RfResult& result);
 void Flush();void Stop();size_t Pending();
private:
 struct Entry {RfJob job;uint64_t epoch;};
 std::mutex mutex;std::condition_variable condition;
 std::deque<Entry> jobs;std::deque<RfResult> results;
 uint64_t epoch=0;size_t capacity=4,inFlight=0;bool stopping=false;
 std::thread thread;
 void Run();
};
