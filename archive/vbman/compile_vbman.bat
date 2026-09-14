@echo off
REM ============================================================
REM  compile_vbman.bat - 用 C3 编译 vbman 项目
REM  用法: compile_vbman.bat [syntax|emit-c|compile]
REM ============================================================

set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat
set C3=C:\Users\vi\Desktop\c3.vb6.pro\.build\C3.exe
set SRC=C:\Users\vi\Desktop\c3.vb6.pro\vbman\src
set DIST=C:\Users\vi\Desktop\c3.vb6.pro\vbman\dist

call "%VCVARS%" x64 >nul 2>&1
cd /d C:\Users\vi\Desktop\c3.vb6.pro\vbman

set MODE=%1
if "%MODE%"=="" set MODE=compile

if "%MODE%"=="syntax" (
    echo === Syntax check on VBMAN.vbp ===
    "%C3%" "%SRC%\VBMAN.vbp" --syntax-only -v 2>&1
    echo EXIT: %ERRORLEVEL%
) else if "%MODE%"=="emit-c" (
    echo === Emit C from VBMAN.vbp ===
    "%C3%" "%SRC%\VBMAN.vbp" --emit-c --output-dir "%DIST%" -v 2>&1
    echo EXIT: %ERRORLEVEL%
) else (
    echo === Compile VBMAN.vbp as DLL ===
    "%C3%" "%SRC%\VBMAN.vbp" --dll --output-dir "%DIST%" -v 2>&1
    echo EXIT: %ERRORLEVEL%
)
