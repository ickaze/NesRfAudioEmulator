#pragma once
#include "MediaSource.h"
#include "Renderer.h"
class AdjacentWindow {
public:
 void SetIni(const std::wstring& p){ini=p;}
 void Restore(HWND owner);
 void Show(HWND owner);void Close();
 std::shared_ptr<RfInterference> Segment(size_t samples);
private:
 std::wstring ini,filePath;bool closingApp=false;
 void Save();
 HWND window=nullptr,view=nullptr,videoSlider=nullptr,audioSlider=nullptr,channel=nullptr,enabled=nullptr,status=nullptr;
 Renderer renderer;MediaSource media;
 static LRESULT CALLBACK Proc(HWND,UINT,WPARAM,LPARAM);
 static LRESULT CALLBACK ViewProc(HWND,UINT,WPARAM,LPARAM);
 void Layout();void Open();
};
