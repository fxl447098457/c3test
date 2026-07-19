@echo off
setlocal enabledelayedexpansion

REM ==================== 配置 ====================
set VBP_NAME=test_activex_dll.vbp
set DLL_NAME=test_activex_dll_x64.dll
set ARCH=x64
set REGSVR=C:\Windows\System32\regsvr32.exe

REM ==================== 自动提权 UAC ====================
net session >nul 2>&1
if !errorlevel! neq 0 (
    echo 正在请求管理员权限
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

REM ==================== 切换到脚本目录 ====================
cd /d "%~dp0"
set VBP=%~dp0%VBP_NAME%
set DLL=%~dp0%DLL_NAME%

REM ==================== 定位 c3 编译器 ====================
set C3=C:\pro\c3\C3.exe
if not exist "!C3!" (
    echo 错误 找不到 c3 编译器
    echo 已尝试 !C3!
    echo 请确认 c3 已安装在 C:\pro\c3 目录
    pause
    exit /b 1
)
REM ==================== 检查 vbp 存在 ====================
if not exist "%VBP%" (
    echo 错误 找不到 vbp 文件
    echo !VBP!
    pause
    exit /b 1
)

REM ==================== 清理旧产物 ====================
if exist "%DLL%" (
    echo 反注册旧 DLL
    "%REGSVR%" /u /s "%DLL%" >nul 2>&1
    del /f /q "%DLL%" >nul 2>&1
    if exist "%DLL%" (
        echo 警告 旧 DLL 被占用无法删除 请关闭使用该 DLL 的进程
        pause
        exit /b 1
    )
)

REM ==================== 编译 ====================
echo ========================================
echo 编译并注册 64 位 ActiveX DLL
echo ========================================
echo 编译器 !C3!
echo 目标架构 !ARCH!
echo 输出文件 !DLL!
echo.

"!C3!" --dll --arch !ARCH! -o "%DLL%" "%VBP%"
set RC=!errorlevel!

if !RC! neq 0 (
    echo.
    echo 编译失败 退出码 !RC!
    pause
    exit /b !RC!
)

if not exist "%DLL%" (
    echo.
    echo 编译失败 未产出 DLL
    pause
    exit /b 1
)

echo.
echo 编译成功
echo.

REM ==================== 注册 ====================
echo 注册到 64 位注册表视图 HKCR CLSID
"%REGSVR%" /s "%DLL%"
set RC=!errorlevel!

if !RC! neq 0 (
    echo.
    echo 静默注册失败 退出码 !RC!
    echo 显示 regsvr32 错误对话框
    "%REGSVR%" "%DLL%"
    pause
    exit /b !RC!
)

echo.
echo ========================================
echo 全部完成
echo ========================================
echo 编译 !ARCH! DLL 成功
echo 注册 64 位视图成功
echo 适用宿主 64 位 Office VBA 64 位进程
echo ========================================
echo.
pause
