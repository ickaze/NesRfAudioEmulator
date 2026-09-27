#include "Rf.h"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace {
constexpr double Pi=3.14159265358979323846;
float clamp(float v,float lo,float hi){return std::clamp(v,lo,hi);}
int wrap(int v,int n){v%=n;return v<0?v+n:v;}
uint32_t rgb(float y,float i,float q){
    auto b=[](float v){return uint32_t(clamp(v,0.f,1.f)*255.f+.5f);};
    return 0xff000000u|b(y+.956f*i+.621f*q)|(b(y-.272f*i-.647f*q)<<8)|(b(y-1.106f*i+1.703f*q)<<16);
}
}
float RfProcessor::Random(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return float(rng>>8)*(1.f/16777216.f);}
void RfProcessor::Reset(){*this=RfProcessor();}
void RfProcessor::InitFir(){
    if(firReady)return;
    double sum=0;constexpr double fc=18000./192000.;
    for(int k=0;k<63;++k){int n=k-31;double sinc=n?std::sin(2*Pi*fc*n)/(Pi*n):2*fc;double w=.54-.46*std::cos(2*Pi*k/62);fir[k]=float(sinc*w);sum+=fir[k];}
    for(float& v:fir)v=float(v/sum);firReady=true;
}
void RfProcessor::Process(const uint32_t* source,std::vector<float>& audio,double fps,const RfParams& p,const RfInterference* adjacent){
    if(!p.enabled){std::copy(source,source+Width*Height,output.begin());wasEnabled=false;return;}
    if(!wasEnabled){Reset();wasEnabled=true;}InitFir();
    fps=std::clamp(fps,45.,65.);
    const int totalLines=fps<55?312:262;
    const double lineRate=fps*totalLines, sampleRate=lineRate*LineSamples;
    const float signal=clamp(p.signal,0,1),noise=clamp(p.noise,0,1),contact=clamp(p.contact,0,1);
    const float detune=clamp(p.detune,-1,1), ghost=clamp(p.ghost,0,.8f);
    const int delay=int(clamp(p.ghostDelay,1,48)*4);
    // Retain a whole-frame composite timeline for the sound detector. Audio
    // samples use the SAME contact/gain events as video, not independent noise.
    composite.resize(size_t(totalLines)*LineSamples);prefix.resize(composite.size()+1);
    status={0,0,0};
    const float c[6]={1,.5f,-.5f,-1,-.5f,.5f};
    const float s[6]={0,.8660254f,.8660254f,0,-.8660254f,-.8660254f};
    for(int line=0;line<totalLines;++line){
        // Telegraph-like contact losses followed by RC recovery; chance is per
        // second, so speed and audio chunk size do not change event frequency.
        if(contactTarget==0&&Random()<float((.2+contact*18)*contact/lineRate))contactTarget=.45f+.55f*Random();
        else if(contactTarget>0&&Random()<float(1./((.004+.06*contact)*lineRate)))contactTarget=0;
        if(contact==0){contactTarget=0;contactState=0;}
        contactState+=(contactTarget-contactState)*float(1.-std::exp(-1./(clamp(p.recovery,.0001f,.1f)*lineRate)));
        auto& state=lines[line];state.contact=contactState;
        state.gain=signal*(1-.98f*contactState)*std::exp(-2.f*detune*detune);
        state.sync=clamp((state.gain-.07f)/(noise*.7f+.38f),0,1);
        if(contactState>.2f)++status.dropoutLines;
        status.signal+=state.gain/totalLines;status.sync+=state.sync*clamp(1-std::abs(p.hHold)/.8f,0,1)/totalLines;
        // Receiver horizontal flywheel: free-running drift corrected by sync.
        hPhase+=clamp(p.hHold,-1,1)*2.5f;
        float hLock=clamp(1-std::abs(p.hHold)/.8f,0,1);
        hPhase-=hPhase*(.14f*state.sync*hLock);
        hPhase=std::remainder(hPhase,float(LineSamples));
        float jitter=(clamp(p.hJitter,0,1)*14.f+(1-state.sync)*22.f)*Noise();
        state.shift=hPhase+jitter;
        colorPhase=.92f*colorPhase+.08f*Noise();
        state.phase=clamp(p.colorJitter,0,1)*colorPhase*3.f+detune*.5f+(1-state.sync)*Noise()*1.7f;
        const bool visible=line<Height;
        std::array<float,Width> yl{},il{},ql{},neighbor{},neighborI{},neighborQ{};
        if(visible)for(int x=0;x<Width;++x){
            uint32_t pixel=source[line*Width+x];float r=(pixel&255)/255.f,g=((pixel>>8)&255)/255.f,b=((pixel>>16)&255)/255.f;
            yl[x]=.299f*r+.587f*g+.114f*b;il[x]=.596f*r-.274f*g-.322f*b;ql[x]=.211f*r-.523f*g+.312f*b;
            if(adjacent&&adjacent->active){uint32_t n=adjacent->video[line*Width+x];float nr=(n&255)/255.f,ng=((n>>8)&255)/255.f,nb=((n>>16)&255)/255.f;
                neighbor[x]=.299f*nr+.587f*ng+.114f*nb;neighborI[x]=.596f*nr-.274f*ng-.322f*nb;neighborQ[x]=.211f*nr-.523f*ng+.312f*nb;}
        }
        const int phaseStart=int((frameNumber*uint64_t(totalLines)*LineSamples+uint64_t(line)*LineSamples)%6);
        for(int t=0;t<LineSamples;++t){
            const int phase=(phaseStart+t)%6;
            float value=0;
            if(t<100)value=-.4f;
            else if(t>=116&&t<184)value=.15f*c[phase]; // burst reference
            if(visible&&t>=ActiveStart&&t<ActiveStart+Width*4){
                int x=(t-ActiveStart)/4;value=yl[x]+.55f*(il[x]*c[phase]+ql[x]*s[phase]);
            }
            // Approximate noninterlaced vertical blank/equalizing interval.
            if(line>=Height+3&&line<Height+6)value=(t%682<580)?-.4f:0.f;
            encoded[t]=value;
        }
        float videoLP=0;
        const float bw=clamp(p.bandwidth,1,6)*1e6f/(1+std::abs(detune)*1.8f);
        const float alpha=float(1.-std::exp(-2*Pi*bw/sampleRate));
        const float agc=1.f/std::max(.22f,state.gain);
        bool interference=adjacent&&adjacent->active&&adjacent->videoLevel>0;
        double delta=interference?2*Pi*adjacent->offsetMHz*1e6/sampleRate:0;
        float oscC=float(std::cos(adjacentPhase)),oscS=float(std::sin(adjacentPhase));
        float stepC=float(std::cos(delta)),stepS=float(std::sin(delta));
        for(int t=0;t<LineSamples;++t){
            float echo=t>=delay?encoded[t-delay]:0;
            float channel=(encoded[t]+ghost*echo)/(1+ghost);
            channel=channel*state.gain+Noise()*(noise*.10f+(1-state.gain)*.012f+contactState*.09f);
            if(interference){
                float level=0;
                if(visible&&t>=ActiveStart&&t<ActiveStart+Width*4){int x=(t-ActiveStart)/4,phase=(phaseStart+t+2)%6;level=neighbor[x]+.55f*(neighborI[x]*c[phase]+neighborQ[x]*s[phase]);}
                // Adjacent picture carrier at +/-6 MHz, observed through an
                // imperfect IF selector. Level is effective post-selector leakage.
                // Coarse asymmetric picture IF rejection (VSB-like); not a
                // measured tuner response. This differentiates upper/lower.
                float selectivity=adjacent->offsetMHz<0?.55f:1.f;
                channel+=clamp(adjacent->videoLevel,0,1)*selectivity*.45f*(1-.65f*level)*oscC;
                float next=oscC*stepC-oscS*stepS;oscS=oscS*stepC+oscC*stepS;oscC=next;
            }
            videoLP+=alpha*(channel-videoLP);
            received[t]=videoLP*agc;
            composite[size_t(line)*LineSamples+t]=received[t];
        }
        adjacentPhase=std::remainder(adjacentPhase+delta*LineSamples,2*Pi);
        // Luma/chroma separation: a one-subcarrier-period box notch. This is
        // intentionally a simple receiver, not a motion-adaptive comb filter.
        float sum=0;for(int k=-2;k<=3;++k)sum+=received[std::clamp(k,0,LineSamples-1)];
        for(int t=0;t<LineSamples;++t){smooth[t]=sum/6;sum+=received[std::min(t+4,LineSamples-1)]-received[std::max(t-2,0)];}
        if(!visible)continue;
        float iLP=0,qLP=0;
        // Recover the received burst phase/gain: otherwise the IF/video filter
        // itself would rotate healthy reds toward magenta even at zero detune.
        float burstI=0,burstQ=0;
        for(int t=140;t<176;++t){int phase=(phaseStart+t)%6;float v=received[t]-smooth[t];burstI+=v*c[phase];burstQ+=v*s[phase];}
        burstI/=18;burstQ/=18;
        float burstAngle=std::atan2(burstQ,burstI);
        float chromaGain=clamp(.15f/std::max(.04f,std::hypot(burstI,burstQ)),.5f,2.f);
        float cs=std::cos(state.phase-burstAngle),sn=std::sin(state.phase-burstAngle);
        // Vertical hold is a receiver raster error, not a sound pitch change.
        int outLine=wrap(line+int(vPhase),totalLines);if(outLine>=Height)continue;
        for(int x=0;x<Width;++x){
            float yy=0,ii=0,qq=0;
            for(int sub=0;sub<4;++sub){
                int t=ActiveStart+x*4+sub;
                int shifted=wrap(t+int(state.shift),LineSamples);
                float luma=smooth[shifted];float chroma=received[shifted]-luma;
                int phase=(phaseStart+shifted)%6;
                float di=chroma*c[phase]*2/.55f,dq=chroma*s[phase]*2/.55f;
                iLP+=.15f*(di-iLP);qLP+=.15f*(dq-qLP);
                yy+=luma;ii+=iLP;qq+=qLP;
            }
            yy*=.25f;ii*=.25f;qq*=.25f;
            float colorLock=clamp((state.sync-.1f)/.5f,0,1)*chromaGain;
            output[outLine*Width+x]=rgb(yy,(ii*cs-qq*sn)*colorLock,(ii*sn+qq*cs)*colorLock);
        }
    }
    // Fill receiver rows which currently fall in vertical blanking.
    for(int row=0;row<Height;++row){int inputLine=wrap(row-int(vPhase),totalLines);if(inputLine>=Height)for(int x=0;x<Width;++x)output[row*Width+x]=0xff000000u;}
    vPhase+=clamp(p.vHold,-1,1)*120.f/float(fps)+(1-status.sync)*Noise()*6.f;
    if(std::abs(p.vHold)<.005f)vPhase*=1-.18f*status.sync;
    vPhase=std::fmod(vPhase,float(totalLines));
    // Integration before resampling rejects most chroma energy. Video phase
    // leakage is then differentiated by the FM phase discriminator, generating
    // scan-related buzz and picture-dependent transients, not an arbitrary tone.
    prefix[0]=0;for(size_t i=0;i<composite.size();++i)prefix[i+1]=prefix[i]+composite[i];
    const size_t n=audio.size();
    const float deemA=std::exp(-1.f/(192000.f*75e-6f));
    const float audioAlpha=float(1.-std::exp(-2*Pi*clamp(p.cutoff,400,18000)/48000.));
    const double deviation=25000., step=2*Pi*deviation/192000.;
    for(size_t k=0;k<n;++k){
        float current=audio[k],filtered=0;
        // Sound leakage is a receiver-crosstalk approximation, not direct
        // undersampling of a 6 MHz FM carrier into the audible band.
        if(adjacent&&adjacent->active&&k<adjacent->audio.size())current+=adjacent->audio[k]*clamp(adjacent->audioLevel,0,1)*.65f;
        for(int sub=0;sub<4;++sub){
            const size_t os=k*4+sub,total=n*4;
            size_t a=os*composite.size()/total,b=(os+1)*composite.size()/total;b=std::max(b,a+1);
            float video=float((prefix[b]-prefix[a])/double(b-a));
            const auto& st=lines[std::min(totalLines-1,int(os*totalLines/total))];
            float input=previousAudio+(current-previousAudio)*float(sub+1)*.25f;
            float pre=(input-deemA*preInput)/(1-deemA);preInput=input;
            pre=clamp(pre,-1.5f,1.5f);
            fmPhase=std::remainder(fmPhase+step*pre,2*Pi);
            float syncPulse=float(std::sin(2*Pi*lineRate*double(audioClock+os)/(192000.)));
            float phaseLeak=clamp(p.mix,0,1)*video*(.4f+std::abs(detune)+st.contact*1.5f);
            phaseLeak+=clamp(p.syncLeak,0,1)*syncPulse*(.015f+std::abs(p.hHold)*.04f+std::abs(p.vHold)*.02f+(1-st.sync)*.12f);
            // Equivalent complex sound-carrier envelope. Frequency has already
            // been translated from the 4.5 MHz intercarrier to baseband.
            double phase=fmPhase+phaseLeak;
            float hiss=noise*.022f+(1-st.gain)*.004f+st.contact*.09f;
            float re=float(std::cos(phase))*st.gain+Noise()*hiss;
            float im=float(std::sin(phase))*st.gain+Noise()*hiss;
            float demod=float(std::atan2(im*previousRe-re*previousIm,re*previousRe+im*previousIm)/step);
            previousRe=re;previousIm=im;
            // Detector/baseband crosstalk retains field-rate brightness buzz.
            // Phase modulation alone differentiates luma and loses this component.
            // Use the received composite, so blanking/contact/adjacent video agree.
            demod+=clamp(p.mix,0,1)*.5f*video*st.gain;
            deState+=(1-deemA)*(demod-deState);
            history[historyPos]=deState;filtered=0;
            if(sub==3)for(size_t tap=0;tap<fir.size();++tap)filtered+=fir[tap]*history[(historyPos+history.size()-tap)%history.size()];
            historyPos=(historyPos+1)%history.size();
        }
        previousAudio=current;
        float dc=filtered-dcIn+.9974f*dcOut;dcIn=filtered;dcOut=dc;
        soundLP1+=audioAlpha*(dc-soundLP1);soundLP2+=audioAlpha*(soundLP1-soundLP2);
        audio[k]=std::isfinite(soundLP2)?clamp(soundLP2,-2.f,2.f):0.f;
    }
    audioClock+=n*4;
    ++frameNumber;
}
