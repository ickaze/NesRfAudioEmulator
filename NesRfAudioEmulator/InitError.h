#pragma once
#include "Common.h"
inline std::wstring InitError(const wchar_t* stage,HRESULT hr){
 wchar_t code[32];swprintf_s(code,L"0x%08X",unsigned(hr));
 return std::wstring(stage)+L"\nHRESULT: "+code;
}
inline std::wstring ShaderMessage(const char* bytes,size_t count){
 if(!bytes||!count)return L"";int n=MultiByteToWideChar(CP_UTF8,0,bytes,int(count),nullptr,0);
 std::wstring s(n,L'\0');if(n)MultiByteToWideChar(CP_UTF8,0,bytes,int(count),s.data(),n);
 while(!s.empty()&&s.back()==0)s.pop_back();return s;
}
