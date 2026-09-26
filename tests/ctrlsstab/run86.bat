@echo off
REM x86 build + run of the SSTab fixture. --arch x86 is what actually selects the
REM 32-bit target; without it C3 builds amd64 and this "x86" run re-tests x64.
set VS=D:\Program Files (x86)\Microsoft Visual Studio\2019\Community
set C3=C:\Users\Administrator\Documents\c3.vb6.pro\.build\C3.exe

call "%VS%\VC\Auxiliary\Build\vcvars32.bat" >NUL 2>&1
cd /d "%~dp0"
if not exist x86 mkdir x86
del /q x86\CtrlSSTab.exe 2>NUL

"%C3%" --verbose --arch x86 --output-dir "%CD%\x86" CtrlSSTab.vbp > build86.log 2>&1
echo C3_EXIT=%ERRORLEVEL%

cmd /c "x86\CtrlSSTab.exe > run86.out 2>&1"
echo RUN_EXIT=%ERRORLEVEL%
type run86.out
