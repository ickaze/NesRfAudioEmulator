#pragma once
#include "Common.h"
#include <xaudio2.h>
#include "TvSound.h"
class AudioEngine: private IXAudio2VoiceCallback {
public:
 const std::wstring& Error()const{return error;}
 bool Initialize();void Shutdown();void Clear();
 void SetQueueLimit(unsigned n){queueLimit=std::clamp(n,2u,4u);}
 bool CanSubmit()const;
 void Submit(const std::vector<float>& mono);
 void UpdateTvSound(bool on,float whine,float discharge,float deflection);
 void SetMaster(float v){master.store(v);if(source)source->SetVolume(std::clamp(v,0.f,1.f));}
private:
 unsigned queueLimit=4;
 std::wstring error;
 struct Buffer {std::vector<int16_t> data;std::atomic<bool> busy{false};};
 std::array<Buffer,6> buffers;
 std::array<Buffer,4> tvBuffers;TvSound tvSound;IXAudio2SourceVoice* cabinet=nullptr;
 IXAudio2* xa=nullptr;IXAudio2MasteringVoice* mastering=nullptr;IXAudio2SourceVoice* source=nullptr;
 bool comInitialized=false;std::atomic<float> master{.8f};
 void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32)override{}
 void STDMETHODCALLTYPE OnVoiceProcessingPassEnd()override{}
 void STDMETHODCALLTYPE OnStreamEnd()override{}
 void STDMETHODCALLTYPE OnBufferStart(void*)override{}
 void STDMETHODCALLTYPE OnBufferEnd(void* context)override{static_cast<Buffer*>(context)->busy.store(false);}
 void STDMETHODCALLTYPE OnLoopEnd(void*)override{}
 void STDMETHODCALLTYPE OnVoiceError(void*,HRESULT)override{}
};
