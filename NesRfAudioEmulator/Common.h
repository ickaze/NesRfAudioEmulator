#pragma once
#include <windows.h>
#include <unknwn.h>
#include <cstdint>
#include <vector>
#include <array>
#include <string>
#include <atomic>
#include <mutex>
#include <algorithm>
#include <cmath>
#include <cstring>

inline void SafeRelease(IUnknown*& p){ if(p){ p->Release(); p=nullptr; } }
template<class T> inline void SafeReleaseT(T*& p){ if(p){ p->Release(); p=nullptr; } }
constexpr int NES_W=256, NES_H=240;
