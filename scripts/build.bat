@echo off
REM ============================================================
REM  build.bat - 构建 C3.exe
REM  用法: build [clean]
REM    build       = 增量构建
REM    build clean = 清理后完整构建
REM ============================================================

REM vcvarsall 路径: 环境变量 C3_VCVARSALL 优先, 未设置用默认 (VS2022 Community). 详见 scripts\README.md
if not defined C3_VCVARSALL set "C3_VCVARSALL=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
call "%C3_VCVARSALL%" x64 >nul 2>&1
if errorlevel 1 (
    echo [ERROR] vcvarsall.bat 加载失败
    exit /b 1
)

REM 项目根目录: 环境变量 C3_PROJECT_DIR 优先, 未设置取脚本所在目录的上一级. 详见 scripts\README.md
if not defined C3_PROJECT_DIR set "C3_PROJECT_DIR=%~dp0.."
cd /d "%C3_PROJECT_DIR%"

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

echo [OK] C3.exe 构建成功: .build\C3.exe
