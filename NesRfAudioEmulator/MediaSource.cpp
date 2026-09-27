#include "MediaSource.h"
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <wrl/client.h>
#include <chrono>
using Microsoft::WRL::ComPtr;
void MediaSource::Open(const std::wstring& path){Close();{std::lock_guard<std::mutex> l(mutex);stop=false;opened=true;cursor=0;status=L"動画を読み込み中...";}thread=std::thread(&MediaSource::Decode,this,path);}
void MediaSource::Close(){{std::lock_guard<std::mutex> l(mutex);stop=true;}condition.notify_all();if(thread.joinable())thread.join();std::lock_guard<std::mutex> l(mutex);video.clear();sound.clear();current.reset();opened=false;cursor=0;status=L"動画未選択";}
std::wstring MediaSource::Status(){std::lock_guard<std::mutex> l(mutex);return status;}
bool MediaSource::IsOpen(){std::lock_guard<std::mutex> l(mutex);return opened;}
void MediaSource::Error(const wchar_t* text,HRESULT hr){wchar_t h[40];swprintf_s(h,L" (0x%08X)",unsigned(hr));std::lock_guard<std::mutex> l(mutex);status=std::wstring(text)+h;opened=false;}
std::shared_ptr<RfInterference> MediaSource::Segment(size_t count){
 auto out=std::make_shared<RfInterference>();out->audio.resize(count);std::lock_guard<std::mutex> l(mutex);
 if(!opened)return out;
 // Wait for the first decoded video before advancing the media clock.
 if(!current&&video.empty())return out;
 while(!video.empty()&&video.front().time<=cursor+.001){current=video.front().pixels;video.pop_front();}
 if(!current&&!video.empty()){current=video.front().pixels;cursor=std::max(cursor,video.front().time);video.pop_front();}
 if(!current)return out;out->active=true;out->video=*current;
 for(size_t i=0;i<count;++i){double time=cursor+double(i)/48000.;while(!sound.empty()&&sound.front().time+double(sound.front().samples.size())/sound.front().rate<=time)sound.pop_front();
  if(!sound.empty()){auto& s=sound.front();double index=(time-s.time)*s.rate;if(index>=0&&index<double(s.samples.size())){size_t a=size_t(index),b=std::min(a+1,s.samples.size()-1);out->audio[i]=s.samples[a]+float(index-a)*(s.samples[b]-s.samples[a]);}}
 }
 cursor+=double(count)/48000.;condition.notify_all();return out;
}
void MediaSource::Decode(std::wstring path){
 HRESULT com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);if(FAILED(com)){Error(L"COM初期化失敗",com);return;}
 HRESULT hr=MFStartup(MF_VERSION);if(FAILED(hr)){Error(L"Media Foundation初期化失敗",hr);CoUninitialize();return;}
 { // Release all MF objects before MFShutdown/CoUninitialize.
 ComPtr<IMFAttributes> attr;ComPtr<IMFSourceReader> reader;
 MFCreateAttributes(&attr,2);if(attr)attr->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING,TRUE);
 hr=MFCreateSourceReaderFromURL(path.c_str(),attr.Get(),&reader);
 if(FAILED(hr)){Error(L"動画を開けません",hr);}
 else {
  reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS,FALSE);
  ComPtr<IMFMediaType> vtype;MFCreateMediaType(&vtype);vtype->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Video);vtype->SetGUID(MF_MT_SUBTYPE,MFVideoFormat_RGB32);
  hr=reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,nullptr,vtype.Get());
  if(FAILED(hr)){Error(L"映像をRGB32へ変換できません",hr);}
  else{
   reader->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM,TRUE);
   vtype.Reset();reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,&vtype);
   
   UINT32 width=0,height=0;if(vtype)MFGetAttributeSize(vtype.Get(),MF_MT_FRAME_SIZE,&width,&height);
   LONG stride=LONG((vtype?MFGetAttributeUINT32(vtype.Get(),MF_MT_DEFAULT_STRIDE,width*4):0));
   ComPtr<IMFMediaType> atype;MFCreateMediaType(&atype);atype->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Audio);atype->SetGUID(MF_MT_SUBTYPE,MFAudioFormat_PCM);atype->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE,16);
   bool hasAudio=SUCCEEDED(reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM,nullptr,atype.Get()));
   UINT32 rate=48000,channels=1;if(hasAudio){reader->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM,TRUE);atype.Reset();reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM,&atype);rate=MFGetAttributeUINT32(atype.Get(),MF_MT_AUDIO_SAMPLES_PER_SECOND,48000);channels=MFGetAttributeUINT32(atype.Get(),MF_MT_AUDIO_NUM_CHANNELS,1);}
   if(!width||!height||width>8192||height>8192||!rate||!channels||channels>32||(hasAudio&&MFGetAttributeUINT32(atype.Get(),MF_MT_AUDIO_BITS_PER_SAMPLE,16)!=16)){Error(L"動画の形式が不正です",E_INVALIDARG);}
   else{
    {std::lock_guard<std::mutex> l(mutex);status=hasAudio?L"再生中（音声はRF混信へ出力）":L"再生中（音声ストリームなし）";}
    bool videoEnd=false,audioEnd=!hasAudio;double furthest=0,loopBase=0,loopEnd=0,duration=0;bool loopHasSample=false;
    PROPVARIANT durationValue{};
    if(SUCCEEDED(reader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE,MF_PD_DURATION,&durationValue))&&durationValue.vt==VT_UI8)duration=double(durationValue.uhVal.QuadPart)/10000000.;
    PropVariantClear(&durationValue);
    for(;;){
     {std::unique_lock<std::mutex> l(mutex);condition.wait(l,[&]{return stop||furthest<cursor+.35||(!current&&video.empty());});if(stop)break;}
     DWORD stream=0,flags=0;LONGLONG stamp=0;ComPtr<IMFSample> sample;
     hr=reader->ReadSample(MF_SOURCE_READER_ANY_STREAM,0,&stream,&flags,&stamp,&sample);
     if(FAILED(hr)){Error(L"動画のデコードに失敗しました",hr);break;}
     ComPtr<IMFMediaType> actual;hr=reader->GetCurrentMediaType(stream,&actual);
     if(FAILED(hr)||!actual){Error(L"デコーダーの出力形式を取得できません",FAILED(hr)?hr:E_FAIL);break;}
     GUID major{};actual->GetGUID(MF_MT_MAJOR_TYPE,&major);
     if(flags&MF_SOURCE_READERF_ENDOFSTREAM){reader->SetStreamSelection(stream,FALSE);if(major==MFMediaType_Video)videoEnd=true;else audioEnd=true;if(videoEnd&&audioEnd){
       double length=std::max(duration,loopEnd);
       if(!loopHasSample||length<=0){Error(L"ループ可能な映像・音声サンプルがありません",E_FAIL);break;}
       reader->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM,TRUE);
       if(hasAudio)reader->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM,TRUE);
       PROPVARIANT start{};start.vt=VT_I8;start.hVal.QuadPart=0;
       hr=reader->SetCurrentPosition(GUID_NULL,start);
       if(FAILED(hr)){Error(L"動画の先頭へ移動できません（ループ失敗）",hr);break;}
       loopBase+=length;loopEnd=0;loopHasSample=false;videoEnd=false;audioEnd=!hasAudio;
       {std::lock_guard<std::mutex> l(mutex);status=L"自動ループ再生中（音声はRF混信へ出力）";}
      }continue;}
     // Decoder notifications also occur at startup, before the first sample.
     // Read the actual output layout before interpreting any returned bytes.
     if(flags&(MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED|MF_SOURCE_READERF_NATIVEMEDIATYPECHANGED)){
      GUID subtype{};actual->GetGUID(MF_MT_SUBTYPE,&subtype);
      bool compatible=(major==MFMediaType_Video&&subtype==MFVideoFormat_RGB32)||
       (major==MFMediaType_Audio&&subtype==MFAudioFormat_PCM&&MFGetAttributeUINT32(actual.Get(),MF_MT_AUDIO_BITS_PER_SAMPLE,0)==16);
      if(!compatible){
       ComPtr<IMFMediaType> requested;hr=MFCreateMediaType(&requested);
       if(SUCCEEDED(hr)){
        requested->SetGUID(MF_MT_MAJOR_TYPE,major);
        requested->SetGUID(MF_MT_SUBTYPE,major==MFMediaType_Video?MFVideoFormat_RGB32:MFAudioFormat_PCM);
        if(major==MFMediaType_Audio)requested->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE,16);
        hr=reader->SetCurrentMediaType(stream,nullptr,requested.Get());
       }
       if(FAILED(hr)){Error(L"変更後の映像・音声をRGB32/PCM16へ変換できません",hr);break;}
       actual.Reset();hr=reader->GetCurrentMediaType(stream,&actual);
       if(FAILED(hr)||!actual){Error(L"変更後の出力形式を取得できません",FAILED(hr)?hr:E_FAIL);break;}
       // This sample belongs to the old layout; resume with the negotiated one.
       sample.Reset();actual->GetGUID(MF_MT_SUBTYPE,&subtype);
      }
      if(major==MFMediaType_Video){
       UINT32 w=0,h=0;hr=MFGetAttributeSize(actual.Get(),MF_MT_FRAME_SIZE,&w,&h);
       LONG pitch=LONG(MFGetAttributeUINT32(actual.Get(),MF_MT_DEFAULT_STRIDE,w*4));
       if(FAILED(hr)||subtype!=MFVideoFormat_RGB32||!w||!h||w>8192||h>8192||std::abs(int64_t(pitch))<int64_t(w)*4||std::abs(int64_t(pitch))>8192*16){Error(L"変更後の映像サイズ・画素形式に対応できません",MF_E_INVALIDMEDIATYPE);break;}
       width=w;height=h;stride=pitch;
      }else if(major==MFMediaType_Audio){
       UINT32 r=MFGetAttributeUINT32(actual.Get(),MF_MT_AUDIO_SAMPLES_PER_SECOND,0),c=MFGetAttributeUINT32(actual.Get(),MF_MT_AUDIO_NUM_CHANNELS,0);
       if(subtype!=MFAudioFormat_PCM||MFGetAttributeUINT32(actual.Get(),MF_MT_AUDIO_BITS_PER_SAMPLE,0)!=16||!r||r>384000||!c||c>32){Error(L"変更後の音声形式に対応できません",MF_E_INVALIDMEDIATYPE);break;}
       rate=r;channels=c;
      }
     }
     if(!sample)continue;
     double relative=double(stamp)/10000000.;LONGLONG sampleDuration=0;sample->GetSampleDuration(&sampleDuration);
     loopHasSample=true;loopEnd=std::max(loopEnd,relative+std::max(double(sampleDuration)/10000000.,major==MFMediaType_Video?1./60.:0.));
     double timestamp=loopBase+relative;furthest=std::max(furthest,timestamp);
     ComPtr<IMFMediaBuffer> buffer;hr=sample->ConvertToContiguousBuffer(&buffer);if(FAILED(hr))continue;
     if(major==MFMediaType_Video){
      auto pixels=std::make_shared<std::array<uint32_t,256*240>>();BYTE* scan=nullptr;LONG pitch=stride;ComPtr<IMF2DBuffer> two;bool locked2=false;DWORD length=0;
      if(SUCCEEDED(buffer.As(&two))&&SUCCEEDED(two->Lock2D(&scan,&pitch)))locked2=true;
      else{hr=buffer->Lock(&scan,nullptr,&length);if(FAILED(hr))continue;if(length<size_t(std::abs(stride))*height){buffer->Unlock();Error(L"映像バッファが不足しています",E_FAIL);break;}if(stride<0)scan+=size_t(-stride)*(height-1);}
      // Preserve aspect ratio in the broadcast frame, with letterboxing.
      pixels->fill(0xff000000);float scale=std::min(256.f/width,240.f/height);int w=std::max(1,int(width*scale)),h=std::max(1,int(height*scale)),left=(256-w)/2,top=(240-h)/2;
      for(int y=0;y<h;++y){const BYTE* row=scan+ptrdiff_t((uint64_t(y)*height)/h)*pitch;for(int x=0;x<w;++x){const BYTE* px=row+((uint64_t(x)*width)/w)*4;(*pixels)[(y+top)*256+x+left]=0xff000000u|uint32_t(px[2])|(uint32_t(px[1])<<8)|(uint32_t(px[0])<<16);}}
      if(locked2)two->Unlock2D();else buffer->Unlock();
      std::lock_guard<std::mutex> l(mutex);video.push_back({timestamp,pixels});while(video.size()>90)video.pop_front();
     }else if(major==MFMediaType_Audio){
      BYTE* bytes=nullptr;DWORD length=0;if(FAILED(buffer->Lock(&bytes,nullptr,&length)))continue;size_t count=length/(2*channels);Sound packet;packet.time=timestamp;packet.rate=rate;packet.samples.resize(count);
      const int16_t* pcm=reinterpret_cast<const int16_t*>(bytes);for(size_t i=0;i<count;++i){float sum=0;for(UINT32 ch=0;ch<channels;++ch)sum+=pcm[i*channels+ch]/32768.f;packet.samples[i]=sum/channels;}buffer->Unlock();
      std::lock_guard<std::mutex> l(mutex);sound.push_back(std::move(packet));while(sound.size()>256)sound.pop_front();
     }
    }
   }
  }
 }
 }
 MFShutdown();CoUninitialize();
}
