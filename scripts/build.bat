@echo off
REM ============================================================
REM  build.bat - 构建 C3.exe
REM  用法: build [clean]
REM    build       = 增量构建
REM    build clean = 清理后完整构建
REM ============================================================

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
if errorlevel 1 (
    echo [ERROR] vcvarsall.bat 加载失败
    exit /b 1
)

cd /d D:\code\vi\c3.vb6.pro

if "%1"=="clean" (
    echo [INFO] 清理 .build 目录...
    rmdir /s /q .build 2>nul
    echo [INFO] 重新 configure...
    cmake -B .build -G Ninja -DCMAKE_BUILD_TYPE=Release
    if errorlevel 1 (
        echo [ERROR] CMake configure 失败
        exit /b 1
    )
)

echo [INFO] 构建 C3.exe...
cmake --build .build --config Release
if errorlevel 1 (
    echo [ERROR] 构建失败
    exit /b 1
)

echo [OK] C3.exe 构建成功，输出 .build\C3.exe

REM ---- 复制编译产物到发布目录 (publish) ----
set "PUBLISH_DIR=D:\code\vi\c3.vb6.pro\publish"

if not exist "%PUBLISH_DIR%" (
    echo [INFO] 创建发布目录 %PUBLISH_DIR%
    mkdir "%PUBLISH_DIR%"
)

echo [INFO] 复制 C3.exe 到发布目录...
copy /y ".build\C3.exe" "%PUBLISH_DIR%\C3.exe" >nul
if errorlevel 1 (
    echo [ERROR] 复制 C3.exe 失败
    exit /b 1
)

if exist ".build\C3.pdb" (
    copy /y ".build\C3.pdb" "%PUBLISH_DIR%\C3.pdb" >nul
)

echo [OK] 已复制产物到 %PUBLISH_DIR%\C3.exe

REM 后续冒烟验证: 跑 scripts\test.bat smoke (用例 tests\smoke.bas, 全链路快速自检)
