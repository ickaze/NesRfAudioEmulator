#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <libretro.h>
#include "Rf.h"
// Single libretro instance; callbacks and UI run on the same thread.
class Nes {
public:
 bool Initialize(const std::filesystem::path& dataDirectory);
 void Shutdown();
 bool LoadRom(const std::wstring& path, std::wstring& error);
 bool SaveRam();
 void Reset();
 void SetPad(unsigned port,uint16_t buttons){if(port<2)pads[port]=buttons;}
 void RunRawFrame(std::vector<float>& audio);
 void RunFrame(std::vector<float>& audio,const RfParams& rf);
 const uint32_t* Frame()const{return processed?receiver.Frame():frame.data();}
 const uint32_t* DigitalFrame()const{return frame.data();}
 const RfProcessor::Status& RfStatus()const{return receiver.GetStatus();}
 bool Loaded()const{return loaded;}
 double Fps()const{return fps;}
private:
 static Nes* active;
 static bool Environment(unsigned,void*);
 static void Video(const void*,unsigned,unsigned,size_t);
 static void Sample(int16_t,int16_t);
 static size_t Samples(const int16_t*,size_t);
 static int16_t Input(unsigned,unsigned,unsigned,unsigned);
 static void Poll(){}
 bool initialized=false,loaded=false;
 unsigned pixelFormat=RETRO_PIXEL_FORMAT_0RGB1555;
 std::array<uint16_t,2> pads{};
 std::array<uint32_t,256*240> frame{};
 std::map<std::string,std::string> options;
 std::vector<float> samples;
 std::filesystem::path savePath;
 std::string directory,romPath,message;
 double fps=60.0988;
 RfProcessor receiver;
 bool processed=false;
};
