@echo off
REM ASCII only: regenerate no_manifest.res (the .res is checked in; this records how it was made)
set "VSROOT=D:\Program Files (x86)\Microsoft Visual Studio\2019\Community"
set "KITS=D:\Windows Kits\10"
call "%VSROOT%\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
set "PATH=%KITS%\bin\10.0.19041.0\x64;%PATH%"
cd /d "%~dp0"
rc /nologo /r /fo no_manifest.res no_manifest.rc
echo RC-EXIT=%ERRORLEVEL%
