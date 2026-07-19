@echo off
chcp 65001 >nul 2>&1
REM ============================================================================
REM  C3 编译器配套 - MSVC Build Tools 一键安装 + 部署脚本 (install_msvc.bat)
REM ============================================================================
REM  功能
REM    1. 检测本地迷你 MSVC 工具链 (msvc\vcvars.bat) 或本机已安装的 MSVC
REM    2. 若本地迷你工具链已就绪 -> 直接使用,跳过 3-4 GB 在线安装
REM    3. 若本地无工具链且未安装 -> 下载并静默安装 VS 2022 Build Tools
REM       (仅 VCTools 工作负载 + Windows SDK, 不含 VS 集成开发环境)
REM    4. 将 C3 编译器及配套文件部署到 C:\pro\c3
REM    5. 安装右键菜单 "使用 C3 编译" (导入注册表)
REM    6. 配置环境变量 (PATH + MSVC,离线模式零配置可直接 c3)
REM    7. 输出 C3 编译器环境配置说明
REM
REM  用法
REM    直接双击运行即可,脚本会自动提升管理员权限
REM    (也可右键 -> "以管理员身份运行" 跳过提权步骤)
REM
REM  安装目录 C:\BuildTools (MSVC) + C:\pro\c3 (C3 编译器)
REM  预计耗时 10-30 分钟 (取决于网络速度,已安装 MSVC 或有本地工具链则仅需数秒)
REM ============================================================================

setlocal EnableDelayedExpansion
title C3 - MSVC Build Tools 一键安装 + 部署脚本

REM ============================================================================
REM  Step 0 自动提升管理员权限 (UAC)
REM ============================================================================
>nul 2>&1 net session
if %errorlevel% neq 0 (
    echo 正在请求管理员权限...
    powershell -Command "Start-Process cmd -ArgumentList '/c \"%~f0\"' -Verb RunAs"
    exit /b
)

echo.
echo ==================================================
echo   C3 编译器 - MSVC Build Tools 一键安装 + 部署脚本
echo ==================================================
echo.

REM ============================================================================
REM  Step 1 检测 MSVC 工具链
REM ============================================================================
echo [步骤 1/6] 检测 MSVC 工具链...

set "MSVC_MODE="
set "VCVARSALL="

REM --- 优先检测本地迷你 MSVC 工具链 (离线模式) ---
set "LOCAL_VCVARS=%~dp0msvc\vcvars.bat"
if exist "!LOCAL_VCVARS!" (
    echo.
    echo [√] 检测到本地迷你 MSVC 工具链 ^(离线模式^)
    echo     !LOCAL_VCVARS!
    echo.
    echo     无需在线安装,直接使用内置工具链 ^(~65 MB^)。 
    set "VCVARSALL=!LOCAL_VCVARS!"
    set "MSVC_MODE=local"
    echo     跳过安装步骤,直接进入部署阶段。 
    echo.
    goto :deploy
)

REM --- 检测本机已安装的 MSVC 工具链 (VCTools.x86.x64 组件) ---
REM 用直接路径,避免 %%ProgramFiles(x86)%% 的括号被 CMD 误解析
set "VSWHERE_X86=C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSWHERE_X64=C:\Program Files\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSWHERE="

if exist "!VSWHERE_X86!" set "VSWHERE=!VSWHERE_X86!"
if not defined VSWHERE if exist "!VSWHERE_X64!" set "VSWHERE=!VSWHERE_X64!"

if defined VSWHERE (
    for /f "usebackq tokens=*" %%i in (`""!VSWHERE!" -latest -all -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul"`) do (
        if exist "%%i\VC\Auxiliary\Build\vcvarsall.bat" (
            set "VCVARSALL=%%i\VC\Auxiliary\Build\vcvarsall.bat"
        )
    )
)

REM Fallback vswhere 不可靠时,直接检查常见安装路径
if not defined VCVARSALL (
    for %%e in (Community Professional Enterprise BuildTools) do (
        if exist "C:\Program Files\Microsoft Visual Studio\2022\%%e\VC\Auxiliary\Build\vcvarsall.bat" (
            set "VCVARSALL=C:\Program Files\Microsoft Visual Studio\2022\%%e\VC\Auxiliary\Build\vcvarsall.bat"
        )
    )
)
if not defined VCVARSALL (
    if exist "C:\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" set "VCVARSALL=C:\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
)

if defined VCVARSALL (
    echo.
    echo [√] 已检测到可用的 MSVC 工具链
    echo     !VCVARSALL!
    set "MSVC_MODE=system"
    echo.
    echo 跳过安装步骤,直接进入部署阶段。 
    echo.
    goto :deploy
)

echo     [i] 未检测到 MSVC 工具链,开始下载安装。 

REM ============================================================================
REM  Step 2 下载 VS Build Tools 安装引导程序
REM ============================================================================
echo.
echo [步骤 2/6] 下载 VS Build Tools 安装引导程序...

set "INSTALLER=%TEMP%\c3_vs_BuildTools.exe"
set "URL=https://aka.ms/vs/17/release/vs_BuildTools.exe"

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "$ProgressPreference = 'SilentlyContinue';" ^
    "try {" ^
    "    Invoke-WebRequest -Uri '%URL%' -OutFile '%INSTALLER%' -UseBasicParsing -TimeoutSec 120;" ^
    "    if (Test-Path '%INSTALLER%') {" ^
    "        $sz = (Get-Item '%INSTALLER%').Length;" ^
    "        Write-Host ('    [√] 下载完成: %INSTALLER% (' + [math]::Round($sz/1KB) + ' KB)');" ^
    "    } else {" ^
    "        Write-Host '    [x] 下载文件未生成,请检查网络后重试';" ^
    "        exit 1;" ^
    "    }" ^
    "} catch {" ^
    "    Write-Host ('    [x] 下载失败: ' + $_.Exception.Message);" ^
    "    exit 1;" ^
    "}"

if errorlevel 1 (
    echo.
    echo [错误] 安装引导程序下载失败。 
    echo        请检查网络连接后重试,或手动访问以下地址下载
    echo        https^://visualstudio.microsoft.com/zh-hans/visual-cpp-build-tools/
    echo.
    pause
    exit /b 1
)

if not exist "%INSTALLER%" (
    echo.
    echo [错误] 安装引导程序下载失败,文件不存在。 
    pause
    exit /b 1
)

REM ============================================================================
REM  Step 3 静默安装 (VCTools 工作负载 + x64/x86 编译工具 + Windows 11 SDK)
REM ============================================================================
echo.
echo [步骤 3/6] 静默安装 VS Build Tools...
echo     此过程通常需要 10-30 分钟,取决于网络速度。 
echo     安装过程中无需任何操作,请耐心等待,窗口请勿关闭。 
echo.

"%INSTALLER%" ^
    --quiet --norestart --wait --nocache ^
    --installPath "C:\BuildTools" ^
    --add Microsoft.VisualStudio.Workload.VCTools ^
    --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 ^
    --add Microsoft.VisualStudio.Component.Windows11SDK.22621

set "INSTALL_EXIT=%errorLevel%"

REM 安装完毕后删除引导程序
del "%INSTALLER%" >nul 2>&1

if %INSTALL_EXIT% neq 0 (
    echo.
    echo [错误] 安装过程返回错误码 %INSTALL_EXIT%
    echo        可能原因 网络中断 / 磁盘空间不足 / 权限不足 / 已有 VS 安装冲突。 
    echo        建议先关闭其他 VS 安装程序,重启系统后再次运行此脚本。 
    echo        ^(已下载的临时文件将自动清理^)
    echo.
    pause
    exit /b 1
)

REM ============================================================================
REM  Step 4 验证安装结果
REM ============================================================================
echo.
echo [步骤 4/6] 验证安装结果...

set "VSWHERE="
if exist "!VSWHERE_X86!" set "VSWHERE=!VSWHERE_X86!"
if not defined VSWHERE if exist "!VSWHERE_X64!" set "VSWHERE=!VSWHERE_X64!"

set "VCVARSALL="
if defined VSWHERE (
    for /f "usebackq tokens=*" %%i in (`""!VSWHERE!" -latest -all -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul"`) do (
        if exist "%%i\VC\Auxiliary\Build\vcvarsall.bat" (
            set "VCVARSALL=%%i\VC\Auxiliary\Build\vcvarsall.bat"
        )
    )
)

REM Fallback vswhere 不可靠时,直接检查常见安装路径
if not defined VCVARSALL (
    for %%e in (Community Professional Enterprise BuildTools) do (
        if exist "C:\Program Files\Microsoft Visual Studio\2022\%%e\VC\Auxiliary\Build\vcvarsall.bat" (
            set "VCVARSALL=C:\Program Files\Microsoft Visual Studio\2022\%%e\VC\Auxiliary\Build\vcvarsall.bat"
        )
    )
)
if not defined VCVARSALL (
    if exist "C:\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" set "VCVARSALL=C:\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
)

if not defined VCVARSALL (
    echo.
    echo [x] 安装完成但未检测到 MSVC 工具链。 
    echo     请尝试重启系统后再运行此脚本验证,或手动检查 C:\BuildTools 目录。 
    echo.
    pause
    exit /b 1
)

echo   [√] 安装成功! MSVC 工具链已就绪
echo       !VCVARSALL!
set "MSVC_MODE=system"
echo.

REM ============================================================================
REM  Step 5 部署 C3 编译器到 C:\pro\c3 + 安装右键菜单
REM ============================================================================
:deploy
echo [步骤 5/6] 部署 C3 编译器到 C:\pro\c3...

REM 获取脚本所在目录 (去掉末尾反斜杠)
set "SRC_DIR=%~dp0"
if "!SRC_DIR:~-1!"=="\" set "SRC_DIR=!SRC_DIR:~0,-1!"

REM 如果已在目标目录则跳过复制
if /i "!SRC_DIR!"=="C:\pro\c3" (
    echo     [i] 已在目标目录 C:\pro\c3,跳过复制
    goto :import_reg
)

REM 创建目标目录
if not exist "C:\pro\c3" mkdir "C:\pro\c3"

REM 复制当前目录所有文件到 C:\pro\c3 (含 msvc\ 子目录)
echo     正在复制文件 -- !SRC_DIR! -- C:\pro\c3 ^(含 msvc\^)
xcopy "!SRC_DIR!\*" "C:\pro\c3\" /E /Y /I /Q >nul 2>&1
if %errorLevel% neq 0 (
    echo     [x] 文件复制失败
    echo         请确认源目录可读且 C 盘可写。 
    pause
    exit /b 1
)

REM 复制 VC 运行时 DLL 到 C3.exe 旁边 (避免 MSVCP140.dll 找不到)
if exist "C:\pro\c3\msvc\bin\msvcp140.dll" (
    copy /Y "C:\pro\c3\msvc\bin\msvcp*.dll" "C:\pro\c3\" >nul 2>&1
    copy /Y "C:\pro\c3\msvc\bin\vcruntime140*.dll" "C:\pro\c3\" >nul 2>&1
    echo     [√] VC 运行时 DLL 已部署 ^(MSVCP140 / VCRUNTIME140^)
)

REM 验证关键文件
if not exist "C:\pro\c3\C3.exe" (
    echo     [x] C3.exe 未复制到 C:\pro\c3,部署失败
    echo         请确保 install_msvc.bat 与 C3.exe 在同一目录。 
    pause
    exit /b 1
)
echo     [√] 文件部署完成 C:\pro\c3

:import_reg
REM 导入右键菜单注册表
echo.
echo     正在安装右键菜单...
if exist "C:\pro\c3\menu\c3_right_click.reg" (
    reg import "C:\pro\c3\menu\c3_right_click.reg" >nul 2>&1
    if !errorLevel! equ 0 (
        echo     [√] 右键菜单已安装 - "使用 C3 编译"
    ) else (
        echo     [!] 注册表导入失败,可手动双击以下文件安装
        echo         C:\pro\c3\menu\c3_right_click.reg
    )
) else (
    echo     [!] 未找到 c3_right_click.reg,跳过右键菜单安装
)

echo.
echo   [√] C3 编译器已部署到 C:\pro\c3
echo       右键任意文件即可看到 "使用 C3 编译" 菜单项

REM ============================================================================
REM  Step 6 配置环境变量 (PATH + MSVC 工具链)
REM ============================================================================
echo [步骤 6/6] 配置环境变量...

REM --- 读取当前用户 PATH ---
set "USER_PATH="
for /f "tokens=2,*" %%a in ('reg query "HKCU\Environment" /v Path 2^>nul') do set "USER_PATH=%%b"
set "NEW_PATH=!USER_PATH!"
set "PATH_CHANGED=0"

REM --- 通用: 将 C:\pro\c3 加入 PATH (使 c3 命令行可用) ---
if defined NEW_PATH (
    echo !NEW_PATH! | findstr /i /c:"C:\\pro\\c3" >nul 2>&1
    if !errorLevel! neq 0 (
        set "NEW_PATH=!NEW_PATH!;C:\pro\c3"
        set "PATH_CHANGED=1"
    )
) else (
    set "NEW_PATH=C:\pro\c3"
    set "PATH_CHANGED=1"
)

REM --- 离线模式: 永久写入 MSVC 环境变量,使 c3 命令行和右键菜单零配置 ---
REM     C3 通过 VCINSTALLDIR 检测 MSVC 已就绪,直接调用 cl.exe
REM     故需将 INCLUDE/LIB/PATH(msvc\bin) 永久写入用户环境
if "!MSVC_MODE!"=="local" (
    set "MSVC_DIR=C:\pro\c3\msvc"
    echo     正在写入 MSVC 环境变量 ^(VCINSTALLDIR / INCLUDE / LIB^)...
    reg add "HKCU\Environment" /v VCINSTALLDIR /t REG_SZ /d "!MSVC_DIR!" /f >nul 2>&1
    reg add "HKCU\Environment" /v INCLUDE /t REG_SZ /d "!MSVC_DIR!\include;!MSVC_DIR!\include\ucrt;!MSVC_DIR!\include\um;!MSVC_DIR!\include\shared" /f >nul 2>&1
    reg add "HKCU\Environment" /v LIB /t REG_SZ /d "!MSVC_DIR!\lib\x64" /f >nul 2>&1
    echo     [√] VCINSTALLDIR / INCLUDE / LIB 已写入用户环境变量

    REM 将 msvc\bin 加入 PATH (cl.exe 所在目录,C3 直接调用 cl.exe 需此路径)
    echo !NEW_PATH! | findstr /i /c:"C:\\pro\\c3\\msvc\\bin" >nul 2>&1
    if !errorLevel! neq 0 (
        set "NEW_PATH=C:\pro\c3\msvc\bin;!NEW_PATH!"
        set "PATH_CHANGED=1"
    )
)

REM --- 写回 PATH ---
if "!PATH_CHANGED!"=="1" (
    reg add "HKCU\Environment" /v Path /t REG_EXPAND_SZ /d "!NEW_PATH!" /f >nul 2>&1
    if !errorLevel! equ 0 (
        echo     [√] PATH 已更新 ^(新增 C3 相关条目^)
        echo     [i] 新开的命令行窗口将自动生效,无需重启
    ) else (
        echo     [!] PATH 写入失败,请手动将 C:\pro\c3 加入用户环境变量
    )
) else (
    echo     [i] PATH 已包含所需条目,跳过
)

REM 广播 WM_SETTINGCHANGE 消息,通知其他应用环境变量已更新
setx _C3_BROADCAST 1 >nul 2>&1
reg delete "HKCU\Environment" /v _C3_BROADCAST /f >nul 2>&1

REM ============================================================================
REM  输出使用说明
REM ============================================================================
:print_usage
echo.
echo ==================================================
echo   安装完成! 现在可以开始使用 C3 编译器了。 
echo ==================================================
echo.
echo 用法 A - 右键菜单 ^(最简单^)
echo   右键点击任意 VB6 文件 -- "使用 C3 编译"
echo   在弹出窗口中输入编译参数 ^(可留空^),回车开始编译。 
echo.
echo 用法 B - 直接在命令行使用 ^(已加入 PATH^)
echo   打开任意 CMD 或 PowerShell,直接输入
echo     c3 hello.bas           ^(编译单个文件^)
echo     c3 MyProject.vbp      ^(编译 VBP 工程^)
echo   ^(新开的窗口生效,已打开的窗口需重启^)
echo.
if "!MSVC_MODE!"=="local" (
    echo   注 离线模式环境变量已自动配置,c3 可直接使用无需额外设置 
    echo.
    echo 用法 C - 直接调用 cl.exe ^(可选^)
    echo   如需在命令行直接使用 cl.exe/link.exe,可临时加载环境
    echo   执行 call "C:\pro\c3\msvc\vcvars.bat"
    echo.
    echo   内置迷你工具链无需安装 VS Build Tools ^(约 65 MB^)
    echo       包含 cl.exe, link.exe 及必要的 Windows SDK 头文件和库
) else (
    echo 用法 C - 使用 VS 开发命令提示符
    echo   1. 开始菜单搜索 "x64 Native Tools Command Prompt for VS 2022"
    echo   2. 在该命令行中切换到你的 VB6 工程目录
    echo   3. 执行 c3 hello.bas      ^(编译单个文件^)
    echo           c3 MyProject.vbp   ^(编译 VBP 工程^)
)
echo.
echo 用法 D - 在普通 CMD 中临时配置 MSVC 环境
echo   1. 打开 CMD 或 PowerShell
if "!MSVC_MODE!"=="local" (
    echo   2. 执行 call "C:\pro\c3\msvc\vcvars.bat" ^(仅需直接用 cl.exe 时^)
) else (
    echo   2. 执行 call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
)
echo   3. 即可运行 c3 你的VB6工程.vbp
echo.
echo 卸载右键菜单
echo   双击运行 C:\pro\c3\menu\c3_right_click_remove.reg 即可移除。 
echo 卸载环境变量
echo   系统设置 -- 环境变量 -- 用户变量,删除以下条目 
echo     Path 中的 C:\pro\c3 及 C:\pro\c3\msvc\bin 
if "!MSVC_MODE!"=="local" (
    echo     VCINSTALLDIR / INCLUDE / LIB ^(离线模式自动添加^) 
)
echo 卸载 C3 编译器
echo   删除 C:\pro\c3 目录即可完全卸载。 
echo.
echo ==================================================
echo.
pause
exit /b 0