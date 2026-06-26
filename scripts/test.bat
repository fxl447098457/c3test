@echo off
REM ============================================================
REM  test.bat - 运行回归测试
REM  用法: test [all|run|compile|syntax] [verbose]
REM    test          = 全部48个测试
REM    test run      = 仅运行测试(编译+运行+输出校验)
REM    test compile  = 仅编译测试
REM    test syntax   = 仅语法测试
REM    test verbose  = 全部测试(详细模式)
REM ============================================================

cd /d D:\vb6pro

REM 设置VB6 RTL目录
set VB6RTL_DIR=D:\vb6pro\src\rtl\core

powershell -ExecutionPolicy Bypass -File tests\run_tests.ps1 -Category %1 -Verbose:%2
