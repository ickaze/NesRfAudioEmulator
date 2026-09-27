#include "Audio.h"
#include "InitError.h"
bool AudioEngine::Initialize(){
 Shutdown();error.clear();auto fail=[&](const wchar_t* stage,HRESULT hr){error=InitError(stage,hr);Shutdown();return false;};
 HRESULT hr=CoInitializeEx(nullptr,COINIT_MULTITHREADED);comInitialized=SUCCEEDED(hr);if(FAILED(hr)&&hr!=RPC_E_CHANGED_MODE)return fail(L"音声 COM初期化",hr);
 hr=XAudio2Create(&xa,0,XAUDIO2_DEFAULT_PROCESSOR);if(FAILED(hr))return fail(L"XAudio2 エンジン生成",hr);
 hr=xa->CreateMasteringVoice(&mastering);if(FAILED(hr))return fail(L"XAudio2 出力デバイス / マスターボイス",hr);
 WAVEFORMATEX wf{};wf.wFormatTag=WAVE_FORMAT_PCM;wf.nChannels=1;wf.nSamplesPerSec=48000;wf.wBitsPerSample=16;wf.nBlockAlign=2;wf.nAvgBytesPerSec=96000;
 hr=xa->CreateSourceVoice(&source,&wf,0,2.f,this);if(FAILED(hr))return fail(L"XAudio2 再生ボイス",hr);
 wf.nChannels=2;wf.nBlockAlign=4;wf.nAvgBytesPerSec=192000;
 hr=xa->CreateSourceVoice(&cabinet,&wf,0,2.f,this);if(FAILED(hr))return fail(L"XAudio2 テレビ本体音",hr);
 hr=cabinet->Start();if(FAILED(hr))return fail(L"XAudio2 テレビ本体音開始",hr);
 source->SetVolume(master.load());hr=source->Start();if(FAILED(hr))return fail(L"XAudio2 再生開始",hr);return true;
}
void AudioEngine::Shutdown(){if(cabinet){cabinet->Stop();cabinet->DestroyVoice();cabinet=nullptr;}for(auto& b:tvBuffers)b.busy.store(false);tvSound=TvSound();if(source){source->Stop();source->DestroyVoice();source=nullptr;}if(mastering){mastering->DestroyVoice();mastering=nullptr;}SafeReleaseT(xa);for(auto& b:buffers)b.busy.store(false);if(comInitialized){CoUninitialize();comInitialized=false;}}
void AudioEngine::Clear(){if(source){source->Stop();source->FlushSourceBuffers();source->Start();}}
void AudioEngine::Submit(const std::vector<float>& mono){
 if(!source||mono.empty())return;XAUDIO2_VOICE_STATE state{};source->GetState(&state);if(state.BuffersQueued>=queueLimit)return;
 for(auto& b:buffers){bool expected=false;if(!b.busy.compare_exchange_strong(expected,true))continue;
  b.data.resize(mono.size());float gain=1.f;for(size_t i=0;i<mono.size();++i)b.data[i]=int16_t(std::lrint(std::clamp(mono[i]*gain,-1.f,1.f)*32767.f));
  XAUDIO2_BUFFER buffer{};buffer.AudioBytes=UINT32(b.data.size()*sizeof(int16_t));buffer.pAudioData=reinterpret_cast<const BYTE*>(b.data.data());buffer.pContext=&b;
  if(FAILED(source->SubmitSourceBuffer(&buffer)))b.busy.store(false);return;
 }
}

bool AudioEngine::CanSubmit()const{if(!source)return true;XAUDIO2_VOICE_STATE state{};source->GetState(&state);return state.BuffersQueued<queueLimit;}

void AudioEngine::UpdateTvSound(bool on,float whine,float discharge,float deflection){
 if(!cabinet)return;XAUDIO2_VOICE_STATE state{};cabinet->GetState(&state);
 while(state.BuffersQueued<2){
  Buffer* slot=nullptr;for(auto& b:tvBuffers){bool free=false;if(b.busy.compare_exchange_strong(free,true)){slot=&b;break;}}if(!slot)return;
  slot->data.resize(960);for(size_t i=0;i<480;++i){auto pair=tvSound.Stereo(on,whine,discharge,deflection);for(size_t ch=0;ch<2;++ch)slot->data[i*2+ch]=int16_t(std::lrint(std::clamp(pair[ch],-1.f,1.f)*32767));}
  XAUDIO2_BUFFER packet{};packet.AudioBytes=UINT32(slot->data.size()*2);packet.pAudioData=reinterpret_cast<BYTE*>(slot->data.data());packet.pContext=slot;
  if(FAILED(cabinet->SubmitSourceBuffer(&packet))){slot->busy.store(false);return;}++state.BuffersQueued;
 }
}
