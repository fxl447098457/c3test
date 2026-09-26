@echo off
REM x86 build + run of the form-events fixture (P20-43). --arch x86 selects 32-bit.
set VS=D:\Program Files (x86)\Microsoft Visual Studio\2019\Community
set C3=C:\Users\Administrator\Documents\c3.vb6.pro\.build\C3.exe

call "%VS%\VC\Auxiliary\Build\vcvars32.bat" >NUL 2>&1
cd /d "%~dp0"
if not exist x86 mkdir x86
del /q x86\FrmEvents.exe 2>NUL

"%C3%" --arch x86 --output-dir "%CD%\x86" FrmEvents.vbp > build86.log 2>&1
echo C3_EXIT=%ERRORLEVEL%

cmd /c "x86\FrmEvents.exe > run86.out 2>&1"
echo RUN_EXIT=%ERRORLEVEL%
type run86.out
