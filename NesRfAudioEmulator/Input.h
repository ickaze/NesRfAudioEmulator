#pragma once
#include "Common.h"
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <xinput.h>
#include <string>
#include <vector>
#include <array>
class InputManager {
public:
 static constexpr int ActionCount=11;
 bool Initialize(HWND owner,const std::wstring& ini);
 void Shutdown();
 void Refresh();
 void Poll();
 uint16_t State(int player,bool focused)const;
 void Configure(HWND owner);
 bool MappedKey(int vk)const;
private:
 struct Device {std::wstring id,name;IDirectInputDevice8W* di=nullptr;int xi=-1;std::array<bool,8> axes{};std::array<bool,320> down{};};
 struct Player {std::wstring device;std::array<int,ActionCount> key{},button{};};
 IDirectInput8W* direct=nullptr;HMODULE xmodule=nullptr;
 using XGet=DWORD (WINAPI*)(DWORD,XINPUT_STATE*);XGet xget=nullptr;
 HWND owner=nullptr;std::wstring ini;std::vector<Device> devices;std::array<Player,2> players;std::wstring lastActive;
 static BOOL CALLBACK EnumDevice(const DIDEVICEINSTANCEW*,void*);
 static BOOL CALLBACK EnumAxis(const DIDEVICEOBJECTINSTANCEW*,void*);
 static LRESULT CALLBACK ConfigProc(HWND,UINT,WPARAM,LPARAM);
 void Defaults();void Load();bool Save();
 const Device* Selected(int player)const;
 std::wstring BindingName(int code,bool keyboard)const;
 HWND dialog=nullptr,playerBox=nullptr,deviceBox=nullptr,help=nullptr;
 std::array<HWND,ActionCount> keyButtons{},padButtons{};
 std::vector<std::wstring> deviceIds;
 int editPlayer=0,capture=-1;bool captureKey=false,armed=false;
 std::array<bool,320> capturePrevious{};
 void UpdateDialog();void CancelCapture();
};
