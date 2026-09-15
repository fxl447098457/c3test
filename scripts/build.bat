@echo off
REM ============================================================
REM  build.bat - 构建 C3.exe
REM  用法: build [clean]
REM    build       = 增量构建
REM    build clean = 清理后完整构建
REM  说明: 通过 vswhere 自动定位本机 Visual Studio, 不依赖固定安装路径,
REM        可在任意开发机 / CI 环境使用
REM ============================================================

REM ---- 定位项目根目录 (由脚本自身位置推导, 不依赖固定盘符) ----
cd /d "%~dp0.."
if errorlevel 1 (
    echo [ERROR] 无法进入项目根目录
    exit /b 1
)

REM ---- 通过 vswhere 定位 vcvarsall.bat (兼容 Community/Professional/Enterprise/BuildTools) ----
REM 捆绑 CMake/Ninja 只随完整 IDE 分发; 若本机只装 BuildTools 则在另一个实例里找
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VCVARSALL="
set "VS_CMAKE_BIN="
set "VS_NINJA_BIN="
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        if exist "%%i\VC\Auxiliary\Build\vcvarsall.bat" set "VCVARSALL=%%i\VC\Auxiliary\Build\vcvarsall.bat"
    )
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -all -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        if not defined VS_CMAKE_BIN if exist "%%i\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" set "VS_CMAKE_BIN=%%i\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
        if not defined VS_NINJA_BIN if exist "%%i\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe" set "VS_NINJA_BIN=%%i\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"
    )
)

if "%VCVARSALL%"=="" (
    echo [ERROR] 未找到 Visual Studio 2022 的 vcvarsall.bat
    echo         请安装 VS2022 并勾选 [使用 C++ 的桌面开发] 工作负载
    exit /b 1
)

call "%VCVARSALL%" x64 >nul 2>&1
if errorlevel 1 (
    echo [ERROR] vcvarsall.bat 加载失败
    exit /b 1
)

REM ---- cmake/ninja 不在 PATH 时, 补充 VS 捆绑的 CMake/Ninja (C++ 工作负载自带) ----
where cmake >nul 2>&1
if errorlevel 1 if defined VS_CMAKE_BIN set "PATH=%PATH%;%VS_CMAKE_BIN%"
where ninja >nul 2>&1
if errorlevel 1 if defined VS_NINJA_BIN set "PATH=%PATH%;%VS_NINJA_BIN%"

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
set "PUBLISH_DIR=%~dp0..\publish"

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

echo [OK] 已复制产物到 publish\C3.exe

REM 后续冒烟验证: 跑 scripts\test.bat smoke (用例 tests\smoke.bas, 全链路快速自检)
