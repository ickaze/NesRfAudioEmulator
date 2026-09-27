#include "Nes.h"
#include <fstream>
#include <algorithm>
#include <cmath>
#include <cstring>
Nes* Nes::active=nullptr;
bool Nes::Initialize(const std::filesystem::path& dir){
 if(initialized)return true;if(active)return false;
 std::error_code ec;std::filesystem::create_directories(dir,ec);if(ec)return false;
 directory=dir.u8string();active=this;
 retro_set_environment(Environment);retro_set_video_refresh(Video);
 retro_set_audio_sample(Sample);retro_set_audio_sample_batch(Samples);
 retro_set_input_poll(Poll);retro_set_input_state(Input);retro_init();initialized=true;return true;
}
bool Nes::Environment(unsigned cmd,void* data){
 if(!active)return false;auto& n=*active;
 switch(cmd){
 case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
 case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:*static_cast<const char**>(data)=n.directory.c_str();return true;
 case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:*static_cast<unsigned*>(data)=0;return true;
 case RETRO_ENVIRONMENT_SET_VARIABLES:{
  for(auto v=static_cast<retro_variable*>(data);v&&v->key;++v){
   std::string s=v->value?v->value:"";auto pos=s.find(';');
   if(pos!=std::string::npos){s=s.substr(pos+1);while(!s.empty()&&s.front()==' ')s.erase(0,1);n.options[v->key]=s.substr(0,s.find('|'));}
  }return true;
 }
 case RETRO_ENVIRONMENT_GET_VARIABLE:{
  auto v=static_cast<retro_variable*>(data);std::string key=v->key;
  if(key=="fceumm_sndrate_hint")n.options[key]="48KHz";
  if(key=="fceumm_sndquality")n.options[key]="High";
  if(key.find("fceumm_overscan_")==0)n.options[key]="0";
  auto it=n.options.find(key);v->value=it==n.options.end()?nullptr:it->second.c_str();return v->value!=nullptr;
 }
 case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:*static_cast<bool*>(data)=false;return true;
 case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:n.pixelFormat=*static_cast<unsigned*>(data);return n.pixelFormat<=RETRO_PIXEL_FORMAT_RGB565;
 case RETRO_ENVIRONMENT_GET_CAN_DUPE:*static_cast<bool*>(data)=true;return true;
 case RETRO_ENVIRONMENT_SET_MESSAGE:n.message=static_cast<retro_message*>(data)->msg;return true;
 case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:n.fps=static_cast<retro_system_av_info*>(data)->timing.fps;return true;
 case RETRO_ENVIRONMENT_SET_GEOMETRY:case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:return true;
 default:return false;
 }
}
void Nes::Video(const void* data,unsigned w,unsigned h,size_t pitch){
 if(!active||!data||!w||!h)return;auto& n=*active;
 for(unsigned y=0;y<240;++y)for(unsigned x=0;x<256;++x){
  auto row=static_cast<const uint8_t*>(data)+(y*h/240)*pitch;unsigned r,g,b;
  if(n.pixelFormat==RETRO_PIXEL_FORMAT_XRGB8888){uint32_t p;std::memcpy(&p,row+(x*w/256)*4,4);r=(p>>16)&255;g=(p>>8)&255;b=p&255;}
  else{uint16_t p;std::memcpy(&p,row+(x*w/256)*2,2);b=(p&31)*255/31;
   if(n.pixelFormat==RETRO_PIXEL_FORMAT_RGB565){r=((p>>11)&31)*255/31;g=((p>>5)&63)*255/63;}
   else{r=((p>>10)&31)*255/31;g=((p>>5)&31)*255/31;}}
  n.frame[y*256+x]=0xff000000u|(b<<16)|(g<<8)|r; // R8G8B8A8
 }
}
void Nes::Sample(int16_t l,int16_t r){if(active)active->samples.push_back((float(l)+float(r))/65536.f);}
size_t Nes::Samples(const int16_t* data,size_t count){for(size_t i=0;i<count;++i)Sample(data[i*2],data[i*2+1]);return count;}
int16_t Nes::Input(unsigned port,unsigned device,unsigned,unsigned id){return active&&port<2&&(device&RETRO_DEVICE_MASK)==RETRO_DEVICE_JOYPAD&&id<16?((active->pads[port]>>id)&1):0;}
bool Nes::SaveRam(){
 if(!loaded)return true;auto size=retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);auto ptr=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);if(!ptr||!size)return true;
 auto tmp=savePath;tmp+=".tmp";std::ofstream f(tmp,std::ios::binary|std::ios::trunc);if(!f)return false;
 f.write(static_cast<const char*>(ptr),static_cast<std::streamsize>(size));f.close();if(!f)return false;
 std::error_code ec;auto bak=savePath;bak+=".bak";
 if(std::filesystem::exists(savePath)){std::filesystem::remove(bak,ec);ec.clear();std::filesystem::rename(savePath,bak,ec);if(ec)return false;}
 std::filesystem::rename(tmp,savePath,ec);
 if(ec){std::error_code ignored;if(std::filesystem::exists(bak))std::filesystem::rename(bak,savePath,ignored);return false;}return true;
}
bool Nes::LoadRom(const std::wstring& path,std::wstring& error){
 if(!initialized){error=L"コアが初期化されていません。";return false;}
 std::filesystem::path file(path);std::ifstream f(file,std::ios::binary|std::ios::ate);
 if(!f||f.tellg()<=0||f.tellg()>64*1024*1024){error=L"ROMを開けないか、サイズが不正です（上限64MB）。";return false;}
 auto size=static_cast<size_t>(f.tellg());std::vector<char> bytes(size);f.seekg(0);f.read(bytes.data(),size);if(!f){error=L"ROMを最後まで読み込めませんでした。";return false;}
 if(size>=4&&std::memcmp(bytes.data(),"NES\x1a",4)==0){
  if(size<16){error=L"iNESヘッダーが途中で切れています。";return false;}
  auto h=reinterpret_cast<const uint8_t*>(bytes.data());
  if((h[7]&12)!=8){size_t required=16+((h[6]&4)?512:0)+size_t(h[4])*16384+size_t(h[5])*8192;
   if(!h[4]||size<required){error=L"iNESのPRG/CHRデータが不足しています。";return false;}}
 }
 if(!SaveRam()){error=L"セーブを保存できません。ROMフォルダーの書き込み権限を確認してください。";return false;}
 if(loaded){retro_unload_game();loaded=false;}
 romPath=file.u8string();message.clear();retro_game_info info{romPath.c_str(),bytes.data(),bytes.size(),nullptr};
 if(!retro_load_game(&info)){error=L"ROMを読み込めません。形式・マッパー・破損、またはFDS BIOS不足を確認してください。";return false;}
 loaded=true;savePath=file;savePath+=".sav";
 auto ptr=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);auto len=retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
 if(ptr&&len){std::ifstream s(savePath,std::ios::binary);if(s)s.read(static_cast<char*>(ptr),len);}
 retro_set_controller_port_device(0,RETRO_DEVICE_JOYPAD);retro_set_controller_port_device(1,RETRO_DEVICE_JOYPAD);
 retro_system_av_info av{};retro_get_system_av_info(&av);fps=av.timing.fps;receiver.Reset();processed=false;return true;
}
void Nes::Reset(){if(loaded)retro_reset();receiver.Reset();processed=false;}
void Nes::Shutdown(){if(!initialized)return;SaveRam();if(loaded)retro_unload_game();retro_deinit();loaded=initialized=false;active=nullptr;}
void Nes::RunRawFrame(std::vector<float>& out){
 samples.clear();if(loaded)retro_run();out=samples;processed=false;
}
void Nes::RunFrame(std::vector<float>& out,const RfParams& rf){
 RunRawFrame(out);if(!loaded)return;
 receiver.Process(frame.data(),out,fps,rf);processed=true;
}
