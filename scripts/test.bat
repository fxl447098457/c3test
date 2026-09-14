@echo off
REM ============================================================
REM  test.bat - 运行回归测试
REM  用法: test [all|run|compile|syntax] [verbose]
REM    test          = 全部测试
REM    test run      = 仅运行测试(编译+运行+输出校验)
REM    test compile  = 仅编译测试
REM    test syntax   = 仅语法测试
REM ============================================================

REM 路径由脚本自身位置推导, 不依赖固定盘符
cd /d "%~dp0.."
if errorlevel 1 (
    echo [ERROR] 无法进入项目根目录
    exit /b 1
)

REM 设置VB6 RTL目录
set "VB6RTL_DIR=%CD%\src\rtl\core"

set "CAT=%1"
if "%CAT%"=="" set "CAT=all"

if "%2"=="" (
    powershell -ExecutionPolicy Bypass -File "%~dp0..\tests\run_tests.ps1" -Category %CAT%
) else (
    powershell -ExecutionPolicy Bypass -File "%~dp0..\tests\run_tests.ps1" -Category %CAT% -Verbose:%2
)
