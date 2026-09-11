@echo off
REM ============================================================
REM  build_rtl_libs.bat - rebuild RTL static libs (x64 + x86)
REM
REM  Promoted from .temp\build_rtl_libs.bat (that path is .gitignored,
REM  but C3 links these .lib files and they are embedded into C3.exe via
REM  c3rtl.rc, so the build must be reproducible).
REM
REM  Outputs:
REM    src\rtl\lib\vb6rtl.lib      = vb6rtl + vb6com + vb6_di_stubs
REM    src\rtl\lib\vb6rtl_dll.lib  = vb6comserver
REM    src\rtl\lib\vb6rtl_gui.lib  = vb6forms
REM    src\rtl\lib\x86\*.lib       = same, 32-bit
REM
REM  NOTE: keep this file ASCII-only. Non-ASCII comments in a .bat get
REM  mangled under cp936 console codepage and corrupt the command lines.
REM
REM  After editing RTL sources: run this, then rebuild C3.exe to re-embed.
REM ============================================================
setlocal enabledelayedexpansion

set "ROOT=%~dp0.."
set "RTLCORE=%ROOT%\src\rtl\core"
set "OUT64=%ROOT%\src\rtl\lib"
set "OUT86=%ROOT%\src\rtl\lib\x86"
set "OBJDIR=%ROOT%\.temp\rtl_obj"

if not exist "%OUT64%" mkdir "%OUT64%"
if not exist "%OUT86%" mkdir "%OUT86%"
if not exist "%OBJDIR%" mkdir "%OBJDIR%"
if not exist "%OBJDIR%\x64" mkdir "%OBJDIR%\x64"
if not exist "%OBJDIR%\x86" mkdir "%OBJDIR%\x86"

set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"

set "COMMON_FLAGS=/c /O2 /W3 /utf-8 /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /D_CRT_NONSTDC_NO_WARNINGS /I"%RTLCORE%""
set "SRCS=%RTLCORE%\vb6rtl.c %RTLCORE%\vb6com.c %RTLCORE%\vb6comserver.c %RTLCORE%\vb6forms.c %RTLCORE%\vb6_di_stubs.c"

rem x64 - generated C code in this environment defaults to /MT (static CRT)
call "%VCVARS%" x64 >nul 2>&1
if errorlevel 1 (
    echo ERROR: Failed to initialize x64 MSVC environment.
    exit /b 1
)
cl %COMMON_FLAGS% /MT /Fo"%OBJDIR%\x64\\" %SRCS%
if errorlevel 1 (
    echo ERROR: x64 RTL compilation failed.
    exit /b 1
)
lib /OUT:"%OUT64%\vb6rtl.lib" "%OBJDIR%\x64\vb6rtl.obj" "%OBJDIR%\x64\vb6com.obj" "%OBJDIR%\x64\vb6_di_stubs.obj"
lib /OUT:"%OUT64%\vb6rtl_dll.lib" "%OBJDIR%\x64\vb6comserver.obj"
lib /OUT:"%OUT64%\vb6rtl_gui.lib" "%OBJDIR%\x64\vb6forms.obj"
if errorlevel 1 (
    echo ERROR: x64 library creation failed.
    exit /b 1
)

rem x86 - generated C code uses /MT explicitly
call "%VCVARS%" x86 >nul 2>&1
if errorlevel 1 (
    echo ERROR: Failed to initialize x86 MSVC environment.
    exit /b 1
)
cl %COMMON_FLAGS% /MT /Fo"%OBJDIR%\x86\\" %SRCS%
if errorlevel 1 (
    echo ERROR: x86 RTL compilation failed.
    exit /b 1
)
lib /OUT:"%OUT86%\vb6rtl.lib" "%OBJDIR%\x86\vb6rtl.obj" "%OBJDIR%\x86\vb6com.obj" "%OBJDIR%\x86\vb6_di_stubs.obj"
lib /OUT:"%OUT86%\vb6rtl_dll.lib" "%OBJDIR%\x86\vb6comserver.obj"
lib /OUT:"%OUT86%\vb6rtl_gui.lib" "%OBJDIR%\x86\vb6forms.obj"
if errorlevel 1 (
    echo ERROR: x86 library creation failed.
    exit /b 1
)

echo RTL libraries rebuilt successfully.
endlocal
