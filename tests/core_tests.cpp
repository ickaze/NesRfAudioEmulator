#include "Nes.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cmath>
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static std::filesystem::path makeRom(const std::filesystem::path& dir,int mapper){
 auto path=dir/("mapper"+std::to_string(mapper)+".nes");
 std::vector<uint8_t> rom(16+65536+8192,0);rom[0]='N';rom[1]='E';rom[2]='S';rom[3]=26;rom[4]=4;rom[5]=1;rom[6]=uint8_t((mapper<<4)|2);rom[7]=uint8_t(mapper&0xf0);
 // Identical program in every 8K bank lets each mapper boot at E000 safely.
 const uint8_t program[]={0x78,0xd8,0xa2,0xff,0x9a,0xa9,0x42,0x85,0x10,
  0xa9,0x37,0x8d,0x00,0x60, // battery RAM
  0xa9,0x3f,0x8d,0x06,0x20,0xa9,0,0x8d,0x06,0x20,0xa9,0x21,0x8d,0x07,0x20,
  0xa9,1,0x8d,0x15,0x40,0xa9,0xbf,0x8d,0,0x40,0xa9,0x80,0x8d,2,0x40,0xa9,8,0x8d,3,0x40,
  0xa9,1,0x8d,0x16,0x40,0xa9,0,0x8d,0x16,0x40,0xad,0x16,0x40,0x85,0x11,0xad,0x17,0x40,0x85,0x12,
  0x4c,0x31,0xe0};
 for(int bank=0;bank<8;++bank){auto offset=16+bank*8192;std::copy(std::begin(program),std::end(program),rom.begin()+offset);for(int v=0x1ffa;v<=0x1ffe;v+=2){rom[offset+v]=0;rom[offset+v+1]=0xe0;}}
 std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(rom.data()),rom.size());return path;
}
int main(){try{
 auto dir=std::filesystem::current_path()/"test-data";std::filesystem::create_directories(dir);
 Nes nes;check(nes.Initialize(dir),"initialize");std::wstring error;RfParams rf;rf.mix=0;std::vector<float> audio;
 for(int mapper:{0,1,2,3,4,7,11,66}){
  auto path=makeRom(dir,mapper);check(nes.LoadRom(path.wstring(),error),"load mapper");
  nes.SetPad(0,1u<<RETRO_DEVICE_ID_JOYPAD_A);nes.SetPad(1,1u<<RETRO_DEVICE_ID_JOYPAD_A);
  bool sound=false;for(int frame=0;frame<15;++frame){nes.RunFrame(audio,rf);check(audio.size()>500&&audio.size()<1200,"audio count");for(float v:audio){check(std::isfinite(v),"finite audio");sound|=std::abs(v)>.0001f;}}
  auto ram=static_cast<uint8_t*>(retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM));check(ram&&ram[0x10]==0x42,"CPU boot marker");check((ram[0x11]&1)==1&&(ram[0x12]&1)==1,"two player input");check(sound,"APU output");
  check((nes.Frame()[0]&0xffffff)!=0,"video output");check(nes.SaveRam(),"save RAM");
  std::cout<<"Mapper "<<mapper<<": boot/video/audio/two-player input/save OK\n";
 }
 auto path=makeRom(dir,0);check(nes.LoadRom(path.wstring(),error),"reload");
 auto save=static_cast<uint8_t*>(retro_get_memory_data(RETRO_MEMORY_SAVE_RAM));check(save!=nullptr,"battery data");save[42]=0xa5;check(nes.SaveRam(),"battery write");check(nes.LoadRom(path.wstring(),error),"battery reload");save=static_cast<uint8_t*>(retro_get_memory_data(RETRO_MEMORY_SAVE_RAM));check(save[42]==0xa5,"battery roundtrip");
 nes.SetPad(0,0);nes.SetPad(1,0);nes.RunFrame(audio,rf);auto ram=static_cast<uint8_t*>(retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM));check(!(ram[0x11]&1)&&!(ram[0x12]&1),"release input");
 rf.enabled=true;rf.mix=1;rf.cutoff=300;for(int i=0;i<10;++i)nes.RunFrame(audio,rf);for(float v:audio)check(std::isfinite(v),"RF audio finite");
 auto bad=dir/"truncated.nes";{std::ofstream f(bad,std::ios::binary);f.write("NES\x1a",4);}check(!nes.LoadRom(bad.wstring(),error),"reject truncated header");check(nes.Loaded(),"keep loaded game on invalid preflight");
 nes.Reset();nes.RunFrame(audio,rf);check(ram[0x10]==0x42,"reset");nes.Shutdown();
 check(nes.Initialize(dir),"reinitialize");check(nes.LoadRom(path.wstring(),error),"reload after shutdown");nes.Shutdown();
 std::cout<<"Battery roundtrip, input release, RF processing, reset, shutdown/reinitialize OK\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
