#pragma once
#include "Common.h"
#include <commctrl.h>
#include <windowsx.h>
// Trackbar-compatible rotary control: shared by RF and adjacent-channel panels.
// Style 1 exposes only the bottom quarter of a wheel; style 2 is power/volume.
constexpr UINT KNOB_STYLE=WM_APP+60,KNOB_POWER=WM_APP+61;
struct KnobData{int value=0,maximum=1000,style=0,x=0,y=0,initial=0,axis=0;bool dragging=false,on=true,keyboardFocus=false;};
inline void KnobFill(HDC d,RECT r,COLORREF c){HBRUSH b=CreateSolidBrush(c);FillRect(d,&r,b);DeleteObject(b);}
inline HFONT KnobFont(){static HFONT f=CreateFontW(-11,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Meiryo UI");return f;}
inline LRESULT CALLBACK KnobProc(HWND h,UINT m,WPARAM w,LPARAM l){
 auto p=reinterpret_cast<KnobData*>(GetWindowLongPtrW(h,GWLP_USERDATA));
 if(m==WM_NCCREATE){p=new KnobData;SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(p));}
 if(!p)return DefWindowProcW(h,m,w,l);
 auto change=[&](int v){p->value=std::clamp(v,0,p->maximum);InvalidateRect(h,nullptr,FALSE);SendMessageW(GetParent(h),WM_HSCROLL,MAKEWPARAM(TB_THUMBTRACK,p->value),reinterpret_cast<LPARAM>(h));};
 switch(m){
 case WM_SETTEXT:{auto result=DefWindowProcW(h,m,w,l);InvalidateRect(h,nullptr,FALSE);return result;}
 case KNOB_STYLE:p->style=int(w);InvalidateRect(h,nullptr,FALSE);return 0;
 case KNOB_POWER:p->on=w!=0;InvalidateRect(h,nullptr,FALSE);return 0;
 case TBM_SETRANGE:p->maximum=std::max(1,int(HIWORD(l)));return 0;
 case TBM_SETPOS:p->value=std::clamp(int(l),0,p->maximum);InvalidateRect(h,nullptr,FALSE);return 0;
 case TBM_GETPOS:return p->value;
 case WM_LBUTTONDOWN:SetFocus(h);p->keyboardFocus=false;InvalidateRect(h,nullptr,FALSE);SetCapture(h);p->dragging=true;p->x=GET_X_LPARAM(l);p->y=GET_Y_LPARAM(l);p->initial=p->value;p->axis=0;return 0;
 case WM_MOUSEMOVE:if(p->dragging){int dx=GET_X_LPARAM(l)-p->x,dy=GET_Y_LPARAM(l)-p->y;
  if(!p->axis&&(std::abs(dx)>5||std::abs(dy)>5))p->axis=(p->style==2&&std::abs(dy)>std::abs(dx))?2:1;
  if(p->axis==1)change(p->initial+dx*p->maximum/160);
  if(p->axis==2&&std::abs(dy)>18){bool next=dy>0;if(next!=p->on){p->on=next;InvalidateRect(h,nullptr,FALSE);SendMessageW(GetParent(h),KNOB_POWER,next,reinterpret_cast<LPARAM>(h));}}
 }return 0;
 case WM_LBUTTONUP:if(p->dragging){p->dragging=false;ReleaseCapture();SendMessageW(GetParent(h),WM_HSCROLL,TB_ENDTRACK,reinterpret_cast<LPARAM>(h));}return 0;
 case WM_CAPTURECHANGED:p->dragging=false;return 0;
 case WM_MOUSEWHEEL:change(p->value+GET_WHEEL_DELTA_WPARAM(w)*p->maximum/120/50);SendMessageW(GetParent(h),WM_HSCROLL,TB_ENDTRACK,reinterpret_cast<LPARAM>(h));return 0;
 case WM_SETFOCUS:p->keyboardFocus=true;InvalidateRect(h,nullptr,FALSE);return 0;
 case WM_KILLFOCUS:p->keyboardFocus=false;InvalidateRect(h,nullptr,FALSE);return 0;
 case WM_KEYDOWN:p->keyboardFocus=true;InvalidateRect(h,nullptr,FALSE);if(w==VK_LEFT||w==VK_RIGHT)change(p->value+(w==VK_RIGHT?1:-1)*std::max(1,p->maximum/50));else if(p->style==2&&(w==VK_UP||w==VK_DOWN)){p->on=w==VK_DOWN;SendMessageW(GetParent(h),KNOB_POWER,p->on,reinterpret_cast<LPARAM>(h));InvalidateRect(h,nullptr,FALSE);}return 0;
 case WM_ERASEBKGND:return 1;
 case WM_ENABLE:InvalidateRect(h,nullptr,FALSE);return 0;
 case WM_PAINT:{PAINTSTRUCT ps;HDC target=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);int W=r.right,H=r.bottom;
  HDC d=CreateCompatibleDC(target);HBITMAP bmp=CreateCompatibleBitmap(target,std::max(1,W),std::max(1,H));auto old=SelectObject(d,bmp);
  KnobFill(d,r,RGB(42,35,34));int rad=p->style==1?std::min(W/2-5,H*2):std::min(W/2-8,H/2-5);rad=std::max(2,rad);int cx=W/2,cy=p->style==1?-rad/2:H/2;
  auto circle=[&](int rr,COLORREF c){HBRUSH b=CreateSolidBrush(c);auto ob=SelectObject(d,b);auto op=SelectObject(d,GetStockObject(NULL_PEN));Ellipse(d,cx-rr,cy-rr,cx+rr,cy+rr);SelectObject(d,op);SelectObject(d,ob);DeleteObject(b);};
  circle(rad,RGB(9,8,8));circle(rad-2,RGB(156,148,128));circle(rad-5,RGB(37,36,33));
  double turn=(double(p->value)/p->maximum*1.5-.75)*3.141592653589793;
  HPEN pen=CreatePen(PS_SOLID,1,RGB(95,91,79));auto op=SelectObject(d,pen);
  for(int n=0;n<48;++n){double a=n*6.283185307/48+turn;MoveToEx(d,cx+int(std::sin(a)*(rad-11)),cy-int(std::cos(a)*(rad-11)),nullptr);LineTo(d,cx+int(std::sin(a)*(rad-5)),cy-int(std::cos(a)*(rad-5)));}
  SelectObject(d,op);DeleteObject(pen);pen=CreatePen(PS_SOLID,3,IsWindowEnabled(h)?RGB(243,215,165):RGB(100,96,89));op=SelectObject(d,pen);
  MoveToEx(d,cx+int(std::sin(turn)*(rad*.35)),cy-int(std::cos(turn)*(rad*.35)),nullptr);LineTo(d,cx+int(std::sin(turn)*(rad-13)),cy-int(std::cos(turn)*(rad-13)));SelectObject(d,op);DeleteObject(pen);
  if(p->style==2){RECT led{W-14,5,W-6,13};KnobFill(d,led,p->on?RGB(248,106,54):RGB(67,37,27));}
  wchar_t value[80];GetWindowTextW(h,value,80);auto font=SelectObject(d,KnobFont());SetBkMode(d,OPAQUE);SetBkColor(d,RGB(42,35,34));SetTextColor(d,RGB(247,226,194));RECT text{0,0,W,15};DrawTextW(d,value,-1,&text,DT_CENTER|DT_SINGLELINE|DT_NOPREFIX);SelectObject(d,font);
  if(GetFocus()==h&&p->keyboardFocus){RECT f{2,2,W-2,H-2};DrawFocusRect(d,&f);}
  BitBlt(target,0,0,W,H,d,0,0,SRCCOPY);SelectObject(d,old);DeleteObject(bmp);DeleteDC(d);EndPaint(h,&ps);return 0;}
 case WM_NCDESTROY:delete p;SetWindowLongPtrW(h,GWLP_USERDATA,0);break;
 }return DefWindowProcW(h,m,w,l);
}
// Buffered text controls never erase their background before drawing text.
inline LRESULT CALLBACK TvTextProc(HWND h,UINT m,WPARAM w,LPARAM l){
 if(m==WM_ERASEBKGND)return 1;
 if(m==WM_SETTEXT){
  int n=GetWindowTextLengthW(h);std::vector<wchar_t> old(size_t(n)+1);GetWindowTextW(h,old.data(),n+1);
  const wchar_t* next=l?reinterpret_cast<const wchar_t*>(l):L"";
  if(std::wcscmp(old.data(),next)==0)return TRUE;
  auto result=DefWindowProcW(h,m,w,l);InvalidateRect(h,nullptr,FALSE);return result;
 }
 if(m==WM_PAINT){
  PAINTSTRUCT ps;HDC target=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);
  HDC d=CreateCompatibleDC(target);HBITMAP bitmap=CreateCompatibleBitmap(target,std::max(1L,r.right),std::max(1L,r.bottom));auto old=SelectObject(d,bitmap);
  KnobFill(d,r,RGB(42,35,34));SetBkMode(d,TRANSPARENT);SetTextColor(d,RGB(237,217,185));
  bool centered=(GetWindowLongPtrW(h,GWL_STYLE)&SS_TYPEMASK)==SS_CENTER;
  auto font=SelectObject(d,centered?KnobFont():reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));
  int n=GetWindowTextLengthW(h);std::vector<wchar_t> text(size_t(n)+1);GetWindowTextW(h,text.data(),n+1);
  DrawTextW(d,text.data(),n,&r,DT_NOPREFIX|(centered?(DT_CENTER|DT_VCENTER|DT_SINGLELINE):(DT_LEFT|DT_WORDBREAK)));
  SelectObject(d,font);BitBlt(target,0,0,r.right,r.bottom,d,0,0,SRCCOPY);SelectObject(d,old);DeleteObject(bitmap);DeleteDC(d);EndPaint(h,&ps);return 0;
 }
 return DefWindowProcW(h,m,w,l);
}
inline void RegisterKnob(){WNDCLASSW c{};c.lpfnWndProc=KnobProc;c.hInstance=GetModuleHandleW(nullptr);c.hCursor=LoadCursorW(nullptr,IDC_SIZEWE);c.lpszClassName=L"TvKnob";RegisterClassW(&c);c.lpfnWndProc=TvTextProc;c.hCursor=LoadCursorW(nullptr,IDC_ARROW);c.lpszClassName=L"TvText";RegisterClassW(&c);}
