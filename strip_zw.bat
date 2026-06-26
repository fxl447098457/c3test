@echo off
setlocal
if "%~1"=="" (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0strip_zw.ps1"
) else (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0strip_zw.ps1" -Files %*
)
endlocal