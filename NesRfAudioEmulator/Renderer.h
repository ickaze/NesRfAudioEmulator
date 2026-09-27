#pragma once
#include "Common.h"
#include <d3d11.h>
#include <dxgi.h>
#include "TvState.h"

class Renderer {
public:
    bool Initialize(HWND hwnd); void Shutdown(); void Resize(int w,int h); void Present(const uint32_t* rgba);
    const std::wstring& Error()const{return error;}
    const std::wstring& Notice()const{return notice;}
    void SetCpu(bool enabled){forceCpu=enabled;}
    std::wstring ShortStatus()const{return std::wstring(backend.find(L"WARP")==0?L"WARP / CRT: ":L"GPU / CRT: ")+((forceCpu||cpuCrt)?(cpuCrt?L"CPU（自動退避）":L"CPU（手動）"):L"シェーダー適用中 (SM4.0)");}
    std::wstring Status()const{return backend+L" / CRT: "+((forceCpu||cpuCrt)?(cpuCrt?L"CPU（シェーダー不可）":L"CPU（手動）"):L"GPUシェーダー適用中 (SM4.0)");}
    void SetTv(const TvState& state){tv=state;television=true;}
private:
    std::wstring error,notice,backend;bool cpuCrt=false,forceCpu=false;std::array<uint32_t,256*240> cpuPixels{};
    TvState tv;bool television=false; ID3D11Buffer* constants=nullptr;
    HWND hwnd=nullptr; ID3D11Device* dev=nullptr; ID3D11DeviceContext* ctx=nullptr; IDXGISwapChain* swap=nullptr;
    ID3D11RenderTargetView* rtv=nullptr; ID3D11Texture2D* tex=nullptr; ID3D11ShaderResourceView* srv=nullptr;
    ID3D11VertexShader* vs=nullptr; ID3D11PixelShader* ps=nullptr;ID3D11PixelShader* basicPs=nullptr; ID3D11SamplerState* sampler=nullptr; ID3D11Buffer* vb=nullptr;
    HRESULT CreateTarget();
};
