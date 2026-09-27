#include "AdjacentWindow.h"
#include <commctrl.h>
#include <commdlg.h>
#include "Knob.h"
#include "Settings.h"
void AdjacentWindow::Show(HWND owner){
 if(window){ShowWindow(window,SW_RESTORE);SetForegroundWindow(window);return;}
 WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);static HBRUSH red=CreateSolidBrush(RGB(145,28,34));wc.hbrBackground=red;wc.lpfnWndProc=Proc;wc.lpszClassName=L"NesAdjacent";RegisterClassW(&wc);wc.lpfnWndProc=ViewProc;wc.lpszClassName=L"NesAdjacentView";RegisterClassW(&wc);
 window=CreateWindowExW(0,L"NesAdjacent",L"隣接チャンネル - 動画ファイル",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,720,690,owner,nullptr,wc.hInstance,this);
 if(window){closingApp=false;ShowWindow(window,SW_SHOW);RestorePlacement(window,ini,L"VideoWindow",720,440);
 filePath=ReadSetting(ini,L"Video",L"File");if(!filePath.empty()&&std::filesystem::is_regular_file(filePath))media.Open(filePath);
 SendMessageW(videoSlider,TBM_SETPOS,TRUE,std::clamp(int(GetPrivateProfileIntW(L"Video",L"VideoLevel",25,ini.c_str())),0,100));
 SendMessageW(audioSlider,TBM_SETPOS,TRUE,std::clamp(int(GetPrivateProfileIntW(L"Video",L"AudioLevel",25,ini.c_str())),0,100));
 SendMessageW(channel,CB_SETCURSEL,GetPrivateProfileIntW(L"Video",L"Channel",0,ini.c_str())?1:0,0);
 SendMessageW(enabled,BM_SETCHECK,GetPrivateProfileIntW(L"Video",L"Enabled",1,ini.c_str())?BST_CHECKED:BST_UNCHECKED,0);
 SendMessageW(window,WM_HSCROLL,0,0);}

}
void AdjacentWindow::Close(){closingApp=true;if(window)DestroyWindow(window);else media.Close();}
void AdjacentWindow::Open(){
 wchar_t path[32768]{};OPENFILENAMEW ofn{};ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=window;ofn.lpstrFile=path;ofn.nMaxFile=32768;ofn.lpstrFilter=L"動画ファイル\0*.mp4;*.m4v;*.mov;*.wmv;*.avi;*.mkv\0すべてのファイル\0*.*\0";ofn.Flags=OFN_FILEMUSTEXIST|OFN_NOCHANGEDIR;
 if(GetOpenFileNameW(&ofn)){filePath=path;media.Open(path);Save();SetWindowTextW(window,(std::wstring(L"隣接チャンネル - ")+path).c_str());}
}
void AdjacentWindow::Layout(){if(!window)return;RECT r{};GetClientRect(window,&r);MoveWindow(view,10,10,std::max(1L,r.right-20),std::max(1L,r.bottom-185),TRUE);int y=std::max(20L,r.bottom-165);
 MoveWindow(GetDlgItem(window,1),10,y,150,28,TRUE);MoveWindow(GetDlgItem(window,2),170,y,90,28,TRUE);MoveWindow(enabled,275,y,170,28,TRUE);MoveWindow(channel,450,y,240,130,TRUE);
 MoveWindow(GetDlgItem(window,10),10,y+40,160,22,TRUE);MoveWindow(videoSlider,220,y+30,62,62,TRUE);MoveWindow(GetDlgItem(window,11),390,y+40,115,22,TRUE);MoveWindow(audioSlider,540,y+30,62,62,TRUE);MoveWindow(status,10,y+100,std::max(1L,r.right-20),55,TRUE);
}
std::shared_ptr<RfInterference> AdjacentWindow::Segment(size_t samples){
 if(!window)return nullptr;auto result=media.Segment(samples);
 result->videoLevel=float(SendMessageW(videoSlider,TBM_GETPOS,0,0))/100.f;result->audioLevel=float(SendMessageW(audioSlider,TBM_GETPOS,0,0))/100.f;
 result->offsetMHz=SendMessageW(channel,CB_GETCURSEL,0,0)==1?-6.f:6.f;
 if(result->active&&!IsIconic(window))renderer.Present(result->video.data());
 if(SendMessageW(enabled,BM_GETCHECK,0,0)!=BST_CHECKED)result->active=false;
 auto text=media.Status()+L"\n映像混信 "+std::to_wstring(int(result->videoLevel*100))+L"% / 音声混信 "+std::to_wstring(int(result->audioLevel*100))+L"%（RF出力時のみ）";
 SetWindowTextW(status,text.c_str());return result;
}
LRESULT CALLBACK AdjacentWindow::ViewProc(HWND h,UINT m,WPARAM w,LPARAM l){auto self=reinterpret_cast<AdjacentWindow*>(GetWindowLongPtrW(h,GWLP_USERDATA));if(m==WM_SIZE&&self){self->renderer.Resize(LOWORD(l),HIWORD(l));return 0;}if(m==WM_PAINT){PAINTSTRUCT p;BeginPaint(h,&p);EndPaint(h,&p);return 0;}return DefWindowProcW(h,m,w,l);}
LRESULT CALLBACK AdjacentWindow::Proc(HWND h,UINT m,WPARAM w,LPARAM l){
 auto self=reinterpret_cast<AdjacentWindow*>(GetWindowLongPtrW(h,GWLP_USERDATA));if(m==WM_NCCREATE){self=static_cast<AdjacentWindow*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));self->window=h;}
 if(!self)return DefWindowProcW(h,m,w,l);
 switch(m){
 case WM_CREATE:{auto make=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int id){return CreateWindowW(cls,text,WS_CHILD|WS_VISIBLE|style,0,0,100,30,h,reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);};
 self->view=make(L"NesAdjacentView",L"",0,20);SetWindowLongPtrW(self->view,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));make(L"BUTTON",L"動画を開く / 再読込",0,1);make(L"BUTTON",L"停止",0,2);
 self->enabled=make(L"BUTTON",L"混信を有効",BS_AUTOCHECKBOX,3);SendMessageW(self->enabled,BM_SETCHECK,BST_CHECKED,0);
 self->channel=make(L"COMBOBOX",L"",CBS_DROPDOWNLIST,4);SendMessageW(self->channel,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"上隣接 (+6 MHz)"));SendMessageW(self->channel,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"下隣接 (-6 MHz)"));SendMessageW(self->channel,CB_SETCURSEL,0,0);
 make(L"TvText",L"映像の混信量",SS_CENTER|SS_CENTERIMAGE,10);make(L"TvText",L"音声の混信量",SS_CENTER|SS_CENTERIMAGE,11);for(int id:{10,11})SendMessageW(GetDlgItem(h,id),WM_SETFONT,reinterpret_cast<WPARAM>(KnobFont()),TRUE);self->videoSlider=make(L"TvKnob",L"",WS_TABSTOP,12);self->audioSlider=make(L"TvKnob",L"",WS_TABSTOP,13);
 for(HWND slider:{self->videoSlider,self->audioSlider}){SendMessageW(slider,TBM_SETRANGE,TRUE,MAKELONG(0,100));SendMessageW(slider,TBM_SETPOS,TRUE,25);}
 self->status=make(L"TvText",L"動画未選択。元音声は直接再生せずRF混信へ送ります。",0,14);self->Layout();if(!self->renderer.Initialize(self->view))MessageBoxW(h,self->renderer.Error().c_str(),L"動画プレビュー初期化エラー",MB_ICONERROR);return 0;
 }
 case WM_ERASEBKGND:return 1;
 case WM_PAINT:{
  PAINTSTRUCT ps;HDC target=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);
  HDC d=CreateCompatibleDC(target);HBITMAP bitmap=CreateCompatibleBitmap(target,std::max(1L,r.right),std::max(1L,r.bottom));auto old=SelectObject(d,bitmap);
  KnobFill(d,r,RGB(145,28,34));int y=std::max(20L,r.bottom-165);
  RECT panel{10,y+30,r.right-10,r.bottom-10};KnobFill(d,panel,RGB(42,35,34));
  BitBlt(target,0,0,r.right,r.bottom,d,0,0,SRCCOPY);SelectObject(d,old);DeleteObject(bitmap);DeleteDC(d);EndPaint(h,&ps);return 0;
 }
 case WM_CTLCOLORSTATIC:{HDC d=reinterpret_cast<HDC>(w);SetBkColor(d,RGB(145,28,34));SetTextColor(d,RGB(246,220,183));static HBRUSH b=CreateSolidBrush(RGB(145,28,34));return reinterpret_cast<LRESULT>(b);}
 case WM_HSCROLL:{for(HWND knob:{self->videoSlider,self->audioSlider}){auto t=std::to_wstring(SendMessageW(knob,TBM_GETPOS,0,0))+L"%";SetWindowTextW(knob,t.c_str());}self->Save();return 0;}
 case WM_SIZE:self->Layout();RedrawWindow(h,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);return 0;
 case WM_GETMINMAXINFO:{auto p=reinterpret_cast<MINMAXINFO*>(l);p->ptMinTrackSize.x=720;p->ptMinTrackSize.y=440;return 0;}
 case WM_COMMAND:if(LOWORD(w)==1)self->Open();else if(LOWORD(w)==2){self->filePath.clear();self->media.Close();self->Save();SetWindowTextW(self->status,L"停止しました");}return 0;
 case WM_DESTROY:self->Save();if(!self->closingApp)WritePrivateProfileStringW(L"Video",L"Open",L"0",self->ini.c_str());self->media.Close();self->renderer.Shutdown();self->window=nullptr;return 0;
 }
 return DefWindowProcW(h,m,w,l);
}

void AdjacentWindow::Restore(HWND owner){if(GetPrivateProfileIntW(L"Video",L"Open",0,ini.c_str()))Show(owner);}
void AdjacentWindow::Save(){if(ini.empty()||!window)return;SavePlacement(window,ini,L"VideoWindow");
 auto put=[&](const wchar_t* key,int v){WritePrivateProfileStringW(L"Video",key,std::to_wstring(v).c_str(),ini.c_str());};
 put(L"Open",1);
 put(L"VideoLevel",int(SendMessageW(videoSlider,TBM_GETPOS,0,0)));put(L"AudioLevel",int(SendMessageW(audioSlider,TBM_GETPOS,0,0)));
 put(L"Channel",int(SendMessageW(channel,CB_GETCURSEL,0,0)));put(L"Enabled",SendMessageW(enabled,BM_GETCHECK,0,0)==BST_CHECKED);
 WritePrivateProfileStringW(L"Video",L"File",filePath.c_str(),ini.c_str());
}
