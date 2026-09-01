@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
if errorlevel 1 ( echo VCVARS_FAIL & exit /b 1 )
set DIR=C:\Users\vi\AppData\Local\Temp\C3C\20655277328600
cl /nologo /c /I"%DIR%\rtl" /I"%DIR%" /O2 /std:c11 /DUNICODE /D_UNICODE /utf-8 /D_CRT_SECURE_NO_WARNINGS /D_CRT_NONSTDC_NO_WARNINGS "%DIR%\cAsyncSocket.c" /Fo:C:\Users\vi\AppData\Local\Temp\C3C\cAsyncSocket_test.obj 2>&1
echo CL_EXIT=%ERRORLEVEL%
