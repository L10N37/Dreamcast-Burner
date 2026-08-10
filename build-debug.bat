@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\build.ps1" -Configuration Debug
exit /b %ERRORLEVEL%

