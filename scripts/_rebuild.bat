@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
cd /d C:\Users\vi\Desktop\c3.vb6.pro
cmake --build .build --config Release > .build\build_inc.log 2>&1
echo EXITCODE=%ERRORLEVEL%
