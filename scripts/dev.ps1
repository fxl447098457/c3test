#!/usr/bin/env powershell
# ============================================================
#  dev.ps1 - 一键构建+测试
#  用法: dev [skip-build] [skip-test]
#    dev           = 构建 + 全部测试
#    dev sb        = 跳过构建，只跑测试
#    dev st        = 只构建，跳过测试
#    dev -TestCategory smoke  = 只跑冒烟测试 (tests\smoke.bas)
#  适用于 PowerShell 环境 (Agent 会话内)
# ============================================================

param(
    [switch]$SkipBuild,
    [switch]$SkipTest,
    [switch]$NoObjCache,
    [string]$TestCategory = "all"
)

$ErrorActionPreference = "Continue"
# 项目根目录: 环境变量优先, 缺省由脚本位置推导 (dev.ps1 位于 <root>\scripts\). 详见 scripts\README.md
$ProjectDir = if ($env:C3_PROJECT_DIR) { $env:C3_PROJECT_DIR } else { Split-Path -Parent $PSScriptRoot }
# vcvarsall 路径: C3_VCVARSALL 优先, 未设置时 vswhere 自动探测 (同 tests\run_tests.ps1, 支持 BuildTools/CI 环境)
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$VcVars = $env:C3_VCVARSALL
if (-not $VcVars -and (Test-Path $vswhere)) {
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
    if ($vsPath) {
        $candidate = Join-Path $vsPath "VC\Auxiliary\Build\vcvarsall.bat"
        if (Test-Path $candidate) { $VcVars = $candidate }
    }
}
if (-not $VcVars) {
    Write-Host "[ERROR] 未找到 vcvarsall.bat, 请设置 C3_VCVARSALL 环境变量 (详见 scripts\README.md)" -ForegroundColor Red
    exit 1
}

# --- 加载 MSVC 环境 ---
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  c3 Dev Workflow" -ForegroundColor Cyan
Write-Host "  $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

function Load-MSVC {
    $tempBat = "$env:TEMP\vcvars_env.bat"
    cmd /c "call `"$VcVars`" x64 >nul 2>&1 && set" | Out-File $tempBat -Encoding ASCII
    Get-Content $tempBat | ForEach-Object {
        if ($_ -match '^([^=]+)=(.*)$') {
            [Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
        }
    }
    Remove-Item $tempBat -ErrorAction SilentlyContinue
}

# --- 构建 ---
if (-not $SkipBuild) {
    Write-Host "--- Build ---" -ForegroundColor Yellow
    Load-MSVC
    $buildOut = & cmd /c "call `"$VcVars`" x64 >nul 2>&1 && cd /d $ProjectDir && cmake --build .build --config Release 2>&1" 2>&1
    $buildOut | ForEach-Object { Write-Host $_ }
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[FAIL] Build failed" -ForegroundColor Red
        exit 1
    }
    Write-Host "[OK] Build succeeded" -ForegroundColor Green
    Write-Host ""
}

# --- 测试 ---
if (-not $SkipTest) {
    Write-Host "--- Test ($TestCategory) ---" -ForegroundColor Yellow
    # ai/030 §十: 本地这条默认吃 obj store (RTL 不再每例重编, 一趟 bas 实测 282 s → 36 s)。
    # 编译器自己的默认路径没动 —— 变的是这里显式传开关; 要复现 GA 那条路加 -NoObjCache。
    # (别用数组 splat 传这串: @('-Category', $x, '-Incremental') 会被按位置绑定, -Incremental
    #  会撞进第 4 个形参 $Jobs ⇒ "Cannot convert value '-Incremental' to type Int32"。)
    if ($NoObjCache) {
        & "$ProjectDir\tests\run_tests.ps1" -Category $TestCategory
    } else {
        & "$ProjectDir\tests\run_tests.ps1" -Category $TestCategory -Incremental
    }
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[FAIL] Tests failed" -ForegroundColor Red
        exit 1
    }
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "  All done!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
