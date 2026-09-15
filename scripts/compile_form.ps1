#!/usr/bin/env powershell
# ============================================================
#  compile_form.ps1 - 编译 .frm 窗体并可选运行
#  用法: compile_form.ps1 <form.frm> [-Run] [-TimeoutSec 5]
#  示例:
#    .\compile_form.ps1 tests\test_form\empty_form.frm
#    .\compile_form.ps1 tests\test_form\form_test_p74.frm -Run -TimeoutSec 8
# ============================================================

param(
    [Parameter(Mandatory=$true)]
    [string]$FormFile,
    [switch]$Run,
    [int]$TimeoutSec = 5
)

$ProjectDir = "D:\vb6pro"
$VcVars = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"

# 加载 MSVC 环境
$tempBat = "$env:TEMP\vcvars_env.bat"
cmd /c "call `"$VcVars`" x64 >nul 2>&1 && set" | Out-File $tempBat -Encoding ASCII
Get-Content $tempBat | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') {
        [Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
    }
}
Remove-Item $tempBat -ErrorAction SilentlyContinue

# 确保 .frm 路径
if (-not (Test-Path $FormFile)) {
    $FormFile = Join-Path $ProjectDir $FormFile
}
if (-not (Test-Path $FormFile)) {
    Write-Host "[ERROR] 找不到: $FormFile" -ForegroundColor Red
    exit 1
}

# 编译
Write-Host "[INFO] Compiling: $FormFile" -ForegroundColor Yellow
$result = & "$ProjectDir\.build\C3.exe" $FormFile --output-dir "$ProjectDir\output" 2>&1
$result | ForEach-Object { Write-Host $_ }

if ($LASTEXITCODE -ne 0) {
    Write-Host "[FAIL] Compile failed" -ForegroundColor Red
    exit 1
}

# 确定 EXE 路径
$baseName = [System.IO.Path]::GetFileNameWithoutExtension($FormFile)
$exePath = Join-Path "$ProjectDir\output" "$baseName.exe"

if ($Run -and (Test-Path $exePath)) {
    Write-Host "[INFO] Running: $exePath (timeout=${TimeoutSec}s)" -ForegroundColor Yellow
    $proc = Start-Process -FilePath $exePath -PassThru -ErrorAction Stop
    Start-Sleep -Seconds $TimeoutSec
    if (-not $proc.HasExited) {
        Stop-Process -Id $proc.Id -Force
        Write-Host "[OK] Window closed after ${TimeoutSec}s timeout" -ForegroundColor Green
    } else {
        Write-Host "[OK] Process exited with code: $($proc.ExitCode)" -ForegroundColor Green
    }
}

Write-Host "[OK] Done" -ForegroundColor Green
