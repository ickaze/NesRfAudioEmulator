#pragma once
#include <array>
#include <vector>
#include <cstdint>
// Receiver-level NTSC-style approximation, not a transistor/RF-carrier simulation.
struct RfParams {
    bool enabled=false;
    float mix=.18f, cutoff=12000.f;
    float signal=.9f, detune=0.f, noise=.025f, contact=0.f;
    float ghost=.04f, ghostDelay=12.f, bandwidth=4.2f;
    float hJitter=.02f, hHold=0.f, vHold=0.f, colorJitter=.02f, syncLeak=.08f, recovery=.003f;
};
struct RfInterference {
    bool active=false;
    std::array<uint32_t,256*240> video{};
    std::vector<float> audio;
    float videoLevel=.25f,audioLevel=.25f;
    float offsetMHz=6.f;
};
class RfProcessor {
public:
    void Reset();
    void Process(const uint32_t* rgb, std::vector<float>& audio, double fps, const RfParams& params,const RfInterference* adjacent=nullptr);
    const uint32_t* Frame()const{return output.data();}
    struct Status {float signal=1, sync=1;unsigned dropoutLines=0;};
    const Status& GetStatus()const{return status;}
private:
    static constexpr int Width=256,Height=240,LineSamples=1364,ActiveStart=256;
    struct Line {float gain=1,sync=1,shift=0,phase=0,contact=0;};
    std::array<uint32_t,Width*Height> output{};
    std::array<Line,312> lines{};
    std::vector<float> composite;
    std::vector<double> prefix;
    std::array<float,LineSamples> encoded{}, received{}, smooth{};
    std::array<float,63> fir{}, history{};
    bool firReady=false, wasEnabled=false;
    uint32_t rng=0x13579bdf;
    uint64_t frameNumber=0,audioClock=0;
    float contactState=0, contactTarget=0, hPhase=0, vPhase=0, colorPhase=0;
    float previousAudio=0, preInput=0, deState=0, soundLP1=0,soundLP2=0,dcIn=0,dcOut=0;
    double adjacentPhase=0;
    double fmPhase=0;float previousRe=1,previousIm=0;
    size_t historyPos=0;
    Status status;
    float Random();
    float Noise(){return 2.f*Random()-1.f;}
    void InitFir();
};
