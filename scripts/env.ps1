#!/usr/bin/env powershell
# ============================================================
#  env.ps1 - 加载 MSVC + VB6RTL 环境变量
#  用法: . .\scripts\env.ps1  (注意前面的点号 - dot-source)
#  效果: 当前 PowerShell 会话获得 MSVC 编译环境和 VB6RTL_DIR
#  加载后可直接使用 cl.exe, cmake, ninja, c3.exe 等
# ============================================================

$VcVars = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
$ProjectDir = "D:\vb6pro"

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

$env:VB6RTL_DIR = "$ProjectDir\src\rtl\core"

Write-Host "[OK] Loaded $count environment variables" -ForegroundColor Green
Write-Host "[OK] VB6RTL_DIR = $env:VB6RTL_DIR" -ForegroundColor Green

# 验证
$clOk = $null -ne (Get-Command cl.exe -ErrorAction SilentlyContinue)
$cmakeOk = $null -ne (Get-Command cmake -ErrorAction SilentlyContinue)
$c3Ok = Test-Path "$ProjectDir\.build\c3.exe"

Write-Host ""
Write-Host "Tool check:" -ForegroundColor Cyan
Write-Host "  cl.exe:    $(if($clOk){'OK'}else{'MISSING'})" -ForegroundColor $(if($clOk){'Green'}else{'Red'})
Write-Host "  cmake:     $(if($cmakeOk){'OK'}else{'MISSING'})" -ForegroundColor $(if($cmakeOk){'Green'}else{'Red'})
Write-Host "  c3.exe:    $(if($c3Ok){'OK'}else{'NOT BUILT'})" -ForegroundColor $(if($c3Ok){'Green'}else{'Red'})
