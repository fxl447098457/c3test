@echo off
REM x64 build + run + manifest check of the StatusBar fixture.
REM  This is the local directional smoke test for the StatusBar re-implementation
REM  (#10): build -> run the fixture -> verify the app manifest really got into
REM  the binary. The GA run on the CI repo is the gate; this is only a quick check.
set VS=D:\Program Files (x86)\Microsoft Visual Studio\2019\Community
set C3=C:\Users\Administrator\Documents\c3.vb6.pro\.build\C3.exe

call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >NUL 2>&1
cd /d "%~dp0"
del /q CtrlStatusBar.exe 2>NUL

"%C3%" --verbose --arch x64 --output-dir "%CD%" CtrlStatusBar.vbp > build64.log 2>&1
echo C3_EXIT=%ERRORLEVEL%
findstr /C:"MT (manifest)" build64.log

cmd /c "CtrlStatusBar.exe > run64.out 2>&1"
echo RUN_EXIT=%ERRORLEVEL%
type run64.out

python ..\resx.py --check CtrlStatusBar.exe "%TEMP%\C3C\*\CtrlStatusBar.c3.manifest"
echo RES_EXIT=%ERRORLEVEL%
