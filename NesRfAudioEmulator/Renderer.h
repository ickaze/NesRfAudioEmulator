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
    void SetTv(const TvState& state){tv=state;television=true;}
private:
    std::wstring error,notice;bool cpuCrt=false;std::array<uint32_t,256*240> cpuPixels{};
    TvState tv;bool television=false; ID3D11Buffer* constants=nullptr;
    HWND hwnd=nullptr; ID3D11Device* dev=nullptr; ID3D11DeviceContext* ctx=nullptr; IDXGISwapChain* swap=nullptr;
    ID3D11RenderTargetView* rtv=nullptr; ID3D11Texture2D* tex=nullptr; ID3D11ShaderResourceView* srv=nullptr;
    ID3D11VertexShader* vs=nullptr; ID3D11PixelShader* ps=nullptr; ID3D11SamplerState* sampler=nullptr; ID3D11Buffer* vb=nullptr;
    HRESULT CreateTarget();
};
