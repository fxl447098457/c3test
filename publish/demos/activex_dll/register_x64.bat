@echo off
setlocal enabledelayedexpansion

REM ==================== 配置 ====================
set DLL_NAME=test_activex_dll.dll

REM ==================== 自动提权 UAC ====================
net session >nul 2>&1
if !errorlevel! neq 0 (
    echo 正在请求管理员权限
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

REM ==================== 切换到脚本目录 ====================
cd /d "%~dp0"
set DLL=%~dp0%DLL_NAME%

REM ==================== 检查 DLL 存在 ====================
if not exist "%DLL%" (
    echo 错误 找不到 DLL 文件
    echo !DLL!
    pause
    exit /b 1
)

REM ==================== 注册 ====================
echo ========================================
echo 注册 64 位 ActiveX DLL
echo ========================================
echo DLL 路径 !DLL!
echo 使用工具 64 位 regsvr32 位于 System32
echo 适用宿主 64 位 Office VBA 64 位进程
echo.

C:\Windows\System32\regsvr32.exe /s "%DLL%"
set RC=!errorlevel!

if !RC! neq 0 (
    echo.
    echo 静默注册失败 退出码 !RC!
    echo 显示 regsvr32 错误对话框
    C:\Windows\System32\regsvr32.exe "%DLL%"
    pause
    exit /b !RC!
)

echo.
echo 注册成功
echo CLSID 已写入 64 位注册表视图 HKCR CLSID
echo.
pause
