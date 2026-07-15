@echo off
REM ============================================================
REM  run.bat - 运行编译后的 EXE
REM  用法: run <exe-name> [timeout-seconds]
REM  示例:
REM    run hello              = 运行 output\hello.exe (5秒超时)
REM    run form_test_p74 10   = 运行 output\form_test_p74.exe (10秒超时)
REM    run output\hello.exe   = 支持完整路径
REM ============================================================

if "%1"=="" (
    echo 用法: run ^<exe-name^> [timeout-seconds]
    echo.
    echo 示例:
    echo   run hello              = 运行 output\hello.exe (5秒超时)
    echo   run form_test_p74 10   = 运行 output\form_test_p74.exe (10秒)
    exit /b 1
)

cd /d C:\Users\vi\Desktop\c3.vb6.pro

REM 确定EXE路径
set EXEPATH=%1
if not exist "%EXEPATH%" (
    set EXEPATH=output\%1
)
if not exist "%EXEPATH%" (
    set EXEPATH=output\%1.exe
)
if not exist "%EXEPATH%" (
    echo [ERROR] 找不到: %1
    exit /b 1
)

REM 超时(秒)
set TIMEOUT=5
if not "%2"=="" set TIMEOUT=%2

echo [INFO] 运行: %EXEPATH% (超时=%TIMEOUT%s)
start "" /wait "%EXEPATH%"
echo [OK] 进程已退出
