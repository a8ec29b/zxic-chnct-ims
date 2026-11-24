@echo off
chcp 65001 >nul
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0deploy\host-windows.ps1" uninstall
set "result=%errorlevel%"
pause
exit /b %result%
