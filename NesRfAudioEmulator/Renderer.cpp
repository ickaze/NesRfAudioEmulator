#include "Renderer.h"
#include "InitError.h"
#include "CrtCpu.h"
#include <d3dcompiler.h>
#pragma comment(lib,"d3dcompiler.lib")

bool Renderer::Initialize(HWND h){ Shutdown();error.clear();notice.clear();cpuCrt=false;hwnd=h;
    auto fail=[&](const wchar_t* stage,HRESULT hr){error=InitError(stage,hr);Shutdown();return false;}; RECT r{}; GetClientRect(hwnd,&r); DXGI_SWAP_CHAIN_DESC sd{}; sd.BufferCount=2; sd.BufferDesc.Width=std::max(1L,r.right); sd.BufferDesc.Height=std::max(1L,r.bottom); sd.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM; sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.OutputWindow=hwnd; sd.SampleDesc.Count=1; sd.Windowed=TRUE; sd.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL fl;const D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_0,D3D_FEATURE_LEVEL_10_1,D3D_FEATURE_LEVEL_10_0};
    HRESULT hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,levels,3,D3D11_SDK_VERSION,&sd,&swap,&dev,&fl,&ctx);
    if(FAILED(hr)){notice=InitError(L"GPU初期化失敗。WARPへ切り替え",hr);Shutdown();hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,levels,3,D3D11_SDK_VERSION,&sd,&swap,&dev,&fl,&ctx);}
    if(FAILED(hr))return fail(L"D3D11 デバイス / スワップチェーン",hr);
    hr=CreateTarget();if(FAILED(hr))return fail(L"D3D11 描画先",hr);
    D3D11_TEXTURE2D_DESC td{}; td.Width=NES_W;td.Height=NES_H;td.MipLevels=1;td.ArraySize=1;td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_DYNAMIC;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;td.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE; hr=dev->CreateTexture2D(&td,nullptr,&tex);if(FAILED(hr))return fail(L"D3D11 映像テクスチャ",hr);hr=dev->CreateShaderResourceView(tex,nullptr,&srv);if(FAILED(hr))return fail(L"D3D11 映像リソースビュー",hr);
    const char* vsrc="struct O{float4 p:SV_Position;float2 u:TEXCOORD;}; O main(uint id:SV_VertexID){float2 p[3]={float2(-1,-1),float2(-1,3),float2(3,-1)};float2 u[3]={float2(0,1),float2(0,-1),float2(2,1)};O o;o.p=float4(p[id],0,1);o.u=u[id];return o;}";
    const char* psrc=R"(
Texture2D t:register(t0);SamplerState s:register(s0);
cbuffer Television:register(b0){float4 controls;float4 beam;float4 options;};
float4 main(float4 p:SV_Position,float2 u:TEXCOORD):SV_Target{
 if(options.x<0.5)return t.Sample(s,u);
 float2 q=u*2-1;
 // Rounded glass and mild curvature. Collapse operates in screen coordinates.
 float glass=1-smoothstep(.94,1.0,pow(abs(q.x),8)+pow(abs(q.y),8));
 float2 extent=max(beam.xy,float2(.002,.002));
 float2 v=q/extent;v*=1+.035*dot(v,v);
 float edge=(1-smoothstep(.96,1.02,abs(v.x)))*(1-smoothstep(.96,1.02,abs(v.y)));
 float3 c=t.Sample(s,saturate(v*.5+.5)).rgb;
 float y=dot(c,float3(.299,.587,.114));
 float i=dot(c,float3(.596,-.274,-.322)),j=dot(c,float3(.211,-.523,.312));
 float cs=cos(controls.z),sn=sin(controls.z);float a=(i*cs-j*sn)*controls.w,b=(i*sn+j*cs)*controls.w;
 c=float3(y+.956*a+.621*b,y-.272*a-.647*b,y-1.106*a+1.703*b);
 c=saturate((c-.5)*controls.x+.5+controls.y);
 float squeeze=saturate(1-beam.y);
 c=lerp(c,float3(1,.92,.79),squeeze*.72);
 float light=beam.z*(1+2*squeeze);
 float scan=.95+.05*cos(v.y*240*3.14159265);
 float glow=exp(-dot(q/float2(max(.008,extent.x),max(.008,extent.y)),q/float2(max(.008,extent.x),max(.008,extent.y)))*beam.z*squeeze*.13;
 float3 glassBase=float3(.018,.024,.022)*(1-.35*dot(q,q));
 return float4((glassBase+c*edge*light*scan+glow)*glass,1);
})";

    auto compile=[&](const char* source,const char* name,const char* profile,ID3DBlob** code){
        ID3DBlob* messages=nullptr;HRESULT result=D3DCompile(source,strlen(source),name,nullptr,nullptr,"main",profile,D3DCOMPILE_ENABLE_STRICTNESS,0,code,&messages);
        if(FAILED(result)){error=InitError(L"D3DCompile",result)+L"\n"+ShaderMessage(name,strlen(name));if(messages)error+=L"\n"+ShaderMessage(static_cast<const char*>(messages->GetBufferPointer()),messages->GetBufferSize());}
        SafeReleaseT(messages);return result;
    };
    ID3DBlob* code=nullptr;
    hr=compile(vsrc,"FullscreenVS.hlsl","vs_4_0",&code);if(FAILED(hr)){SafeReleaseT(code);Shutdown();return false;}
    hr=dev->CreateVertexShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&vs);SafeReleaseT(code);if(FAILED(hr))return fail(L"D3D11 頂点シェーダー生成",hr);
    hr=compile(psrc,"TelevisionPS.hlsl","ps_4_0",&code);
    if(SUCCEEDED(hr)){hr=dev->CreatePixelShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&ps);if(FAILED(hr))error=InitError(L"D3D11 ブラウン管シェーダー生成",hr);}
    SafeReleaseT(code);
    if(FAILED(hr)){
        notice+=L"\n"+error+L"\nブラウン管演出をCPU処理へ切り替えました。";error.clear();cpuCrt=true;
        const char* basic="Texture2D t:register(t0);SamplerState s:register(s0);float4 main(float4 p:SV_Position,float2 u:TEXCOORD):SV_Target{return t.Sample(s,u);}";
        hr=compile(basic,"BasicPS.hlsl","ps_4_0",&code);if(FAILED(hr)){SafeReleaseT(code);Shutdown();return false;}
        hr=dev->CreatePixelShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&ps);SafeReleaseT(code);if(FAILED(hr))return fail(L"D3D11 基本シェーダー生成",hr);
    }
    D3D11_BUFFER_DESC cb{};cb.ByteWidth=48;cb.Usage=D3D11_USAGE_DEFAULT;cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;hr=dev->CreateBuffer(&cb,nullptr,&constants);if(FAILED(hr))return fail(L"D3D11 ブラウン管定数バッファ",hr);
    D3D11_SAMPLER_DESC ss{};ss.Filter=D3D11_FILTER_MIN_MAG_MIP_POINT;ss.AddressU=ss.AddressV=ss.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;ss.MaxAnisotropy=1;ss.ComparisonFunc=D3D11_COMPARISON_NEVER;ss.MaxLOD=D3D11_FLOAT32_MAX;
    hr=dev->CreateSamplerState(&ss,&sampler);if(FAILED(hr))return fail(L"D3D11 サンプラー",hr);return true;
}
HRESULT Renderer::CreateTarget(){
 SafeReleaseT(rtv);ID3D11Texture2D* back=nullptr;if(!swap)return E_POINTER;
 HRESULT hr=swap->GetBuffer(0,__uuidof(ID3D11Texture2D),reinterpret_cast<void**>(&back));
 if(SUCCEEDED(hr)){hr=dev->CreateRenderTargetView(back,nullptr,&rtv);back->Release();}return hr;
}

void Renderer::Resize(int w,int h){ if(!swap||w<=0||h<=0)return; ctx->OMSetRenderTargets(0,nullptr,nullptr); SafeReleaseT(rtv); swap->ResizeBuffers(0,w,h,DXGI_FORMAT_UNKNOWN,0); CreateTarget(); }
void Renderer::Present(const uint32_t* px){ if(!ctx||!rtv||!tex||!srv||!vs||!ps||!sampler||!constants)return;if(cpuCrt&&television){CrtCpu(px,cpuPixels.data(),tv);px=cpuPixels.data();} D3D11_MAPPED_SUBRESOURCE m{}; if(SUCCEEDED(ctx->Map(tex,0,D3D11_MAP_WRITE_DISCARD,0,&m))){for(int y=0;y<NES_H;y++)memcpy((BYTE*)m.pData+y*m.RowPitch,px+y*NES_W,NES_W*4);ctx->Unmap(tex,0);} float c[4]={0,0,0,1};ctx->ClearRenderTargetView(rtv,c);ctx->OMSetRenderTargets(1,&rtv,nullptr);D3D11_VIEWPORT vp{};RECT r{};GetClientRect(hwnd,&r);float cw=(float)(r.right-r.left),ch=(float)(r.bottom-r.top);float aspect=television?4.f/3.f:float(NES_W)/NES_H;vp.Width=std::min(cw,ch*aspect);vp.Height=vp.Width/aspect;vp.TopLeftX=(cw-vp.Width)*.5f;vp.TopLeftY=(ch-vp.Height)*.5f;vp.MaxDepth=1;ctx->RSSetViewports(1,&vp);ctx->VSSetShader(vs,nullptr,0);ctx->PSSetShader(ps,nullptr,0);float data[12]={tv.contrast,tv.brightness,tv.hue,tv.saturation,tv.width,tv.height,tv.beam,0,television?1.f:0.f,0,0,0};ctx->UpdateSubresource(constants,0,nullptr,data,0,0);ctx->PSSetConstantBuffers(0,1,&constants);ctx->PSSetShaderResources(0,1,&srv);ctx->PSSetSamplers(0,1,&sampler);ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);ctx->Draw(3,0);swap->Present(0,0); }
void Renderer::Shutdown(){ SafeReleaseT(constants);SafeReleaseT(vb);SafeReleaseT(sampler);SafeReleaseT(ps);SafeReleaseT(vs);SafeReleaseT(srv);SafeReleaseT(tex);SafeReleaseT(rtv);SafeReleaseT(swap);SafeReleaseT(ctx);SafeReleaseT(dev); }
