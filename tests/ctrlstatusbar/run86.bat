@echo off
REM x86 build + run + manifest check of the StatusBar fixture (32-bit target).
REM  --arch x86 is what selects the 32-bit target: without it C3 builds amd64 and
REM  this "x86" run silently re-tests the x64 exe.
REM  The manifest is injected by mt.exe after link, so rc.exe/mt.exe are found
REM  under <SDK>\bin\<ver>\x64\ and the toolset bits do not matter here.
set VS=D:\Program Files (x86)\Microsoft Visual Studio\2019\Community
set C3=C:\Users\Administrator\Documents\c3.vb6.pro\.build\C3.exe

call "%VS%\VC\Auxiliary\Build\vcvars32.bat" >NUL 2>&1
cd /d "%~dp0"
if not exist x86 mkdir x86
del /q x86\CtrlStatusBar.exe 2>NUL

"%C3%" --verbose --arch x86 --output-dir "%CD%\x86" CtrlStatusBar.vbp > build86.log 2>&1
echo C3_EXIT=%ERRORLEVEL%

cmd /c "x86\CtrlStatusBar.exe > run86.out 2>&1"
echo RUN_EXIT=%ERRORLEVEL%
type run86.out

python ..\resx.py --check x86\CtrlStatusBar.exe "%TEMP%\C3C\*\CtrlStatusBar.c3.manifest"
echo RES_EXIT=%ERRORLEVEL%
