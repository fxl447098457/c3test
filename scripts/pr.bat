@echo off
REM ============================================================
REM  pr.bat - 为当前分支创建 GitCode Pull Request (合并请求)
REM  仓库: https://gitcode.com/woeoio/c3.vb6.pro
REM
REM  重要: 脚本只负责"创建"PR, 绝不自动合并 / 自动审批 / 推送受保护分支,
REM        必须由人在 GitCode 网页上审核后手动点合并.
REM
REM  用法 (双击或在任意目录调用均可):
REM    scripts\pr.bat                     当前分支 -> main
REM    scripts\pr.bat -b dev              base 改为 dev
REM    scripts\pr.bat -s vi/dev           head 改为 vi/dev (默认当前分支)
REM    scripts\pr.bat -t "标题"           自定义标题 (默认取提交信息)
REM    scripts\pr.bat -p                  创建前先 push 当前分支到 origin
REM    scripts\pr.bat -o                  创建后在浏览器打开 PR 页面
REM    scripts\pr.bat -n                  预演模式, 只打印内容不调 API
REM
REM  流程: 取当前分支 -> 检查与 origin 同步 -> 汇总提交 -> 创建 PR -> 输出链接
REM  令牌优先级: 环境变量 GITCODE_TOKEN > scripts\.gitcode_token > git 凭据管理器
REM ============================================================

setlocal EnableDelayedExpansion
title C3 - Pull Request

REM ---- 脚本目录必须在 pushd 之前固化 (pushd 后 %~dp0 会被重新解析) ----
set "SCRIPTDIR=%~dp0"

REM ---- 项目根目录 = 脚本所在目录的上一级 ----
pushd "%SCRIPTDIR%.."
if errorlevel 1 (
    echo [ERROR] 无法进入项目根目录
    exit /b 1
)

set "REPO=woeoio/c3.vb6.pro"
set "BASE=main"
set "HEAD="
set "TITLE="
set "PSARGS="

REM ---- 参数解析 ----
:ARGS
if "%~1"=="" goto ARGSDONE
if /i "%~1"=="-h" goto USAGE
if /i "%~1"=="--help" goto USAGE
if /i "%~1"=="-b" (
    set "BASE=%~2"
    shift
    shift
    goto ARGS
)
if /i "%~1"=="--base" (
    set "BASE=%~2"
    shift
    shift
    goto ARGS
)
if /i "%~1"=="-s" (
    set "HEAD=%~2"
    shift
    shift
    goto ARGS
)
if /i "%~1"=="--head" (
    set "HEAD=%~2"
    shift
    shift
    goto ARGS
)
if /i "%~1"=="-t" (
    set "TITLE=%~2"
    shift
    shift
    goto ARGS
)
if /i "%~1"=="--title" (
    set "TITLE=%~2"
    shift
    shift
    goto ARGS
)
if /i "%~1"=="-p" (
    set "PSARGS=%PSARGS% -Push"
    shift
    goto ARGS
)
if /i "%~1"=="--push" (
    set "PSARGS=%PSARGS% -Push"
    shift
    goto ARGS
)
if /i "%~1"=="-o" (
    set "PSARGS=%PSARGS% -Open"
    shift
    goto ARGS
)
if /i "%~1"=="--open" (
    set "PSARGS=%PSARGS% -Open"
    shift
    goto ARGS
)
if /i "%~1"=="-n" (
    set "PSARGS=%PSARGS% -DryRun"
    shift
    goto ARGS
)
if /i "%~1"=="--dry-run" (
    set "PSARGS=%PSARGS% -DryRun"
    shift
    goto ARGS
)
echo [ERROR] 未知参数 %~1
goto USAGE
:ARGSDONE

REM ---- 前置检查 ----
where git >nul 2>nul
if errorlevel 1 (
    echo [ERROR] 未找到 git
    popd
    exit /b 1
)
if not exist "%SCRIPTDIR%pr.ps1" (
    echo [ERROR] 未找到 scripts\pr.ps1
    popd
    exit /b 1
)

echo.
echo ================ C3 Pull Request ================
echo   仓库   %REPO%
echo   base   %BASE%
if defined HEAD (
    echo   head   %HEAD%
) else (
    echo   head   当前分支
)
if defined TITLE echo   标题   %TITLE%
echo   说明   只创建 PR, 不自动合并, 需人工审核后手动合并
echo.

REM ---- 调 PowerShell 走 GitCode API 创建 PR ----
set "PSHEAD="
if defined HEAD set "PSHEAD=-Head %HEAD%"
set "PSTITLE="
if defined TITLE set "PSTITLE=-Title "%TITLE%""

powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPTDIR%pr.ps1" -Repo "%REPO%" -Base "%BASE%" %PSHEAD% %PSTITLE% %PSARGS%
if errorlevel 1 (
    echo [ERROR] 创建 PR 失败
    popd
    exit /b 1
)
popd
exit /b 0

:USAGE
echo.
echo pr.bat - 为当前分支创建 GitCode Pull Request
echo.
echo 用法:
echo   scripts\pr.bat [选项]
echo.
echo 选项:
echo   -b 分支                base 分支, 默认 main
echo   -s 分支                head 分支, 默认当前分支
echo   -t 标题                PR 标题, 默认取提交标题
echo   -p, --push             创建前先 git push 到 origin
echo   -o, --open             创建后在浏览器打开 PR 页面
echo   -n, --dry-run          预演, 只打印内容不调 API
echo   -h, --help             显示本帮助
echo.
echo 令牌: 环境变量 GITCODE_TOKEN 优先, 其次 scripts\.gitcode_token,
echo       最后自动从 git 凭据管理器 (gitcode.com) 读取.
echo.
exit /b 0
