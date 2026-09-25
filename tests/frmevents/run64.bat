@echo off
REM x64 build + run of the form-events fixture (P20-43).
set VS=D:\Program Files (x86)\Microsoft Visual Studio\2019\Community
set C3=C:\Users\Administrator\Documents\c3.vb6.pro\.build\C3.exe

call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >NUL 2>&1
cd /d "%~dp0"
del /q FrmEvents.exe 2>NUL

"%C3%" --arch x64 --output-dir "%CD%" FrmEvents.vbp > build64.log 2>&1
echo C3_EXIT=%ERRORLEVEL%

cmd /c "FrmEvents.exe > run64.out 2>&1"
echo RUN_EXIT=%ERRORLEVEL%
type run64.out
