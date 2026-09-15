@echo off
REM ============================================================
REM  build.bat - 构建 C3.exe
REM  用法: build [clean]
REM    build       = 增量构建
REM    build clean = 清理后完整构建
REM  说明: vcvarsall 定位 -- 环境变量 C3_VCVARSALL 优先, 未设置时 vswhere 自动探测,
REM        可在任意开发机 / CI 环境零配置使用. 详见 scripts\README.md
REM ============================================================

REM ---- 项目根目录: 环境变量 C3_PROJECT_DIR 优先, 未设置取脚本所在目录的上一级 ----
if not defined C3_PROJECT_DIR set "C3_PROJECT_DIR=%~dp0.."
cd /d "%C3_PROJECT_DIR%"
if errorlevel 1 (
    echo [ERROR] 无法进入项目根目录
    exit /b 1
)

REM ---- vcvarsall 定位: 环境变量 C3_VCVARSALL 优先, 未设置时 vswhere 自动探测 ----
REM (兼容 Community/Professional/Enterprise/BuildTools 多实例, 详见 scripts\README.md)
REM 捆绑 CMake/Ninja 只随完整 IDE 分发; 若本机只装 BuildTools 则在另一个实例里找
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VS_CMAKE_BIN="
set "VS_NINJA_BIN="
if exist "%VSWHERE%" (
    if not defined C3_VCVARSALL (
        for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
            if exist "%%i\VC\Auxiliary\Build\vcvarsall.bat" set "C3_VCVARSALL=%%i\VC\Auxiliary\Build\vcvarsall.bat"
        )
    )
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -all -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        if not defined VS_CMAKE_BIN if exist "%%i\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" set "VS_CMAKE_BIN=%%i\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
        if not defined VS_NINJA_BIN if exist "%%i\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe" set "VS_NINJA_BIN=%%i\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"
    )
)

if not defined C3_VCVARSALL (
    echo [ERROR] 未找到 vcvarsall.bat
    echo         请安装 VS2022 并勾选 [使用 C++ 的桌面开发] 工作负载,
    echo         或设置环境变量 C3_VCVARSALL 指向其完整路径. 详见 scripts\README.md
    exit /b 1
)

call "%C3_VCVARSALL%" x64 >nul 2>&1
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
