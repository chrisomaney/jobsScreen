@echo off
REM One-shot Windows flash for Kids Jobs panel (env ws7, short path C:\js).
REM Double-click or run from cmd:
REM   FLASH.bat
REM First time / wrong tree: FLASH.bat bootstrap

setlocal
cd /d "%~dp0"

if /I "%~1"=="bootstrap" (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\windows_flash.ps1" -Bootstrap
  exit /b %ERRORLEVEL%
)

REM If this copy has no [env:ws7], force bootstrap into C:\js
findstr /R /C:"^\[env:ws7\]" "%~dp0platformio.ini" >nul 2>&1
if errorlevel 1 (
  echo platformio.ini has no [env:ws7] — bootstrapping C:\js from fix branch...
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\windows_flash.ps1" -Bootstrap
  exit /b %ERRORLEVEL%
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\windows_flash.ps1" -Upload
exit /b %ERRORLEVEL%
