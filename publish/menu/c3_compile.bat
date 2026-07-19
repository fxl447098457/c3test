@echo off
chcp 65001 >nul 2>&1
setlocal enabledelayedexpansion

:: ============================================
::  C3 编译器 - 右键菜单调用脚本
::  自动检测 c3.exe,接收文件路径并编译
:: ============================================

:: --- 检测 c3.exe 路径 (二级降级) ---
set "C3_PATH="
if exist "C:\pro\c3\C3.exe" set "C3_PATH=C:\pro\c3\C3.exe"
if not defined C3_PATH if exist "%~dp0..\C3.exe" set "C3_PATH=%~dp0..\C3.exe"

if not defined C3_PATH (
    echo [错误] 找不到 C3.exe
    echo        已检查 C:\pro\c3\C3.exe
    echo                %~dp0..\C3.exe
    echo        请先运行 install_msvc.bat 完成安装。 
    goto :end
)

:: --- 检查参数 ---
set "INPUT_FILE=%~1"
if "%INPUT_FILE%"=="" (
    echo [错误] 未接收到文件路径
    echo        用法 c3_compile.bat "文件完整路径"
    echo        或将 VB6 文件拖拽到此脚本上。 
    goto :end
)

:: --- 检查文件存在 ---
if not exist "%INPUT_FILE%" (
    echo [错误] 文件不存在 %INPUT_FILE%
    goto :end
)

:: --- 显示编译信息 ---
echo.
echo ============================================
echo   C3 VB6 编译器
echo ============================================
echo.
echo   源文件: %INPUT_FILE%
echo   编译器: %C3_PATH%
echo.

:: --- 提示编译参数 ---
set "EXTRA_PARAMS="
set /p "EXTRA_PARAMS=请输入编译参数 (回车跳过): "

echo.
echo --------------------------------------------
echo   正在编译...
echo --------------------------------------------
echo.

:: --- 调用编译 ---
:: 离线模式环境变量已由 install_msvc.bat 永久写入用户环境
:: (VCINSTALLDIR / INCLUDE / LIB / PATH),C3 可直接调用 cl.exe
"%C3_PATH%" %EXTRA_PARAMS% "%INPUT_FILE%"

echo.
if %ERRORLEVEL% EQU 0 (
    echo   [√] 编译成功
) else (
    echo   [×] 编译失败,返回码 %ERRORLEVEL%
)

:end
echo.
pause