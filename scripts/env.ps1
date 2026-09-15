?#!/usr/bin/env powershell
# ============================================================
#  env.ps1 - 加载 MSVC 环境变量
#  用法: . .\scripts\env.ps1  (注意前面的点号 - dot-source)
#  效果: 当前 PowerShell 会话获得 MSVC 编译环境
#  加载后可直接使用 cl.exe, cmake, ninja, C3.exe 等
# ============================================================

# vcvarsall 路径: 环境变量 C3_VCVARSALL 优先, 未设置时 vswhere 自动探测
# (兼容 Community/Professional/Enterprise/BuildTools 多实例). 详见 scripts\README.md
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
    Write-Host "[ERROR] 未找到 vcvarsall.bat, 请设置环境变量 C3_VCVARSALL 指向其完整路径" -ForegroundColor Red
    return
}
# 项目根目录: 环境变量 C3_PROJECT_DIR 优先, 未设置取脚本所在目录的上一级. 详见 scripts\README.md
$ProjectDir = if ($env:C3_PROJECT_DIR) { $env:C3_PROJECT_DIR } else { Split-Path -Parent $PSScriptRoot }

Write-Host "[INFO] Loading MSVC environment..." -ForegroundColor Yellow

$tempBat = "$env:TEMP\vcvars_env.bat"
cmd /c "call `"$VcVars`" x64 >nul 2>&1 && set" | Out-File $tempBat -Encoding ASCII
$count = 0
Get-Content $tempBat | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') {
        [Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
        $count++
    }
}
Remove-Item $tempBat -ErrorAction SilentlyContinue

Write-Host "[OK] Loaded $count environment variables" -ForegroundColor Green

# 验证
$clOk = $null -ne (Get-Command cl.exe -ErrorAction SilentlyContinue)
$cmakeOk = $null -ne (Get-Command cmake -ErrorAction SilentlyContinue)
$c3Ok = Test-Path "$ProjectDir\.build\C3.exe"

Write-Host ""
Write-Host "Tool check:" -ForegroundColor Cyan
Write-Host "  cl.exe:    $(if($clOk){'OK'}else{'MISSING'})" -ForegroundColor $(if($clOk){'Green'}else{'Red'})
Write-Host "  cmake:     $(if($cmakeOk){'OK'}else{'MISSING'})" -ForegroundColor $(if($cmakeOk){'Green'}else{'Red'})
Write-Host "  C3.exe:    $(if($c3Ok){'OK'}else{'NOT BUILT'})" -ForegroundColor $(if($c3Ok){'Green'}else{'Red'})
