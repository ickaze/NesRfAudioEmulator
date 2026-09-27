#include "Common.h"
#include "Nes.h"
#include "Audio.h"
#include "Renderer.h"
#include "Input.h"
#include "RfWorker.h"
#include "AdjacentWindow.h"
#include "Knob.h"
#include "TvState.h"
#include "Settings.h"
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <chrono>
#include <cwchar>
#include <fstream>

static RfWorker worker;static AdjacentWindow adjacent;
static Renderer renderer;static AudioEngine audio;static Nes nes;static InputManager input;static RfParams rf;
static HWND mainWindow=nullptr,view=nullptr;static bool paused=false,configuring=false;static float master=.8f;
static float whineVolume=.15f,dischargeVolume=.25f;static TvState tv;static std::array<uint32_t,256*240> lastPicture{};
static RECT screenRect{};static constexpr int ControlCount=22;
static HWND sliders[ControlCount]{},labels[ControlCount]{},modeBox=nullptr,presetBox=nullptr,rfStatus=nullptr;
static std::wstring rfIni,lastRom;
static bool fullscreen=false;static WINDOWPLACEMENT windowedPlacement{sizeof(WINDOWPLACEMENT)};static LONG_PTR windowedStyle=0;
static bool cpuRendering=false;
static int rfQueue=4,audioQueue=4;static bool immediateDisplay=false;
static HMENU performanceMenu=nullptr,rfQueueMenu=nullptr,audioQueueMenu=nullptr;
struct Control {const wchar_t* name;const wchar_t* key;float* value;float lo,hi,scale;const wchar_t* unit;};
static Control controls[]={
 {L"映像→音声の混入",L"Leak",&rf.mix,0,1,100,L"%"},
 {L"音声ローパス",L"AudioCutoff",&rf.cutoff,400,18000,1,L"Hz"},
 {L"受信信号強度",L"Signal",&rf.signal,0,1,100,L"%"},
 {L"同調ずれ",L"Detune",&rf.detune,-1,1,100,L"%"},
 {L"受信ノイズ",L"Noise",&rf.noise,0,1,100,L"%"},
 {L"接触不良の頻度",L"Contact",&rf.contact,0,1,100,L"%"},
 {L"反射・ゴースト",L"Ghost",&rf.ghost,0,.8f,100,L"%"},
 {L"反射遅延",L"GhostDelay",&rf.ghostDelay,1,48,1,L"px"},
 {L"映像帯域",L"Bandwidth",&rf.bandwidth,1,6,1,L"MHz"},
 {L"水平ジッター",L"HJitter",&rf.hJitter,0,1,100,L"%"},
 {L"水平ホールドずれ",L"HHold",&rf.hHold,-1,1,100,L"%"},
 {L"垂直ホールドずれ",L"VHold",&rf.vHold,-1,1,100,L"%"},
 {L"色位相の不安定さ",L"ColorJitter",&rf.colorJitter,0,1,100,L"%"},
 {L"同期回路→音声の漏れ",L"SyncLeak",&rf.syncLeak,0,1,100,L"%"},
 {L"接触変化の時定数",L"Recovery",&rf.recovery,.0001f,.1f,1000,L"ms"},
 {L"電源 / 音量",L"Master",&master,0,1,100,L"%"},
 {L"映像",L"Contrast",&tv.contrast,0,2,100,L"%"},
 {L"明るさ",L"Brightness",&tv.brightness,-.5f,.5f,100,L"%"},
 {L"色合い",L"Hue",&tv.hue,-1.57f,1.57f,57.2958f,L"°"},
 {L"色の濃さ",L"Saturation",&tv.saturation,0,2,100,L"%"},
 {L"本体動作音",L"WhineVolume",&whineVolume,0,1,100,L"%"},
 {L"放電音",L"DischargeVolume",&dischargeVolume,0,1,100,L"%"}
};
enum{ID_OPEN=100,ID_EXIT,ID_INPUT,ID_RESET,ID_PAUSE,ID_MODE,ID_PRESET,ID_ADJACENT,ID_MIX=200,ID_FULLSCREEN=289,ID_RENDER_CPU=290,ID_RENDER_INFO,ID_PERF_NORMAL=300,ID_PERF_LOW,ID_PERF_IMMEDIATE,ID_RF_QUEUE=310,ID_AUDIO_QUEUE=320,ID_TIMER=1};
using Clock=std::chrono::steady_clock;
static auto nextFrame=Clock::now(),nextScan=Clock::now(),nextSave=Clock::now();
static void ApplyPerformance(bool flush){
 worker.SetCapacity(size_t(rfQueue));audio.SetQueueLimit(unsigned(audioQueue));
 if(flush){worker.Flush();audio.Clear();nextFrame=Clock::now();}
 CheckMenuRadioItem(rfQueueMenu,ID_RF_QUEUE+1,ID_RF_QUEUE+4,ID_RF_QUEUE+rfQueue,MF_BYCOMMAND);
 CheckMenuRadioItem(audioQueueMenu,ID_AUDIO_QUEUE+2,ID_AUDIO_QUEUE+4,ID_AUDIO_QUEUE+audioQueue,MF_BYCOMMAND);
 CheckMenuItem(performanceMenu,ID_PERF_IMMEDIATE,MF_BYCOMMAND|(immediateDisplay?MF_CHECKED:MF_UNCHECKED));
 CheckMenuItem(performanceMenu,ID_PERF_NORMAL,MF_BYCOMMAND|((rfQueue==4&&audioQueue==4&&!immediateDisplay)?MF_CHECKED:MF_UNCHECKED));
 CheckMenuItem(performanceMenu,ID_PERF_LOW,MF_BYCOMMAND|((rfQueue==1&&audioQueue==2&&immediateDisplay)?MF_CHECKED:MF_UNCHECKED));
}
static void ToggleFullscreen(HWND h){
 if(!fullscreen){
  windowedPlacement.length=sizeof(windowedPlacement);if(!GetWindowPlacement(h,&windowedPlacement))return;
  MONITORINFO monitor{sizeof(monitor)};if(!GetMonitorInfoW(MonitorFromWindow(h,MONITOR_DEFAULTTONEAREST),&monitor))return;
  windowedStyle=GetWindowLongPtrW(h,GWL_STYLE);fullscreen=true;
  SetWindowLongPtrW(h,GWL_STYLE,windowedStyle&~WS_OVERLAPPEDWINDOW);
  SetWindowPos(h,nullptr,monitor.rcMonitor.left,monitor.rcMonitor.top,monitor.rcMonitor.right-monitor.rcMonitor.left,monitor.rcMonitor.bottom-monitor.rcMonitor.top,SWP_NOZORDER|SWP_NOOWNERZORDER|SWP_FRAMECHANGED);
 }else{
  fullscreen=false;SetWindowLongPtrW(h,GWL_STYLE,windowedStyle);SetWindowPlacement(h,&windowedPlacement);
  SetWindowPos(h,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOOWNERZORDER|SWP_FRAMECHANGED);
 }
}
static void UpdateLabels(){
 wchar_t text[160];for(int i=0;i<ControlCount;++i){auto& c=controls[i];swprintf_s(text,L"%.1f %s",double(*c.value*c.scale),c.unit);wchar_t old[160];GetWindowTextW(labels[i],old,160);if(std::wcscmp(old,c.name))SetWindowTextW(labels[i],c.name);GetWindowTextW(sliders[i],old,160);if(std::wcscmp(old,text))SetWindowTextW(sliders[i],text);EnableWindow(sliders[i],rf.enabled||i>=15);}
}
static void SaveRf(){
 if(rfIni.empty())return;
 WritePrivateProfileStringW(L"Performance",L"CpuRendering",cpuRendering?L"1":L"0",rfIni.c_str());
 WritePrivateProfileStringW(L"Performance",L"RfQueue",std::to_wstring(rfQueue).c_str(),rfIni.c_str());
 WritePrivateProfileStringW(L"Performance",L"AudioQueue",std::to_wstring(audioQueue).c_str(),rfIni.c_str());
 WritePrivateProfileStringW(L"Performance",L"ImmediateDisplay",immediateDisplay?L"1":L"0",rfIni.c_str());
 bool ok=WritePrivateProfileStringW(L"RF",L"Enabled",rf.enabled?L"1":L"0",rfIni.c_str())!=0;
 for(auto& c:controls)ok&=WritePrivateProfileStringW(L"RF",c.key,std::to_wstring(*c.value).c_str(),rfIni.c_str())!=0;
 WritePrivateProfileStringW(L"Session",L"Rom",lastRom.c_str(),rfIni.c_str());
 WritePrivateProfileStringW(L"Session",L"Power",tv.on?L"1":L"0",rfIni.c_str());
 WritePrivateProfileStringW(L"Session",L"Paused",paused?L"1":L"0",rfIni.c_str());
 if(!ok)SetWindowTextW(rfStatus,L"RF設定を保存できません");
}
static void SyncControls(){
 for(int i=0;i<ControlCount;++i){auto& c=controls[i];int pos=int(std::clamp((*c.value-c.lo)/(c.hi-c.lo),0.f,1.f)*1000+.5f);SendMessageW(sliders[i],TBM_SETPOS,TRUE,pos);}
 SendMessageW(modeBox,CB_SETCURSEL,rf.enabled?1:0,0);UpdateLabels();audio.SetMaster(master*tv.sound);
}
static void LoadRf(){
 rf.enabled=GetPrivateProfileIntW(L"RF",L"Enabled",0,rfIni.c_str())!=0;
 for(auto& c:controls){wchar_t value[80];GetPrivateProfileStringW(L"RF",c.key,L"",value,80,rfIni.c_str());if(*value){wchar_t* end=nullptr;float v=std::wcstof(value,&end);if(end!=value&&!*end&&std::isfinite(v))*c.value=std::clamp(v,c.lo,c.hi);}}
 SyncControls();
}
static void ReadSliders(){
 for(int i=0;i<ControlCount;++i){auto& c=controls[i];*c.value=c.lo+(c.hi-c.lo)*float(SendMessageW(sliders[i],TBM_GETPOS,0,0))/1000.f;}
 audio.SetMaster(master*tv.sound);UpdateLabels();SendMessageW(presetBox,CB_SETCURSEL,0,0);
}
static void Preset(int index){
 if(index<0)return;rf=RfParams();rf.enabled=true;
 if(index==1){rf.mix=.65f;rf.syncLeak=.35f;rf.cutoff=6500;}
 if(index==2){rf.signal=.6f;rf.contact=.6f;rf.noise=.12f;rf.detune=.15f;rf.recovery=.001f;rf.mix=.4f;}
 if(index==3){rf.signal=.5f;rf.noise=.10f;rf.hJitter=.2f;rf.hHold=.75f;rf.vHold=.2f;rf.colorJitter=.35f;rf.syncLeak=.4f;}
 SyncControls();SaveRf();worker.Flush();audio.Clear();
}
static void Load(const std::wstring& path){
 worker.Flush();audio.Clear();std::wstring error;if(!nes.LoadRom(path,error))MessageBoxW(mainWindow,error.c_str(),L"ROM load error",MB_ICONERROR);
 else{lastRom=path;SetWindowTextW(mainWindow,(L"NES RF Audio Emulator - "+path).c_str());paused=false;}
 nextFrame=Clock::now();
}
static void OpenRom(){
 wchar_t file[32768]{};OPENFILENAMEW ofn{};ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=mainWindow;ofn.lpstrFilter=L"NES / FDS / UNIF\0*.nes;*.fds;*.unf;*.unif\0All files\0*.*\0";ofn.lpstrFile=file;ofn.nMaxFile=32768;ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
 configuring=true;worker.Flush();audio.Clear();if(GetOpenFileNameW(&ofn))Load(file);configuring=false;nextFrame=Clock::now();
}
static void Layout(HWND h){
 RECT r{};GetClientRect(h,&r);if(r.right<1||r.bottom<1)return;SendMessageW(h,WM_SETREDRAW,FALSE,0);int panel=420,x=int(r.right)-panel-24;
 int sw=std::max(320,x-70),sh=std::min(int(r.bottom)-230,sw*3/4);sw=sh*4/3;
 screenRect={40,64,40+sw,64+sh};MoveWindow(view,40,64,sw,sh,TRUE);
 HRGN glass=CreateRoundRectRgn(0,0,sw+1,sh+1,60,60);if(!SetWindowRgn(view,glass,TRUE))DeleteObject(glass);
 MoveWindow(modeBox,x+8,28,200,200,TRUE);MoveWindow(presetBox,x+216,28,202,200,TRUE);
 MoveWindow(labels[15],x+14,64,190,25,TRUE);MoveWindow(sliders[15],x+48,91,118,114,TRUE);
 int rowHeight=std::clamp((int(r.bottom)-274)/5,58,98);
 for(int i=0;i<15;++i){int cx=x+8+(i%3)*132,cy=254+(i/3)*rowHeight;MoveWindow(labels[i],cx,cy,128,22,TRUE);MoveWindow(sliders[i],cx+14,cy+26,100,rowHeight-28,TRUE);}
 for(int i=16;i<20;++i){int cx=40+(i-16)*sw/4;MoveWindow(labels[i],cx,sh+154,sw/4-8,28,TRUE);MoveWindow(sliders[i],cx+sw/8-48,sh+116,96,28,TRUE);}
 for(int i=20;i<22;++i){int cx=x+216+(i-20)*100;MoveWindow(labels[i],cx,68,98,36,TRUE);MoveWindow(sliders[i],cx+14,108,68,64,TRUE);}
 MoveWindow(rfStatus,40,int(r.bottom)-42,std::max(300,x-60),38,TRUE);SendMessageW(h,WM_SETREDRAW,TRUE,0);RedrawWindow(h,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
}
static void PaintCabinet(HWND h,HDC d){
 RECT r;GetClientRect(h,&r);KnobFill(d,r,RGB(119,20,27));
 RECT inner{10,10,r.right-10,r.bottom-10};KnobFill(d,inner,RGB(172,39,43));
 RECT shine{14,14,r.right-14,20};KnobFill(d,shine,RGB(219,80,76));
 RECT bezel{screenRect.left-14,screenRect.top-14,screenRect.right+14,screenRect.bottom+14};KnobFill(d,bezel,RGB(29,25,24));
 RECT panel{r.right-452,58,r.right-24,r.bottom-20};KnobFill(d,panel,RGB(42,35,34));
 RECT tuning{32,screenRect.bottom+35,screenRect.right+8,screenRect.bottom+125};KnobFill(d,tuning,RGB(42,35,34));
 SetBkMode(d,TRANSPARENT);SetTextColor(d,RGB(246,220,183));
 RECT title{40,24,500,46};DrawTextW(d,L"N E S   •   C O L O R   1 4",-1,&title,DT_LEFT|DT_SINGLELINE);
 RECT hint{r.right-436,211,r.right-36,244};DrawTextW(d,L"電源  ↑ OFF / ↓ ON    音量  ← →",-1,&hint,DT_CENTER|DT_SINGLELINE);
 int sx=r.right-218;for(int y=182;y<200;y+=7){RECT slot{sx,y,r.right-48,y+3};KnobFill(d,slot,RGB(13,12,12));}
}
static void Pause(){paused=!paused;worker.Flush();audio.Clear();nextFrame=Clock::now();CheckMenuItem(GetMenu(mainWindow),ID_PAUSE,MF_BYCOMMAND|(paused?MF_CHECKED:MF_UNCHECKED));}
static LRESULT CALLBACK ViewProc(HWND h,UINT m,WPARAM w,LPARAM l){
 if(m==WM_SIZE){renderer.Resize(LOWORD(l),HIWORD(l));return 0;}
 if(m==WM_LBUTTONDOWN){SetFocus(mainWindow);return 0;}
 if(m==WM_PAINT){PAINTSTRUCT ps;BeginPaint(h,&ps);EndPaint(h,&ps);return 0;}return DefWindowProcW(h,m,w,l);
}
static LRESULT CALLBACK MainProc(HWND h,UINT m,WPARAM w,LPARAM l){switch(m){
 case WM_CREATE:{
 mainWindow=h;HMENU menu=CreateMenu(),file=CreatePopupMenu(),settings=CreatePopupMenu();AppendMenuW(file,MF_STRING,ID_OPEN,L"Open ROM...\tCtrl+O");AppendMenuW(file,MF_STRING,ID_RESET,L"Reset\tF2");AppendMenuW(file,MF_STRING,ID_PAUSE,L"Pause\tSpace");AppendMenuW(file,MF_SEPARATOR,0,nullptr);AppendMenuW(file,MF_STRING,ID_EXIT,L"Exit");AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(file),L"File");AppendMenuW(settings,MF_STRING,ID_INPUT,L"Controllers / Keys...\tF3");AppendMenuW(settings,MF_STRING,ID_ADJACENT,L"隣接チャンネルの動画...\tF4");AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(settings),L"Settings");AppendMenuW(settings,MF_STRING,ID_RENDER_CPU,L"ブラウン管演出をCPU処理に切り替え");AppendMenuW(settings,MF_STRING,ID_RENDER_INFO,L"描画情報（GPU / シェーダー）...");performanceMenu=CreatePopupMenu();rfQueueMenu=CreatePopupMenu();audioQueueMenu=CreatePopupMenu();
 AppendMenuW(performanceMenu,MF_STRING,ID_PERF_NORMAL,L"従来設定（初期値・安定動作優先）");
 AppendMenuW(performanceMenu,MF_STRING,ID_PERF_LOW,L"低遅延設定（v0.7.1相当）");AppendMenuW(performanceMenu,MF_SEPARATOR,0,nullptr);
 for(int i=1;i<=4;++i){auto name=std::to_wstring(i)+L" フレーム";AppendMenuW(rfQueueMenu,MF_STRING,ID_RF_QUEUE+i,name.c_str());}
 for(int i=2;i<=4;++i){auto name=std::to_wstring(i)+L" フレーム";AppendMenuW(audioQueueMenu,MF_STRING,ID_AUDIO_QUEUE+i,name.c_str());}
 AppendMenuW(performanceMenu,MF_POPUP,reinterpret_cast<UINT_PTR>(rfQueueMenu),L"RF処理の最大待機数");
 AppendMenuW(performanceMenu,MF_POPUP,reinterpret_cast<UINT_PTR>(audioQueueMenu),L"ゲーム音声バッファ数");
 AppendMenuW(performanceMenu,MF_STRING,ID_PERF_IMMEDIATE,L"完成した映像を即時表示");
 AppendMenuW(settings,MF_POPUP,reinterpret_cast<UINT_PTR>(performanceMenu),L"処理速度 / 表示遅延");SetMenu(h,menu);
 view=CreateWindowExW(0,L"NesView",nullptr,WS_CHILD|WS_VISIBLE,0,0,640,480,h,nullptr,GetModuleHandleW(nullptr),nullptr);
 modeBox=CreateWindowW(L"COMBOBOX",L"",WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,0,0,235,200,h,reinterpret_cast<HMENU>(INT_PTR(ID_MODE)),nullptr,nullptr);
 SendMessageW(modeBox,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"デジタル出力（映像・音声）"));SendMessageW(modeBox,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"RF出力（映像・音声連動）"));
 presetBox=CreateWindowW(L"COMBOBOX",L"",WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,0,0,235,200,h,reinterpret_cast<HMENU>(INT_PTR(ID_PRESET)),nullptr,nullptr);
 for(auto name:{L"プリセットを選択...",L"安定したRF",L"映像由来のバズが強い",L"接触不良",L"同期不安定"})SendMessageW(presetBox,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name));
 SendMessageW(presetBox,CB_SETCURSEL,0,0);
 for(int i=0;i<ControlCount;++i){labels[i]=CreateWindowW(L"STATIC",L"",WS_CHILD|WS_VISIBLE|SS_OWNERDRAW,0,0,235,23,h,nullptr,nullptr,nullptr);sliders[i]=CreateWindowExW(0,L"TvKnob",nullptr,WS_CHILD|WS_VISIBLE|WS_TABSTOP,0,0,235,30,h,reinterpret_cast<HMENU>(INT_PTR(ID_MIX+i)),nullptr,nullptr);SendMessageW(sliders[i],TBM_SETRANGE,TRUE,MAKELONG(0,1000));SendMessageW(sliders[i],KNOB_STYLE,i==15?2:(i>=16&&i<20?1:0),0);}
 rfStatus=CreateWindowW(L"TvText",L"",WS_CHILD|WS_VISIBLE,0,0,482,45,h,nullptr,nullptr,nullptr);
 wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);auto dataDir=std::filesystem::path(executable).parent_path();
 rfIni=(dataDir/L"NesRfAudioEmulator.ini").wstring();bool fresh=GetFileAttributesW(rfIni.c_str())==INVALID_FILE_ATTRIBUTES;CreateUnicodeIni(rfIni);
 if(fresh){wchar_t local[MAX_PATH]{};if(SUCCEEDED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,local))){auto old=std::filesystem::path(local)/L"NesRfAudioEmulator";
  for(const wchar_t* section:{L"RF",L"Player1",L"Player2"}){std::vector<wchar_t> values(32768);auto path=old/(std::wstring(section)==L"RF"?L"rf.ini":L"input.ini");if(GetPrivateProfileSectionW(section,values.data(),DWORD(values.size()),path.c_str()))WritePrivateProfileSectionW(section,values.data(),rfIni.c_str());}
 }}
 if(!nes.Initialize(dataDir)){MessageBoxW(h,L"実行ファイルのフォルダーに書き込めません。",L"Initialization error",MB_ICONERROR);return -1;}
 LoadRf();
 rfQueue=std::clamp(int(GetPrivateProfileIntW(L"Performance",L"RfQueue",4,rfIni.c_str())),1,4);
 audioQueue=std::clamp(int(GetPrivateProfileIntW(L"Performance",L"AudioQueue",4,rfIni.c_str())),2,4);
 immediateDisplay=GetPrivateProfileIntW(L"Performance",L"ImmediateDisplay",0,rfIni.c_str())!=0;ApplyPerformance(false);input.Initialize(h,rfIni);adjacent.SetIni(rfIni);
 tv.on=true;SendMessageW(sliders[15],KNOB_POWER,tv.on,0);
 auto logInit=[&](const std::wstring& text){std::ofstream f(dataDir/L"startup.log",std::ios::app|std::ios::binary);if(f){int n=WideCharToMultiByte(CP_UTF8,0,text.data(),int(text.size()),nullptr,0,nullptr,nullptr);std::string utf8(n,0);WideCharToMultiByte(CP_UTF8,0,text.data(),int(text.size()),utf8.data(),n,nullptr,nullptr);f<<utf8<<"\n";}};
 if(!renderer.Initialize(view)){logInit(renderer.Error());MessageBoxW(h,renderer.Error().c_str(),L"描画の初期化に失敗しました",MB_ICONERROR);return -1;}
 cpuRendering=GetPrivateProfileIntW(L"Performance",L"CpuRendering",0,rfIni.c_str())!=0;renderer.SetCpu(cpuRendering);
 CheckMenuItem(menu,ID_RENDER_CPU,MF_BYCOMMAND|(cpuRendering?MF_CHECKED:MF_UNCHECKED));
 SetWindowTextW(rfStatus,renderer.ShortStatus().c_str());logInit(renderer.Status());
 if(!renderer.Notice().empty())logInit(renderer.Notice());
 if(!audio.Initialize()){logInit(audio.Error());MessageBoxW(h,audio.Error().c_str(),L"音声の初期化に失敗しました",MB_ICONERROR);return -1;}
 DragAcceptFiles(h,TRUE);SetTimer(h,ID_TIMER,5,nullptr);Layout(h);return 0;}
 case WM_APP+90:{auto path=ReadSetting(rfIni,L"Session",L"Rom");if(!path.empty()&&std::filesystem::is_regular_file(path))Load(path);
 paused=GetPrivateProfileIntW(L"Session",L"Paused",0,rfIni.c_str())!=0;CheckMenuItem(GetMenu(h),ID_PAUSE,MF_BYCOMMAND|(paused?MF_CHECKED:MF_UNCHECKED));adjacent.Restore(h);return 0;}
 case WM_DRAWITEM:{auto d=reinterpret_cast<DRAWITEMSTRUCT*>(l);if(d->CtlType==ODT_STATIC){HDC target=d->hDC;HDC memory=CreateCompatibleDC(target);HBITMAP bitmap=CreateCompatibleBitmap(target,d->rcItem.right,d->rcItem.bottom);auto previous=SelectObject(memory,bitmap);d->hDC=memory;KnobFill(d->hDC,d->rcItem,RGB(42,35,34));wchar_t text[160];GetWindowTextW(d->hwndItem,text,160);auto f=SelectObject(d->hDC,KnobFont());SetBkMode(d->hDC,TRANSPARENT);SetTextColor(d->hDC,RGB(237,217,185));DrawTextW(d->hDC,text,-1,&d->rcItem,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);SelectObject(d->hDC,f);BitBlt(target,0,0,d->rcItem.right,d->rcItem.bottom,memory,0,0,SRCCOPY);SelectObject(memory,previous);DeleteObject(bitmap);DeleteDC(memory);d->hDC=target;return TRUE;}break;}
 case WM_SIZE:if(w!=SIZE_MINIMIZED)Layout(h);return 0;
 case KNOB_POWER:tv.on=w!=0;return 0;
 case WM_ERASEBKGND:return 1;
 case WM_PAINT:{PAINTSTRUCT ps;HDC target=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);HDC d=CreateCompatibleDC(target);HBITMAP b=CreateCompatibleBitmap(target,std::max(1L,r.right),std::max(1L,r.bottom));auto old=SelectObject(d,b);PaintCabinet(h,d);BitBlt(target,0,0,r.right,r.bottom,d,0,0,SRCCOPY);SelectObject(d,old);DeleteObject(b);DeleteDC(d);EndPaint(h,&ps);return 0;}
 case WM_CTLCOLORSTATIC:{HDC d=reinterpret_cast<HDC>(w);SetBkColor(d,RGB(42,35,34));SetTextColor(d,RGB(237,217,185));static HBRUSH b=CreateSolidBrush(RGB(42,35,34));return reinterpret_cast<LRESULT>(b);}
 case WM_HSCROLL:ReadSliders();if(LOWORD(w)==TB_ENDTRACK){SaveRf();SetFocus(h);}return 0;
 case WM_GETMINMAXINFO:{auto info=reinterpret_cast<MINMAXINFO*>(l);info->ptMinTrackSize.x=1120;info->ptMinTrackSize.y=860;return 0;}
 case WM_COMMAND:
 if((LOWORD(w)>=ID_RF_QUEUE+1&&LOWORD(w)<=ID_RF_QUEUE+4)||(LOWORD(w)>=ID_AUDIO_QUEUE+2&&LOWORD(w)<=ID_AUDIO_QUEUE+4)||LOWORD(w)==ID_PERF_NORMAL||LOWORD(w)==ID_PERF_LOW||LOWORD(w)==ID_PERF_IMMEDIATE){
  int id=LOWORD(w);
  if(id==ID_PERF_NORMAL){rfQueue=4;audioQueue=4;immediateDisplay=false;}
  else if(id==ID_PERF_LOW){rfQueue=1;audioQueue=2;immediateDisplay=true;}
  else if(id==ID_PERF_IMMEDIATE)immediateDisplay=!immediateDisplay;
  else if(id>=ID_AUDIO_QUEUE+2)audioQueue=id-ID_AUDIO_QUEUE;
  else rfQueue=id-ID_RF_QUEUE;
  ApplyPerformance(true);SaveRf();SetFocus(h);return 0;
 }
 switch(LOWORD(w)){
 case ID_FULLSCREEN:ToggleFullscreen(h);break;
 case ID_RENDER_CPU:cpuRendering=!cpuRendering;renderer.SetCpu(cpuRendering);CheckMenuItem(GetMenu(h),ID_RENDER_CPU,MF_BYCOMMAND|(cpuRendering?MF_CHECKED:MF_UNCHECKED));SetWindowTextW(rfStatus,renderer.ShortStatus().c_str());SaveRf();break;
 case ID_RENDER_INFO:MessageBoxW(h,(renderer.Status()+L"\n\nCPU切替はブラウン管演出のみです。RF合成は常にCPU、画面転送はD3D11です。\n"+renderer.Notice()).c_str(),L"描画情報",MB_OK);break;
 case ID_MODE:if(HIWORD(w)==CBN_SELCHANGE){rf.enabled=SendMessageW(modeBox,CB_GETCURSEL,0,0)==1;UpdateLabels();SaveRf();worker.Flush();audio.Clear();SetFocus(h);}break;
 case ID_PRESET:if(HIWORD(w)==CBN_SELCHANGE){Preset(int(SendMessageW(presetBox,CB_GETCURSEL,0,0))-1);SetFocus(h);}break;
 case ID_OPEN:OpenRom();break;case ID_RESET:worker.Flush();audio.Clear();nes.Reset();nextFrame=Clock::now();break;
 case ID_PAUSE:Pause();break;
 case ID_ADJACENT:adjacent.Show(h);break;
 case ID_INPUT:configuring=true;worker.Flush();audio.Clear();input.Configure(h);configuring=false;nextFrame=Clock::now();break;
 case ID_EXIT:SendMessageW(h,WM_CLOSE,0,0);break;}return 0;
 case WM_DROPFILES:{HDROP drop=reinterpret_cast<HDROP>(w);UINT n=DragQueryFileW(drop,0,nullptr,0);std::vector<wchar_t> path(n+1);DragQueryFileW(drop,0,path.data(),n+1);DragFinish(drop);Load(path.data());return 0;}
 case WM_TIMER:{
 if(w!=ID_TIMER)return 0;
 static auto lastTick=Clock::now(),lastPresent=lastTick;auto tick=Clock::now();
 tv.Tick(float(std::chrono::duration<double>(tick-lastTick).count()));lastTick=tick;
 audio.UpdateTvSound(tv.on,whineVolume,dischargeVolume,tv.width);
 audio.SetMaster(master*tv.sound);
 auto refreshPicture=[&](){if(tick-lastPresent>=std::chrono::milliseconds(16)){renderer.SetTv(tv);renderer.Present(lastPicture.data());lastPresent=tick;}};

 if(!immediateDisplay)refreshPicture();
 HWND foreground=GetForegroundWindow();bool appActive=foreground&&(foreground==h||GetAncestor(foreground,GA_ROOTOWNER)==h);
 static bool wasRunning=false;bool running=!configuring&&!paused&&appActive;
 if(!running){if(wasRunning){worker.Flush();audio.Clear();}wasRunning=false;nextFrame=Clock::now();refreshPicture();return 0;}wasRunning=true;
 auto now=Clock::now();if(now>=nextScan){input.Refresh();nextScan=now+std::chrono::seconds(2);}input.Poll();
 nes.SetPad(0,input.State(0,foreground==h));nes.SetPad(1,input.State(1,foreground==h));
 if(!nes.Loaded()){int count=0;while(now>=nextFrame&&count++<2){adjacent.Segment(800);nextFrame+=std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1./60.));}if(now-nextFrame>std::chrono::milliseconds(100))nextFrame=now;refreshPicture();return 0;}
 RfResult result;bool present=false;
 while(audio.CanSubmit()&&worker.Take(result)){audio.Submit(result.audio);present=true;}
 if(present){std::copy(result.video.begin(),result.video.end(),lastPicture.begin());if(immediateDisplay){renderer.SetTv(tv);renderer.Present(lastPicture.data());lastPresent=Clock::now();}wchar_t text[240];swprintf_s(text,L"RFワーカー %.1f ms / 待機 %uフレーム\n%s / 信号 %.0f%% / 推定同期 %.0f%%",result.milliseconds,unsigned(worker.Pending()),rf.enabled?L"RF出力":L"デジタル出力",double(result.status.signal*100),double(result.status.sync*100));SetWindowTextW(rfStatus,(renderer.ShortStatus()+L"\n"+std::wstring(text).substr(0,std::wstring(text).find(L'\n'))).c_str());}
 if(immediateDisplay&&!present)refreshPicture();
 int count=0;while(now>=nextFrame&&count<2&&worker.CanSubmit()){
  RfJob job;nes.RunRawFrame(job.audio);std::copy(nes.DigitalFrame(),nes.DigitalFrame()+job.video.size(),job.video.begin());job.params=rf;job.fps=nes.Fps();job.adjacent=adjacent.Segment(job.audio.size());worker.Submit(std::move(job));
  nextFrame+=std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1./nes.Fps()));++count;
 }
 // Bounded backlog: if CPU throughput is insufficient, slow emulation rather
 // than grow latency forever or drop arbitrary audio chunks.
 if(now-nextFrame>std::chrono::milliseconds(100))nextFrame=now;
 if(now>=nextSave){if(!nes.SaveRam())SetWindowTextW(h,L"NES RF Audio Emulator - Save failed (ROM folder not writable)");nextSave=now+std::chrono::seconds(30);}return 0;}
 case WM_CLOSE:if(!nes.SaveRam()&&MessageBoxW(h,L"セーブを保存できませんでした。保存せず終了しますか？",L"Save failed",MB_YESNO|MB_ICONWARNING)!=IDYES)return 0;if(fullscreen)ToggleFullscreen(h);SavePlacement(h,rfIni,L"Window");adjacent.Close();DestroyWindow(h);return 0;
 case WM_DESTROY:SavePlacement(h,rfIni,L"Window");SaveRf();KillTimer(h,ID_TIMER);worker.Stop();adjacent.Close();input.Shutdown();nes.Shutdown();audio.Shutdown();renderer.Shutdown();PostQuitMessage(0);return 0;
 }return DefWindowProcW(h,m,w,l);}
int WINAPI wWinMain(HINSTANCE hi,HINSTANCE,LPWSTR,int show){
 SetProcessDPIAware();INITCOMMONCONTROLSEX ic{sizeof(ic),ICC_BAR_CLASSES};InitCommonControlsEx(&ic);
 WNDCLASSEXW vc{};vc.cbSize=sizeof(vc);vc.lpfnWndProc=ViewProc;vc.hInstance=hi;vc.lpszClassName=L"NesView";vc.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassExW(&vc);RegisterKnob();
 WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.lpfnWndProc=MainProc;wc.hInstance=hi;wc.lpszClassName=L"NesRfMain";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1);wc.hIcon=LoadIconW(hi,MAKEINTRESOURCEW(101));wc.hIconSm=reinterpret_cast<HICON>(LoadImageW(hi,MAKEINTRESOURCEW(101),IMAGE_ICON,GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),LR_DEFAULTCOLOR));RegisterClassExW(&wc);
 HWND h=CreateWindowExW(0,wc.lpszClassName,L"NES RF Audio Emulator",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1400,940,nullptr,nullptr,hi,nullptr);
 if(!h){worker.Stop();adjacent.Close();input.Shutdown();nes.Shutdown();audio.Shutdown();renderer.Shutdown();return 1;}ShowWindow(h,show);RestorePlacement(h,rfIni,L"Window",1120,860);UpdateWindow(h);PostMessageW(h,WM_APP+90,0,0);
 ACCEL keys[]={{FVIRTKEY|FALT,VK_RETURN,ID_FULLSCREEN},{FVIRTKEY|FCONTROL,'O',ID_OPEN},{FVIRTKEY,VK_F2,ID_RESET},{FVIRTKEY,VK_F3,ID_INPUT},{FVIRTKEY,VK_F4,ID_ADJACENT},{FVIRTKEY,VK_SPACE,ID_PAUSE}};HACCEL accel=CreateAcceleratorTableW(keys,6);
 // Use a waitable timer: WM_TIMER is low priority and has a minimum interval.
 HMODULE multimedia=LoadLibraryW(L"winmm.dll");using Period=UINT(WINAPI*)(UINT);
 auto begin=multimedia?reinterpret_cast<Period>(GetProcAddress(multimedia,"timeBeginPeriod")):nullptr;
 auto end=multimedia?reinterpret_cast<Period>(GetProcAddress(multimedia,"timeEndPeriod")):nullptr;
 bool raised=begin&&end&&begin(1)==0;
 HANDLE ticker=CreateWaitableTimerW(nullptr,FALSE,nullptr);LARGE_INTEGER due;due.QuadPart=-50000;
 if(ticker&&!SetWaitableTimer(ticker,&due,5,nullptr,nullptr,FALSE)){CloseHandle(ticker);ticker=nullptr;}
 if(ticker)KillTimer(h,ID_TIMER);
 MSG msg{};bool quit=false;
 while(!quit){
  DWORD ready=MsgWaitForMultipleObjectsEx(ticker?1:0,ticker?&ticker:nullptr,INFINITE,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
  if(ready==WAIT_FAILED)break;
  if(ticker&&ready==WAIT_OBJECT_0)SendMessageW(h,WM_TIMER,ID_TIMER,0);
  for(int n=0;n<64&&PeekMessageW(&msg,nullptr,0,0,PM_REMOVE);++n){
   if(msg.message==WM_QUIT){quit=true;break;}
   if((msg.message==WM_KEYDOWN&&input.MappedKey(int(msg.wParam)))||!TranslateAcceleratorW(h,accel,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}
  }
 }
 if(ticker){CancelWaitableTimer(ticker);CloseHandle(ticker);}if(raised)end(1);if(multimedia)FreeLibrary(multimedia);
 DestroyAcceleratorTable(accel);return int(msg.wParam);
}
