@echo off
REM x64 build + run of the SSTab fixture. Local directional smoke test only;
REM the GA run on the CI repo is the gate.
set VS=D:\Program Files (x86)\Microsoft Visual Studio\2019\Community
set C3=C:\Users\Administrator\Documents\c3.vb6.pro\.build\C3.exe

call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >NUL 2>&1
cd /d "%~dp0"
del /q CtrlSSTab.exe 2>NUL

"%C3%" --verbose --arch x64 --output-dir "%CD%" CtrlSSTab.vbp > build64.log 2>&1
echo C3_EXIT=%ERRORLEVEL%

cmd /c "CtrlSSTab.exe > run64.out 2>&1"
echo RUN_EXIT=%ERRORLEVEL%
type run64.out
