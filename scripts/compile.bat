@echo off
REM ============================================================
REM  compile.bat - 用 C3.exe 编译 VB6 源文件
REM  用法: compile <source.bas/.frm/.vbp> [output-dir]
REM  示例:
REM    compile tests\hello.bas
REM    compile tests\test_form\form_test_p74.frm
REM    compile tests\test_class.vbp output
REM ============================================================

if "%1"=="" (
    echo 用法: compile ^<source.bas/.frm/.vbp^> [output-dir]
    echo.
    echo 示例:
    echo   compile tests\hello.bas
    echo   compile tests\test_form\empty_form.frm
    echo   compile tests\test_class.vbp output
    exit /b 1
)

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
if errorlevel 1 (
    echo [ERROR] vcvarsall.bat 加载失败
    exit /b 1
)

cd /d D:\vb6pro

REM 设置VB6 RTL目录
set VB6RTL_DIR=D:\vb6pro\src\rtl\core

REM 输出目录默认 output/
set OUTDIR=output
if not "%2"=="" set OUTDIR=%2

echo [INFO] 编译: %1
echo [INFO] 输出: %OUTDIR%

.build\C3.exe %1 --output-dir %OUTDIR%
if errorlevel 1 (
    echo [ERROR] 编译失败
    exit /b 1
)

echo [OK] 编译成功
