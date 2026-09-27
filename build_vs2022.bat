@echo off
setlocal
set "BUILD_CONFIG=%~1"
if not defined BUILD_CONFIG set "BUILD_CONFIG=Release"
if /I "%BUILD_CONFIG%"=="Release" goto config_ok
if /I "%BUILD_CONFIG%"=="Debug" goto config_ok
echo Usage: %~nx0 [Release^|Debug] [VS2022-install-directory]
exit /b 2
:config_ok
if "%~2"=="" goto automatic
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0build_windows.ps1" -Configuration "%BUILD_CONFIG%" -VisualStudioPath "%~2" -MSBuildOnly
exit /b %errorlevel%
:automatic
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0build_windows.ps1" -Configuration "%BUILD_CONFIG%" -MSBuildOnly
exit /b %errorlevel%
