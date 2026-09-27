#include "Input.h"
#include <libretro.h>
#include <cwchar>
#include <map>
static const wchar_t* actions[]={L"A",L"B",L"SELECT",L"START",L"UP",L"DOWN",L"LEFT",L"RIGHT",L"FDS: Side",L"FDS: Eject",L"VS: Coin"};
static const unsigned retroIds[]={8,0,2,3,4,5,6,7,10,11,13};
static const WORD xbuttons[]={XINPUT_GAMEPAD_A,XINPUT_GAMEPAD_B,XINPUT_GAMEPAD_X,XINPUT_GAMEPAD_Y,XINPUT_GAMEPAD_BACK,XINPUT_GAMEPAD_START,XINPUT_GAMEPAD_LEFT_SHOULDER,XINPUT_GAMEPAD_RIGHT_SHOULDER,XINPUT_GAMEPAD_LEFT_THUMB,XINPUT_GAMEPAD_RIGHT_THUMB,XINPUT_GAMEPAD_DPAD_UP,XINPUT_GAMEPAD_DPAD_DOWN,XINPUT_GAMEPAD_DPAD_LEFT,XINPUT_GAMEPAD_DPAD_RIGHT};
void InputManager::Defaults(){
 players[0].device=L"auto";players[1].device=L"none";
 players[0].key={'Z','X',VK_RSHIFT,VK_RETURN,VK_UP,VK_DOWN,VK_LEFT,VK_RIGHT,VK_F6,VK_F7,VK_F8};
 players[1].key={'G','H','R','T','W','S','A','D',0,0,0};
 for(auto& p:players)p.button={1,2,5,6,11,12,13,14,7,8,16};
}
bool InputManager::Initialize(HWND h,const std::wstring& path){
 owner=h;ini=path;Defaults();Load();
 xmodule=LoadLibraryW(L"xinput1_4.dll");if(!xmodule)xmodule=LoadLibraryW(L"xinput9_1_0.dll");
 if(xmodule)xget=reinterpret_cast<XGet>(GetProcAddress(xmodule,"XInputGetState"));
 DirectInput8Create(GetModuleHandleW(nullptr),DIRECTINPUT_VERSION,IID_IDirectInput8W,reinterpret_cast<void**>(&direct),nullptr);
 Refresh();return true;
}
void InputManager::Shutdown(){for(auto& d:devices)if(d.di){d.di->Unacquire();d.di->Release();}devices.clear();SafeReleaseT(direct);if(xmodule)FreeLibrary(xmodule);xmodule=nullptr;xget=nullptr;}
BOOL CALLBACK InputManager::EnumAxis(const DIDEVICEOBJECTINSTANCEW* object,void* context){
 auto device=static_cast<Device*>(context);auto d=device->di;const DWORD offsets[]={DIJOFS_X,DIJOFS_Y,DIJOFS_Z,DIJOFS_RX,DIJOFS_RY,DIJOFS_RZ,DIJOFS_SLIDER(0),DIJOFS_SLIDER(1)};for(int i=0;i<8;++i)if(object->dwOfs==offsets[i])device->axes[i]=true;DIPROPRANGE range{};range.diph.dwSize=sizeof(range);range.diph.dwHeaderSize=sizeof(range.diph);range.diph.dwHow=DIPH_BYID;range.diph.dwObj=object->dwType;range.lMin=-1000;range.lMax=1000;d->SetProperty(DIPROP_RANGE,&range.diph);return DIENUM_CONTINUE;
}
BOOL CALLBACK InputManager::EnumDevice(const DIDEVICEINSTANCEW* instance,void* context){
 auto self=static_cast<InputManager*>(context);Device d;wchar_t guid[64];StringFromGUID2(instance->guidInstance,guid,64);d.id=guid;d.name=std::wstring(L"DirectInput: ")+instance->tszInstanceName;
 if(FAILED(self->direct->CreateDevice(instance->guidInstance,&d.di,nullptr)))return DIENUM_CONTINUE;
 if(FAILED(d.di->SetDataFormat(&c_dfDIJoystick2))||FAILED(d.di->SetCooperativeLevel(self->owner,DISCL_BACKGROUND|DISCL_NONEXCLUSIVE))){d.di->Release();return DIENUM_CONTINUE;}
 d.di->EnumObjects(EnumAxis,&d,DIDFT_AXIS);d.di->Acquire();self->devices.push_back(std::move(d));return DIENUM_CONTINUE;
}
void InputManager::Refresh(){
 std::map<std::wstring,std::array<bool,320>> previous;for(auto& d:devices){previous[d.id]=d.down;if(d.di){d.di->Unacquire();d.di->Release();}}devices.clear();
 if(xget)for(int i=0;i<4;++i){XINPUT_STATE st{};if(xget(i,&st)==ERROR_SUCCESS){Device d;d.xi=i;d.id=L"xinput"+std::to_wstring(i);d.name=L"XInput #"+std::to_wstring(i+1);devices.push_back(std::move(d));}}
 if(direct)direct->EnumDevices(DI8DEVCLASS_GAMECTRL,EnumDevice,this,DIEDFL_ATTACHEDONLY);
 for(auto& d:devices){auto it=previous.find(d.id);if(it!=previous.end())d.down=it->second;}
}
void InputManager::Poll(){
 for(auto& d:devices){auto old=d.down;d.down.fill(false);
  auto axis=[&](int i,LONG value,LONG threshold){d.down[200+i*2]=value<-threshold;d.down[201+i*2]=value>threshold;};
  if(d.xi>=0){XINPUT_STATE s{};if(!xget||xget(d.xi,&s)!=ERROR_SUCCESS)continue;
   for(int i=0;i<14;++i)d.down[i+1]=(s.Gamepad.wButtons&xbuttons[i])!=0;
   d.down[15]=s.Gamepad.bLeftTrigger>64;d.down[16]=s.Gamepad.bRightTrigger>64;
   axis(0,s.Gamepad.sThumbLX,10000);axis(1,-LONG(s.Gamepad.sThumbLY),10000);axis(2,s.Gamepad.sThumbRX,10000);axis(3,-LONG(s.Gamepad.sThumbRY),10000);
  }else{DIJOYSTATE2 s{};if(FAILED(d.di->Poll()))d.di->Acquire();if(FAILED(d.di->GetDeviceState(sizeof(s),&s)))continue;
   for(int i=0;i<128;++i)d.down[i+1]=(s.rgbButtons[i]&128)!=0;
   LONG values[]={s.lX,s.lY,s.lZ,s.lRx,s.lRy,s.lRz,s.rglSlider[0],s.rglSlider[1]};for(int i=0;i<8;++i)if(d.axes[i])axis(i,values[i],350);
   for(int i=0;i<4;++i){DWORD v=s.rgdwPOV[i];if(LOWORD(v)==0xffff)continue;d.down[300+i*4]=v>=31500||v<=4500;d.down[301+i*4]=v>=4500&&v<=13500;d.down[302+i*4]=v>=13500&&v<=22500;d.down[303+i*4]=v>=22500&&v<=31500;}
  }
  for(int i=1;i<320;++i)if(d.down[i]&&!old[i]){lastActive=d.id;break;}
 }
}
const InputManager::Device* InputManager::Selected(int player)const{
 auto id=players[player].device;if(id==L"none")return nullptr;
 if(id==L"auto"){
  const Device* firstX=nullptr;for(auto& d:devices)if(d.xi>=0){if(!firstX)firstX=&d;if(d.id==lastActive)return &d;}if(firstX)return firstX;
  id=lastActive;if(id.empty()&&!devices.empty())return &devices.front();}
 for(auto& d:devices)if(d.id==id)return &d;return nullptr;
}
uint16_t InputManager::State(int player,bool focused)const{
 if(!focused)return 0;auto& p=players[player];auto d=Selected(player);uint16_t bits=0;
 for(int i=0;i<ActionCount;++i){bool down=p.key[i]>0&&(GetAsyncKeyState(p.key[i])&0x8000);
  if(d&&p.button[i]>0&&p.button[i]<320)down|=d->down[p.button[i]];
  // Default XInput directions also accept left stick; custom directions use only their binding.
  if(d&&d->xi>=0&&i>=4&&i<=7&&p.button[i]==11+i-4){int codes[]={202,203,200,201};down|=d->down[codes[i-4]];}
  if(down)bits|=uint16_t(1u<<retroIds[i]);
 }
 if((bits&0x30)==0x30)bits&=~0x30;if((bits&0xc0)==0xc0)bits&=~0xc0;return bits;
}
void InputManager::Load(){
 for(int p=0;p<2;++p){auto section=L"Player"+std::to_wstring(p+1);wchar_t buf[128];GetPrivateProfileStringW(section.c_str(),L"Device",players[p].device.c_str(),buf,128,ini.c_str());players[p].device=buf;
  for(int i=0;i<ActionCount;++i){auto k=L"Key"+std::to_wstring(i),b=L"Button"+std::to_wstring(i);int v=GetPrivateProfileIntW(section.c_str(),k.c_str(),players[p].key[i],ini.c_str());players[p].key[i]=v>=0&&v<256?v:0;v=GetPrivateProfileIntW(section.c_str(),b.c_str(),players[p].button[i],ini.c_str());players[p].button[i]=v>=0&&v<320?v:0;}}
}
bool InputManager::Save(){bool ok=true;for(int p=0;p<2;++p){auto sec=L"Player"+std::to_wstring(p+1);ok&=WritePrivateProfileStringW(sec.c_str(),L"Device",players[p].device.c_str(),ini.c_str())!=0;
 for(int i=0;i<ActionCount;++i){auto k=L"Key"+std::to_wstring(i),b=L"Button"+std::to_wstring(i);ok&=WritePrivateProfileStringW(sec.c_str(),k.c_str(),std::to_wstring(players[p].key[i]).c_str(),ini.c_str())!=0;ok&=WritePrivateProfileStringW(sec.c_str(),b.c_str(),std::to_wstring(players[p].button[i]).c_str(),ini.c_str())!=0;}}return ok;}
std::wstring InputManager::BindingName(int code,bool keyboard)const{
 if(!code)return L"(none)";if(keyboard){wchar_t name[80]{};UINT scan=MapVirtualKeyW(code,MAPVK_VK_TO_VSC);if(code==VK_LEFT||code==VK_RIGHT||code==VK_UP||code==VK_DOWN||code==VK_RCONTROL||code==VK_RMENU||code==VK_INSERT||code==VK_DELETE||code==VK_HOME||code==VK_END||code==VK_PRIOR||code==VK_NEXT)scan|=0x100;GetKeyNameTextW(LONG(scan<<16),name,80);return name[0]?name:L"VK "+std::to_wstring(code);}
 if(code<129)return L"Button "+std::to_wstring(code);
 if(code>=200&&code<216)return L"Axis "+std::to_wstring((code-200)/2+1)+((code&1)?L" +":L" -");
 if(code>=300&&code<316){const wchar_t* dir[]={L"Up",L"Right",L"Down",L"Left"};return L"POV "+std::to_wstring((code-300)/4+1)+L" "+dir[(code-300)%4];}return L"(none)";
}
void InputManager::CancelCapture(){capture=-1;armed=false;SetWindowTextW(help,L"Click a binding, release controls, then press a key/button or move a stick. Esc: cancel, Delete: clear.");}
void InputManager::UpdateDialog(){
 SendMessageW(deviceBox,CB_RESETCONTENT,0,0);deviceIds={L"auto",L"none"};SendMessageW(deviceBox,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Auto (XInput preferred)"));SendMessageW(deviceBox,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Keyboard only"));
 for(auto& d:devices){deviceIds.push_back(d.id);SendMessageW(deviceBox,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(d.name.c_str()));}
 int selected=-1;for(size_t i=0;i<deviceIds.size();++i)if(deviceIds[i]==players[editPlayer].device)selected=int(i);
 if(selected<0){selected=int(deviceIds.size());deviceIds.push_back(players[editPlayer].device);SendMessageW(deviceBox,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"(saved controller disconnected)"));}
 SendMessageW(deviceBox,CB_SETCURSEL,selected,0);
 for(int i=0;i<ActionCount;++i){SetWindowTextW(keyButtons[i],BindingName(players[editPlayer].key[i],true).c_str());SetWindowTextW(padButtons[i],BindingName(players[editPlayer].button[i],false).c_str());}
}
LRESULT CALLBACK InputManager::ConfigProc(HWND h,UINT msg,WPARAM w,LPARAM l){
 auto self=reinterpret_cast<InputManager*>(GetWindowLongPtrW(h,GWLP_USERDATA));
 if(msg==WM_NCCREATE){self=static_cast<InputManager*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
 if(!self)return DefWindowProcW(h,msg,w,l);
 switch(msg){
 case WM_COMMAND:{int id=LOWORD(w);
  if(id==10&&HIWORD(w)==CBN_SELCHANGE){self->CancelCapture();self->editPlayer=int(SendMessageW(self->playerBox,CB_GETCURSEL,0,0));self->UpdateDialog();}
  else if(id==11&&HIWORD(w)==CBN_SELCHANGE){self->CancelCapture();int index=int(SendMessageW(self->deviceBox,CB_GETCURSEL,0,0));if(index>=0&&size_t(index)<self->deviceIds.size())self->players[self->editPlayer].device=self->deviceIds[index];}
  else if(id>=100&&id<100+ActionCount){self->capture=id-100;self->captureKey=true;self->armed=false;SetFocus(h);SetWindowTextW(self->help,L"Release keys, then press the new key. Esc: cancel, Delete: clear.");}
  else if(id>=200&&id<200+ActionCount){self->capture=id-200;self->captureKey=false;self->armed=false;SetFocus(h);SetWindowTextW(self->help,L"Release buttons/sticks, then operate the selected controller. Esc: cancel, Delete: clear.");}
  else if(id==20){self->CancelCapture();self->Refresh();self->UpdateDialog();}
  else if(id==21){self->CancelCapture();self->Defaults();self->UpdateDialog();}
  else if(id==IDOK){if(self->Save())DestroyWindow(h);else MessageBoxW(h,L"設定を保存できません。",L"Save failed",MB_ICONERROR);}
  else if(id==IDCANCEL)DestroyWindow(h);return 0;
 }
 case WM_TIMER:{self->Poll();if(self->capture<0)return 0;
  if(GetAsyncKeyState(VK_ESCAPE)&0x8000){self->CancelCapture();return 0;}
  auto& player=self->players[self->editPlayer];int found=0;
  if(GetAsyncKeyState(VK_DELETE)&0x8000){(self->captureKey?player.key:player.button)[self->capture]=0;self->CancelCapture();self->UpdateDialog();return 0;}
  if(self->captureKey){for(int k=8;k<256;++k){if(k==VK_SHIFT||k==VK_CONTROL||k==VK_MENU)continue;if(GetAsyncKeyState(k)&0x8000){found=k;break;}}}
  else{auto d=self->Selected(self->editPlayer);if(d){if(!self->armed){self->capturePrevious=d->down;self->armed=true;return 0;}for(int b=1;b<320;++b)if(d->down[b]&&!self->capturePrevious[b]){found=b;break;}self->capturePrevious=d->down;}}
  if(!self->armed){if(!found)self->armed=true;return 0;}
  if(found){(self->captureKey?player.key:player.button)[self->capture]=found;self->CancelCapture();self->UpdateDialog();}return 0;
 }
 case WM_CLOSE:DestroyWindow(h);return 0;
 case WM_DESTROY:KillTimer(h,1);self->dialog=nullptr;return 0;
 }
 return DefWindowProcW(h,msg,w,l);
}
void InputManager::Configure(HWND parent){
 auto backup=players;Refresh();WNDCLASSW wc{};wc.lpfnWndProc=ConfigProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"NesInputConfig";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1);RegisterClassW(&wc);
 dialog=CreateWindowExW(WS_EX_DLGMODALFRAME,wc.lpszClassName,L"Controller / Key Settings",WS_CAPTION|WS_SYSMENU, CW_USEDEFAULT,CW_USEDEFAULT,660,565,parent,nullptr,wc.hInstance,this);
 if(!dialog)return;
 auto control=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int width,int height,int id){HWND c=CreateWindowW(cls,text,WS_CHILD|WS_VISIBLE|style,x,y,width,height,dialog,reinterpret_cast<HMENU>(INT_PTR(id)),wc.hInstance,nullptr);SendMessageW(c,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);return c;};
 playerBox=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,12,12,100,100,10);SendMessageW(playerBox,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Player 1"));SendMessageW(playerBox,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Player 2"));editPlayer=0;SendMessageW(playerBox,CB_SETCURSEL,0,0);
 deviceBox=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP|WS_VSCROLL,125,12,505,220,11);
 control(L"STATIC",L"NES",0,15,50,100,20,0);control(L"STATIC",L"Keyboard",0,145,50,190,20,0);control(L"STATIC",L"Gamepad (device-specific)",0,365,50,250,20,0);
 for(int i=0;i<ActionCount;++i){control(L"STATIC",actions[i],0,15,78+i*31,120,24,0);keyButtons[i]=control(L"BUTTON",L"",WS_TABSTOP,140,74+i*31,200,27,100+i);padButtons[i]=control(L"BUTTON",L"",WS_TABSTOP,355,74+i*31,275,27,200+i);}
 help=control(L"STATIC",L"",0,12,422,620,44,0);control(L"BUTTON",L"Refresh devices",WS_TABSTOP,12,477,140,28,20);control(L"BUTTON",L"Defaults (both)",WS_TABSTOP,160,477,140,28,21);control(L"BUTTON",L"Save",WS_TABSTOP,425,477,95,28,IDOK);control(L"BUTTON",L"Cancel",WS_TABSTOP,535,477,95,28,IDCANCEL);
 CancelCapture();UpdateDialog();EnableWindow(parent,FALSE);ShowWindow(dialog,SW_SHOW);SetTimer(dialog,1,20,nullptr);
 MSG m{};while(dialog&&GetMessageW(&m,nullptr,0,0)>0){if(capture>=0||!IsDialogMessageW(dialog,&m)){TranslateMessage(&m);DispatchMessageW(&m);}}
 // Reload committed settings, so closing/cancel discards edits.
 players=backup;Load();EnableWindow(parent,TRUE);SetForegroundWindow(parent);
}

bool InputManager::MappedKey(int vk)const{for(auto& p:players)for(int key:p.key)if(key==vk)return true;return false;}
