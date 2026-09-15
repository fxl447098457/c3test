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
    [string]$TestCategory = "all"
)

$ErrorActionPreference = "Continue"
$ProjectDir = "D:\code\vi\c3.vb6.pro"
$VcVars = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"

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
    & "$ProjectDir\tests\run_tests.ps1" -Category $TestCategory
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[FAIL] Tests failed" -ForegroundColor Red
        exit 1
    }
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "  All done!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
