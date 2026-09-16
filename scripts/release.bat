@echo off
REM ============================================================
REM  release.bat - 打包 publish\ 目录并发布到 GitCode Release
REM  仓库: https://gitcode.com/woeoio/c3.vb6.pro
REM
REM  用法 (双击或在任意目录调用均可):
REM    scripts\release.bat                  用 VERSION 版本号发布, tag 基点默认 origin/main
REM    scripts\release.bat -v 0.10.2        指定版本号 (同时生成 tag v0.10.2)
REM    scripts\release.bat HEAD             tag 基点改为 HEAD (也可以传任意分支/sha)
REM    scripts\release.bat --force          覆盖已存在的同名 tag / release
REM    scripts\release.bat -n               预演模式, 只打包不上传
REM
REM  流程: 读版本号 -> 打包 zip -> 建 tag 并推送 -> 创建 Release -> 上传附件 -> 校验
REM  令牌优先级: 环境变量 GITCODE_TOKEN > scripts\.gitcode_token > git 凭据管理器
REM ============================================================

setlocal EnableDelayedExpansion
title C3 - Release

REM ---- 脚本目录必须在 pushd 之前固化 (pushd 后 %~dp0 会被重新解析) ----
set "SCRIPTDIR=%~dp0"

REM ---- 项目根目录 = 脚本所在目录的上一级 ----
pushd "%SCRIPTDIR%.."
if errorlevel 1 (
    echo [ERROR] 无法进入项目根目录
    exit /b 1
)

set "ROOT=%CD%"
set "REPO=woeoio/c3.vb6.pro"
set "VER="
set "TARGET=origin/main"
set "FORCE=0"
set "DRYRUN=0"

REM ---- 参数解析 ----
:ARGS
if "%~1"=="" goto ARGSDONE
if /i "%~1"=="-h" goto USAGE
if /i "%~1"=="--help" goto USAGE
if /i "%~1"=="-v" (
    set "VER=%~2"
    shift
    shift
    goto ARGS
)
if /i "%~1"=="--version" (
    set "VER=%~2"
    shift
    shift
    goto ARGS
)
if /i "%~1"=="-f" (
    set "FORCE=1"
    shift
    goto ARGS
)
if /i "%~1"=="--force" (
    set "FORCE=1"
    shift
    goto ARGS
)
if /i "%~1"=="-n" (
    set "DRYRUN=1"
    shift
    goto ARGS
)
if /i "%~1"=="--dry-run" (
    set "DRYRUN=1"
    shift
    goto ARGS
)
set "TARGET=%~1"
shift
goto ARGS
:ARGSDONE

REM ---- 版本号: 未指定则读 VERSION 文件 ----
if not defined VER (
    if not exist "VERSION" (
        echo [ERROR] 未找到 VERSION 文件, 请用 -v 指定版本号
        popd
        exit /b 1
    )
    for /f "usebackq delims=" %%v in ("VERSION") do (
        if not defined VER set "VER=%%v"
    )
)
if not defined VER (
    echo [ERROR] VERSION 文件为空
    popd
    exit /b 1
)
REM VERSION 文件内容可能含前后空格, 剔除后再使用
set "VER=%VER: =%"

set "TAG=v%VER%"
set "DIST=dist"
set "ZIP=c3-v%VER%-win-x64.zip"
set "ZIPPATH=%ROOT%\%DIST%\%ZIP%"

echo.
echo ================ C3 Release ================
echo   版本   %VER%      tag %TAG%
echo   仓库   %REPO%
echo   基点   %TARGET%
echo   打包   %DIST%\%ZIP%
if "%DRYRUN%"=="1" echo   模式   DRY-RUN  只打包, 不打 tag / 不上传
echo.

REM ---- 前置检查 ----
if not exist "publish\C3.exe" (
    echo [ERROR] publish\C3.exe 不存在, 请先执行 scripts\build.bat 构建
    popd
    exit /b 1
)
if not exist "%SystemRoot%\System32\tar.exe" (
    echo [ERROR] 未找到 Windows 自带 tar.exe
    popd
    exit /b 1
)
if not exist "%SCRIPTDIR%release.ps1" (
    echo [ERROR] 未找到 scripts\release.ps1
    popd
    exit /b 1
)
where git >nul 2>nul
if errorlevel 1 (
    echo [ERROR] 未找到 git
    popd
    exit /b 1
)

REM ---- 校验 tag 基点 ----
git rev-parse --verify --quiet "%TARGET%^{commit}" >nul 2>nul
if errorlevel 1 (
    echo [ERROR] tag 基点不存在: %TARGET%
    popd
    exit /b 1
)

REM ---- tag 冲突处理 ----
git rev-parse --verify --quiet "refs/tags/%TAG%" >nul 2>nul
if not errorlevel 1 (
    if "%FORCE%"=="1" (
        echo [warn ] tag %TAG% 已存在, --force 生效, 删除旧 tag
        git push origin ":refs/tags/%TAG%" >nul 2>nul
        git tag -d "%TAG%" >nul 2>nul
    ) else (
        echo [ERROR] tag %TAG% 已存在. 需要重发请加 --force, 或先 -v 升版本号
        popd
        exit /b 1
    )
)

REM ---- 打包 ----
echo [1/4] 打包 publish 目录 ...
if not exist "%DIST%" mkdir "%DIST%"
if exist "%ZIPPATH%" del /f /q "%ZIPPATH%"
"%SystemRoot%\System32\tar.exe" -a -c -f "%ZIPPATH%" -C publish .
if errorlevel 1 (
    echo [ERROR] 打包失败
    popd
    exit /b 1
)
for %%A in ("%ZIPPATH%") do set "ZIPSIZE=%%~zA"
echo [1/4] 打包完成 -- %DIST%\%ZIP%  (bytes %ZIPSIZE%)
if "%DRYRUN%"=="1" (
    echo.
    echo 预演模式结束, 未打 tag 也未上传.
    popd
    exit /b 0
)

REM ---- 建 tag 并推送 ----
echo [2/4] 创建并推送 tag %TAG% ...
git tag -a "%TAG%" -m "C3 %VER%" "%TARGET%"
if errorlevel 1 (
    echo [ERROR] 创建 tag 失败
    popd
    exit /b 1
)
git push origin "%TAG%"
if errorlevel 1 (
    echo [ERROR] 推送 tag 失败
    popd
    exit /b 1
)
echo [2/4] tag 已推送

REM ---- 调 PowerShell 走 GitCode API 建 Release + 上传附件 ----
set "PSFORCE="
if "%FORCE%"=="1" set "PSFORCE=-Force"
echo [3/4] 创建 Release 并上传附件 ...
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPTDIR%release.ps1" -Repo "%REPO%" -Tag "%TAG%" -Version "%VER%" -Zip "%ZIPPATH%" -TemplateFile "%SCRIPTDIR%release-notes-template.md" -NotesFile "%SCRIPTDIR%release-notes.md" %PSFORCE%
if errorlevel 1 (
    echo [ERROR] Release 发布失败
    popd
    exit /b 1
)
echo [4/4] 完成
popd
exit /b 0

:USAGE
echo.
echo release.bat - 打包 publish\ 目录并发布到 GitCode Release
echo.
echo 用法:
echo   scripts\release.bat [选项] [tag 基点, 默认 origin/main]
echo.
echo 选项:
echo   -v 版本             指定版本号, 默认读 VERSION 文件
echo   -f, --force         覆盖已存在的同名 tag / release
echo   -n, --dry-run       只打包, 不打 tag 不上传
echo   -h, --help          显示本帮助
echo.
echo 令牌: 环境变量 GITCODE_TOKEN 优先, 其次 scripts\.gitcode_token,
echo       最后自动从 git 凭据管理器 (gitcode.com) 读取.
echo.
exit /b 0
