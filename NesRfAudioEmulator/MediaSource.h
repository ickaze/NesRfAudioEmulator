#pragma once
#include "Common.h"
#include "Rf.h"
#include <thread>
#include <condition_variable>
#include <deque>
#include <memory>
class MediaSource {
public:
 ~MediaSource(){Close();}
 void Open(const std::wstring& path);void Close();
 std::shared_ptr<RfInterference> Segment(size_t samples);
 std::wstring Status();bool IsOpen();
private:
 struct Video {double time=0;std::shared_ptr<std::array<uint32_t,256*240>> pixels;};
 struct Sound {double time=0;unsigned rate=48000;std::vector<float> samples;};
 std::mutex mutex;std::condition_variable condition;std::thread thread;
 std::deque<Video> video;std::deque<Sound> sound;
 std::shared_ptr<std::array<uint32_t,256*240>> current;
 bool stop=false,opened=false;double cursor=0;std::wstring status=L"動画未選択";
 void Decode(std::wstring path);void Error(const wchar_t* text,HRESULT hr);
};
