#pragma once
#include "Common.h"
#include <filesystem>
inline std::wstring ReadSetting(const std::wstring& file,const wchar_t* section,const wchar_t* key){std::vector<wchar_t> b(32768);GetPrivateProfileStringW(section,key,L"",b.data(),DWORD(b.size()),file.c_str());return b.data();}
inline void SavePlacement(HWND h,const std::wstring& ini,const wchar_t* sec){
 if(ini.empty())return;WINDOWPLACEMENT p{sizeof(p)};if(!GetWindowPlacement(h,&p))return;
 auto put=[&](const wchar_t* k,int v){WritePrivateProfileStringW(sec,k,std::to_wstring(v).c_str(),ini.c_str());};
 put(L"X",p.rcNormalPosition.left);put(L"Y",p.rcNormalPosition.top);put(L"W",p.rcNormalPosition.right-p.rcNormalPosition.left);put(L"H",p.rcNormalPosition.bottom-p.rcNormalPosition.top);put(L"Max",p.showCmd==SW_SHOWMAXIMIZED);
}
inline void RestorePlacement(HWND h,const std::wstring& ini,const wchar_t* sec,int minW,int minH){
 int w=GetPrivateProfileIntW(sec,L"W",0,ini.c_str());if(!w)return;
 WINDOWPLACEMENT p{sizeof(p)};GetWindowPlacement(h,&p);
 int x=int(GetPrivateProfileIntW(sec,L"X",p.rcNormalPosition.left,ini.c_str())),y=int(GetPrivateProfileIntW(sec,L"Y",p.rcNormalPosition.top,ini.c_str()));
 int ht=int(GetPrivateProfileIntW(sec,L"H",minH,ini.c_str()));w=std::clamp(w,minW,10000);ht=std::clamp(ht,minH,10000);
 RECT r{x,y,x+w,y+ht};MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromRect(&r,MONITOR_DEFAULTTONEAREST),&mi);
 x=std::clamp(x,int(mi.rcWork.left),std::max(int(mi.rcWork.left),int(mi.rcWork.right)-w));y=std::clamp(y,int(mi.rcWork.top),std::max(int(mi.rcWork.top),int(mi.rcWork.bottom)-ht));
 p.rcNormalPosition={x,y,x+w,y+ht};p.showCmd=GetPrivateProfileIntW(sec,L"Max",0,ini.c_str())?SW_SHOWMAXIMIZED:SW_SHOWNORMAL;SetWindowPlacement(h,&p);
}
inline void CreateUnicodeIni(const std::wstring& path){HANDLE f=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(f!=INVALID_HANDLE_VALUE){WORD bom=0xfeff;DWORD n;WriteFile(f,&bom,2,&n,nullptr);CloseHandle(f);}}
