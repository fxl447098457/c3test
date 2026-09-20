# tests_github/run_t2.ps1 - T2 全量回归 (GitHub CI 专用, 自包含, 夜间层)
#
# 触发: .github/workflows/ci_t2.yml (夜间 cron + 手动 dispatch + tag github-t2-*)
# 内容两类 (口径 2026-09-20 与用户对齐: 有交互的 GUI 不做自动化, GUI 用例只编译):
#   1. vbp 控制台工程 7 个: 编译 + 运行 + 输出断言 (无 GUI, 纯 stdout; 串行)
#      test_vbman 依赖外部 COM (VBMANLIB), 未注册环境自动 SKIP
#   2. GUI vbp 工程 3 个: 只编译不运行 (窗体验证留给本机 T3/L1-L3 体系)
# 与 tests\run_tests.ps1 的关系: 用例 2026-09-20 cp 自 tests\ (复制而非引用) ——
#   方向是 tests_github 自包含、后续废弃 tests\; 本脚本自带清单与引擎。
# 环境: 环境变量 C3_VCVARSALL 优先 (vcvarsall.bat 完整路径), 缺省 vswhere 自动发现,
#       用法见 scripts\README.md。仅限 Windows + PowerShell 7。
# 用法: pwsh -File tests_github\run_t2.ps1 [-C3Path .build\C3.exe] [-Verbose]

param(
    [string]$C3Path = "",
    [switch]$Verbose
)

$ErrorActionPreference = "SilentlyContinue"

$Root = Split-Path -Parent $PSScriptRoot
if (-not $C3Path) { $C3Path = Join-Path $Root ".build\C3.exe" }
$CasesDir = Join-Path $PSScriptRoot "t2_cases"
$OutDir = Join-Path $Root "output"

if (-not (Test-Path $C3Path)) {
    Write-Host "[ERROR] C3.exe 不存在: $C3Path (先构建或用 -C3Path 指定)" -ForegroundColor Red
    exit 1
}
if (-not (Test-Path $CasesDir)) {
    Write-Host "[ERROR] 用例目录不存在: $CasesDir" -ForegroundColor Red
    exit 1
}
if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir | Out-Null }

# === MSVC 环境 (手法与 tests\run_tests.ps1 一致: cmd 导出 vcvarsall 后注入当前进程) ===
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
    Write-Host "[ERROR] 未找到 vcvarsall.bat (T2 全部用例都需要编译)" -ForegroundColor Red
    exit 1
}
$envLines = cmd /c "call `"$VcVars`" x64 >nul 2>&1 && set" 2>$null
foreach ($line in $envLines) {
    if ($line -match '^([^=]+)=(.*)$') {
        [Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
    }
}
Write-Host "MSVC env: $VcVars"

$script:pass = 0
$script:fail = 0
$script:skip = 0

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  T2 Full Regression (tests_github)" -ForegroundColor Cyan
Write-Host "  $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# === COM 注册检查 (32 位程序读 WOW6432Node 视图; 未注册的依赖组件走 SKIP 不算 FAIL) ===
function Test-ComRegistered {
    param([string]$ProgId, [string]$Arch)
    if ($Arch -eq "x86") {
        $paths = @("HKLM:\SOFTWARE\WOW6432Node\Classes\$ProgId")
    } else {
        $paths = @("HKLM:\SOFTWARE\Classes\$ProgId")
    }
    foreach ($p in $paths) {
        if (Test-Path -Path $p) { return $true }
    }
    return $false
}

# === 运行产物并捕获输出 (.NET Process + 5s 超时; 手法对齐 run_tests.ps1 Invoke-TestExe) ===
function Invoke-TestExe {
    param(
        [string]$ExePath,
        [string]$WorkDir,
        [string]$Name
    )
    $stdoutFile = Join-Path $WorkDir "$Name.out"
    $stderrFile = Join-Path $WorkDir "$Name.err"
    Remove-Item $stdoutFile, $stderrFile -ErrorAction SilentlyContinue

    $errors = @()
    # --- 方案 A: .NET Process + 重定向 ---
    try {
        $psi = New-Object System.Diagnostics.ProcessStartInfo
        $psi.FileName = $ExePath
        $psi.WorkingDirectory = $WorkDir
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
            return @{ Ok = $false; ExitCode = $null; Output = @(); Detail = "run timeout: 5s" }
        }
        [System.IO.File]::WriteAllText($stdoutFile, [string]$soTask.Result, [System.Text.Encoding]::Default)
        [System.IO.File]::WriteAllText($stderrFile, [string]$seTask.Result, [System.Text.Encoding]::Default)
        return @{
            Ok       = $true
            ExitCode = $proc.ExitCode
            Output   = (Get-Content $stdoutFile -ErrorAction SilentlyContinue)
            Detail   = "dotnet"
        }
    } catch {
        $errors += ("[dotnet] " + $_.Exception.Message)
    }
    # --- 方案 B: Start-Process 回退 ---
    try {
        $proc = Start-Process -FilePath $ExePath -NoNewWindow -Wait -PassThru `
            -WorkingDirectory $WorkDir `
            -RedirectStandardOutput $stdoutFile `
            -RedirectStandardError $stderrFile `
            -ErrorAction Stop
        return @{
            Ok       = $true
            ExitCode = $proc.ExitCode
            Output   = (Get-Content $stdoutFile -ErrorAction SilentlyContinue)
            Detail   = "start-process"
        }
    } catch {
        $errors += ("[start-process] " + $_.Exception.Message)
    }
    return @{ Ok = $false; ExitCode = $null; Output = @(); Detail = ($errors -join " | ") }
}

# === 断言辅助: 输出行逐一匹配期望子串 ===
function Assert-Output {
    param([string[]]$ExpectedOutputs, $RunOutput)
    foreach ($expected in $ExpectedOutputs) {
        $found = $RunOutput | Where-Object { $_ -like "*$expected*" }
        if (-not $found) { return $false }
    }
    return $true
}

# ============================================================
# 类 1: vbp 控制台工程 7 个 (编译 + 运行 + 输出断言, 串行)
# ============================================================
Write-Host "--- VBP Project Tests (run + assert) ---" -ForegroundColor Yellow

$script:vbpQueue = @()
function Add-VbpTest {
    param([string]$Name, [string]$VbpFile, [string[]]$Expected = @(), [string]$Arch = "", [string]$RequiresCom = "")
    $script:vbpQueue += @{
        Name        = $Name
        VbpFile     = (Join-Path $CasesDir $VbpFile)
        Expected    = @($Expected)
        Arch        = $Arch
        RequiresCom = $RequiresCom
    }
}

Add-VbpTest "test_class" "test_class.vbp" @("3", "0")
Add-VbpTest "M6Test" "M6Test.vbp" @("M6A:OK", "M6B:OK", "M6C:OK", "M6D:OK", "M6 PASSED")
Add-VbpTest "modulemethod" "test_modulemethod.vbp" @("30", "21")
Add-VbpTest "test_implements" "test_implements.vbp" @("IMPL1:OK", "IMPL2:OK", "Implements test PASSED")
Add-VbpTest "test_events" "test_events\test_events.vbp" @("Events test PASSED")
Add-VbpTest "M7Test" "m7_test\M7Test.vbp" @("4/4 PASSED")
Add-VbpTest "test_vbman" "test_vbman\test_vbman.vbp" @("P24-04a:OK", "P24-04b:OK", "P24-04:2/2") -Arch "x86" -RequiresCom "VBMANLIB.cVBMAN"

if ($script:vbpQueue.Count -ne 7) {
    Write-Host "[ERROR] vbp 清单数量异常: $($script:vbpQueue.Count) (应为 7)" -ForegroundColor Red
    exit 1
}
$missing = @($script:vbpQueue | Where-Object { -not (Test-Path $_.VbpFile) })
if ($missing.Count -gt 0) {
    Write-Host "[ERROR] 清单引用了不存在的工程文件:" -ForegroundColor Red
    foreach ($m in $missing) { Write-Host "    $($m.VbpFile)" -ForegroundColor Red }
    exit 1
}

function Test-Vbp {
    param([object]$It)
    Write-Host -NoNewline "  [VBP] $($It.Name) ... "

    # 依赖的外部 COM 组件未注册: SKIP (环境原因, 不计 FAIL)
    if ($It.RequiresCom -and -not (Test-ComRegistered $It.RequiresCom $It.Arch)) {
        $script:skip++
        Write-Host "SKIP (COM '$($It.RequiresCom)' 未注册)" -ForegroundColor Yellow
        return
    }

    if ($It.Arch) {
        $compileResult = & $C3Path $It.VbpFile --arch $It.Arch --output-dir $OutDir 2>&1
    } else {
        $compileResult = & $C3Path $It.VbpFile --output-dir $OutDir 2>&1
    }
    if ($LASTEXITCODE -ne 0) {
        $script:fail++
        Write-Host "FAIL (compile)" -ForegroundColor Red
        if ($Verbose) { Write-Host ($compileResult | Out-String) }
        return
    }

    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($It.VbpFile)
    $exePath = Join-Path $OutDir "$baseName.exe"
    if (-not (Test-Path $exePath)) {
        $script:fail++
        Write-Host "FAIL (no exe)" -ForegroundColor Red
        return
    }

    $run = Invoke-TestExe -ExePath $exePath -WorkDir $OutDir -Name $baseName
    if (-not $run.Ok) {
        $script:fail++
        Write-Host "FAIL (run error)" -ForegroundColor Red
        Write-Host ("    " + $run.Detail) -ForegroundColor Red
        return
    }

    if ($It.Expected.Count -gt 0) {
        if (Assert-Output $It.Expected $run.Output) {
            $script:pass++
            Write-Host "PASS" -ForegroundColor Green
        } else {
            $script:fail++
            Write-Host "FAIL (output mismatch)" -ForegroundColor Red
            if ($Verbose) {
                Write-Host "  Expected: $($It.Expected -join ', ')"
                Write-Host "  Got: $($run.Output -join ' | ')"
            }
        }
    } else {
        $script:pass++
        Write-Host "PASS" -ForegroundColor Green
    }
}

$vbpSw = [Diagnostics.Stopwatch]::StartNew()
foreach ($it in $script:vbpQueue) { Test-Vbp $it }
$vbpSw.Stop()
Write-Host "  (vbp tests took $([Math]::Round($vbpSw.Elapsed.TotalSeconds))s)"
Write-Host ""

# ============================================================
# 类 2: GUI vbp 工程 3 个 (只编译不运行; 交互式 GUI 不做自动化)
# ============================================================
Write-Host "--- GUI VBP Compile-Only Tests ---" -ForegroundColor Yellow

$script:guiQueue = @()
function Add-GuiCompileTest {
    param([string]$Name, [string]$VbpFile, [string]$Arch = "")
    $script:guiQueue += @{
        Name    = $Name
        VbpFile = (Join-Path $CasesDir $VbpFile)
        Arch    = $Arch
    }
}

Add-GuiCompileTest "VbQRCodegen" "VbQRCodegen-master\test\Project1.vbp"
Add-GuiCompileTest "BalloonTooltips" "BalloonTooltips\prjBalloonTooltips.vbp"
Add-GuiCompileTest "Charts2020" "Charts 2020\Proyecto1.vbp" -Arch "x86"

if ($script:guiQueue.Count -ne 3) {
    Write-Host "[ERROR] GUI 编译清单数量异常: $($script:guiQueue.Count) (应为 3)" -ForegroundColor Red
    exit 1
}
$missingGui = @($script:guiQueue | Where-Object { -not (Test-Path $_.VbpFile) })
if ($missingGui.Count -gt 0) {
    Write-Host "[ERROR] GUI 清单引用了不存在的工程文件:" -ForegroundColor Red
    foreach ($m in $missingGui) { Write-Host "    $($m.VbpFile)" -ForegroundColor Red }
    exit 1
}

function Test-GuiCompileOnly {
    param([object]$It)
    Write-Host -NoNewline "  [GUI-COMPILE] $($It.Name) ... "
    if ($It.Arch) {
        $result = & $C3Path $It.VbpFile --arch $It.Arch --output-dir $OutDir 2>&1
    } else {
        $result = & $C3Path $It.VbpFile --output-dir $OutDir 2>&1
    }
    if ($LASTEXITCODE -eq 0) {
        $script:pass++
        Write-Host "PASS" -ForegroundColor Green
    } else {
        $script:fail++
        Write-Host "FAIL" -ForegroundColor Red
        if ($Verbose) { Write-Host ($result | Out-String) }
    }
}

foreach ($it in $script:guiQueue) { Test-GuiCompileOnly $it }
Write-Host ""

# === 汇总 (vbp 7 + gui 只编译 3 = 10) ===
$total = $script:pass + $script:fail + $script:skip
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  T2 Results: PASS=$($script:pass) FAIL=$($script:fail) SKIP=$($script:skip) TOTAL=$total" -ForegroundColor $(if ($script:fail -gt 0) { "Red" } else { "Green" })
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

if ($script:fail -gt 0) { exit 1 } else { exit 0 }
