# tests_github/run_t0.ps1 - T0 最小编译门禁 (GitHub CI 专用)
#
# 触发: .github/workflows/ci_t0.yml, 打 tag github-test-* 推送到 GitHub 即运行
#   git tag github-test-001
#   git push <github 远端> github-test-001
# 内容三段:
#   1. C3.exe --version 冒烟 (产物可用性)
#   2. t0_cases\*.bas 全部 --syntax-only (语法门禁, 秒级, 不需要 MSVC 环境)
#   3. smoke.bas 编译 + 运行 + 输出断言 (end-to-end 最小闭环, 需要 MSVC 环境)
# 与 tests\run_tests.ps1 的关系: T0 用例原位于 tests\ (2026-09-20 git mv 到此),
#   run_tests.ps1 跨目录引用同一份文件, 两边不产生副本。
#
# 环境: 环境变量 C3_VCVARSALL 优先 (vcvarsall.bat 完整路径), 缺省 vswhere 自动发现,
#       用法见 scripts\README.md。仅限 Windows 运行。
# 用法: pwsh -File tests_github\run_t0.ps1 [-C3Path .build\C3.exe] [-Verbose]

param(
    [string]$C3Path = "",
    [switch]$Verbose
)

$ErrorActionPreference = "SilentlyContinue"

$Root = Split-Path -Parent $PSScriptRoot
if (-not $C3Path) { $C3Path = Join-Path $Root ".build\C3.exe" }
$CasesDir = Join-Path $PSScriptRoot "t0_cases"
$OutDir = Join-Path $Root "output"

if (-not (Test-Path $C3Path)) {
    Write-Host "[ERROR] C3.exe 不存在: $C3Path (先构建或用 -C3Path 指定)" -ForegroundColor Red
    exit 1
}
if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir | Out-Null }

# === MSVC 环境 (smoke 编译需要 cl/link; --syntax-only 不需要) ===
# 手法与 tests\run_tests.ps1 一致: cmd 导出 vcvarsall 环境后注入当前进程
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$VcVars = $env:C3_VCVARSALL
if (-not $VcVars -and (Test-Path $vswhere)) {
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
    if ($vsPath) {
        $candidate = Join-Path $vsPath "VC\Auxiliary\Build\vcvarsall.bat"
        if (Test-Path $candidate) { $VcVars = $candidate }
    }
}
if ($VcVars) {
    $envLines = cmd /c "call `"$VcVars`" x64 >nul 2>&1 && set" 2>$null
    foreach ($line in $envLines) {
        if ($line -match '^([^=]+)=(.*)$') {
            [Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
        }
    }
    Write-Host "MSVC env: $VcVars"
} else {
    Write-Host "[WARN] 未找到 vcvarsall.bat, smoke 编译段将跳过 (语法段不受影响)" -ForegroundColor Yellow
    $VcVars = ""
}

$script:pass = 0
$script:fail = 0
$script:skip = 0

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  T0 Minimal Gate (tests_github)" -ForegroundColor Cyan
Write-Host "  $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# === 段 1: 版本冒烟 ===
Write-Host "--- 1/3 C3.exe version smoke ---" -ForegroundColor Yellow
& $C3Path --version
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] C3.exe --version 失败 (退出码 $LASTEXITCODE)" -ForegroundColor Red
    exit 1
}
Write-Host ""

# === 段 2: 语法门禁 (t0_cases\*.bas 全量 --syntax-only) ===
Write-Host "--- 2/3 Syntax gate (t0_cases\*.bas) ---" -ForegroundColor Yellow
$cases = @(Get-ChildItem -Path $CasesDir -Filter "*.bas" | Sort-Object Name)
if ($cases.Count -eq 0) {
    Write-Host "[ERROR] t0_cases\ 下无 .bas 用例" -ForegroundColor Red
    exit 1
}
foreach ($c in $cases) {
    Write-Host -NoNewline "  [SYNTAX] $($c.BaseName) ... "
    $result = & $C3Path $c.FullName --syntax-only 2>&1
    if ($LASTEXITCODE -eq 0) {
        $script:pass++
        Write-Host "PASS" -ForegroundColor Green
    } else {
        $script:fail++
        Write-Host "FAIL" -ForegroundColor Red
        if ($Verbose) { Write-Host ($result | Out-String) }
    }
}
Write-Host ""

# === 段 3: smoke end-to-end (编译+运行+输出断言) ===
Write-Host "--- 3/3 Smoke end-to-end ---" -ForegroundColor Yellow
$smokeSrc = Join-Path $CasesDir "smoke.bas"
if (-not $VcVars) {
    $script:skip++
    Write-Host "  [SKIP] smoke (无 MSVC 环境)" -ForegroundColor Yellow
} else {
    Write-Host -NoNewline "  [SMOKE] compile ... "
    & $C3Path $smokeSrc --output-dir $OutDir 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) {
        $script:fail++
        Write-Host "FAIL (compile)" -ForegroundColor Red
    } else {
        $smokeExe = Join-Path $OutDir "smoke.exe"
        if (-not (Test-Path $smokeExe)) {
            $script:fail++
            Write-Host "FAIL (no exe)" -ForegroundColor Red
        } else {
            Write-Host "PASS" -ForegroundColor Green
            $expected = @("SMOKE-1:OK", "SMOKE-2:OK", "SMOKE-3:OK", "SMOKE PASS")
            # 运行: .NET Process + 5s 超时 + stdout 重定向 (手法对齐 tests\run_tests.ps1 Invoke-TestExe)
            $stdoutFile = Join-Path $OutDir "t0_smoke.out"
            $proc = $null
            try {
                $psi = New-Object System.Diagnostics.ProcessStartInfo
                $psi.FileName = $smokeExe
                $psi.WorkingDirectory = $OutDir
                $psi.UseShellExecute = $false
                $psi.CreateNoWindow = $false
                $psi.RedirectStandardOutput = $true
                $psi.RedirectStandardError = $true
                $proc = [System.Diagnostics.Process]::Start($psi)
                $soTask = $proc.StandardOutput.ReadToEndAsync()
                $seTask = $proc.StandardError.ReadToEndAsync()
                if (-not $proc.WaitForExit(5000)) {
                    try { $proc.Kill() } catch { }
                    $proc.WaitForExit()
                    $script:fail++
                    Write-Host "  [SMOKE] run FAIL (timeout 5s)" -ForegroundColor Red
                    $proc = $null
                }
            } catch {
                $script:fail++
                Write-Host "  [SMOKE] run FAIL ($($_.Exception.Message))" -ForegroundColor Red
                $proc = $null
            }
            if ($proc) {
                $stdout = $soTask.Result
                $stderr = $seTask.Result
                [System.IO.File]::WriteAllText($stdoutFile, [string]$stdout, [System.Text.Encoding]::Default)
                [System.IO.File]::WriteAllText(($stdoutFile -replace '\.out$', '.err'), [string]$stderr, [System.Text.Encoding]::Default)
                $runOutput = @(Get-Content $stdoutFile -ErrorAction SilentlyContinue)
                $allMatch = $true
                foreach ($e in $expected) {
                    if (-not ($runOutput | Where-Object { $_ -like "*$e*" })) { $allMatch = $false; break }
                }
                if ($allMatch) {
                    $script:pass++
                    Write-Host "  [SMOKE] run + output PASS" -ForegroundColor Green
                } else {
                    $script:fail++
                    Write-Host "  [SMOKE] run FAIL (output mismatch)" -ForegroundColor Red
                    if ($Verbose) {
                        Write-Host "  Expected: $($expected -join ', ')"
                        Write-Host "  Got: $($runOutput -join ' | ')"
                    }
                }
            }
        }
    }
}
Write-Host ""

# === 汇总 (语法用例 + smoke 1 项) ===
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  T0 Results: PASS=$($script:pass) FAIL=$($script:fail) SKIP=$($script:skip) TOTAL=$($script:pass + $script:fail + $script:skip)" -ForegroundColor $(if ($script:fail -gt 0) { "Red" } else { "Green" })
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

if ($script:fail -gt 0) { exit 1 } else { exit 0 }
