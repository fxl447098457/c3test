# c3 编译器集成测试框架
# 用法: .\run_tests.ps1 [-Category <all|compile|run|syntax>] [-Verbose]
#
# 测试分类:
#   smoke   - 冒烟测试 (编译+运行+输出校验, 快速验证 .build\C3.exe 可用)
#   compile - 编译测试 (c3 .bas -> .exe, 不运行)
#   run     - 运行测试 (编译+运行+校验输出)
#   syntax  - 语法测试 (--syntax-only, 不生成代码)
#   all     - 全部 (默认)

param(
    [string]$Category = "all",
    [switch]$Verbose
)

$ErrorActionPreference = "SilentlyContinue"

# === 配置 ===
# 路径全部由脚本自身位置推导, 不再硬编码仓库绝对路径 (旧值 D:\vb6pro 已不存在)
$Root = Split-Path -Parent $PSScriptRoot
$C3 = Join-Path $Root ".build\C3.exe"
$Tests = $PSScriptRoot
$OutDir = Join-Path $Root "output"
# vcvarsall 路径: 环境变量 C3_VCVARSALL 优先, 未设置时 vswhere 自动探测
# (兼容 Community/Professional/Enterprise/BuildTools 多实例及 CI 环境). 详见 scripts\README.md
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
    Write-Host "[ERROR] 未找到 vcvarsall.bat" -ForegroundColor Red
    Write-Host "        请安装 VS2022 并勾选 [使用 C++ 的桌面开发] 工作负载," -ForegroundColor Red
    Write-Host "        或设置环境变量 C3_VCVARSALL 指向其完整路径 (详见 scripts\README.md)" -ForegroundColor Red
    exit 1
}

# === 设置MSVC环境 ===
$msvcOutput = cmd /c "call `"$VcVars`" x64 >nul 2>&1 && echo MSVC_OK" 2>&1
if ($msvcOutput -notcontains "MSVC_OK") {
    Write-Host "[ERROR] 无法初始化MSVC环境" -ForegroundColor Red
    exit 1
}

# 设置MSVC环境变量 (通过临时bat导出)
$tempBat = "$env:TEMP\vcvars_env.bat"
cmd /c "call `"$VcVars`" x64 >nul 2>&1 && set" | Out-File $tempBat -Encoding ASCII
Get-Content $tempBat | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') {
        [Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
    }
}
Remove-Item $tempBat -ErrorAction SilentlyContinue

if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir | Out-Null }

# === 测试计数 ===
$script:pass = 0
$script:fail = 0
$script:skip = 0
$script:total = 0

# === 外部 COM 依赖检测 ===
# 部分测试依赖机器上注册的外部 COM 组件 (如 VBMANLIB)。组件未注册时运行必然失败
# (VB6 运行期错误 429), 但这是环境缺失, 不是编译器缺陷, 应 SKIP 而非 FAIL。
# 32 位程序 (Arch=x86) 受注册表重定向影响, 只能看到 WOW6432Node 视图。
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

# === 编译测试 ===
function Test-Compile {
    param([string]$Name, [string]$Source)
    $script:total++
    Write-Host -NoNewline "  [COMPILE] $Name ... "
    
    $result = & $C3 $Source --output-dir $OutDir 2>&1
    $exitCode = $LASTEXITCODE
    
    if ($exitCode -eq 0) {
        $script:pass++
        Write-Host "PASS" -ForegroundColor Green
    } else {
        $script:fail++
        Write-Host "FAIL" -ForegroundColor Red
        if ($Verbose) { Write-Host ($result | Out-String) }
    }
}

# === 运行被编译出的 exe (统一出口) ===
# 返回 @{ Ok; ExitCode; Output; Detail }
#
# 为什么要有这个函数: 启动外部进程这件事在不同宿主会话下可靠性不同。
# Start-Process 的 -RedirectStandardOutput 依赖宿主允许子进程继承控制台重定向,
# 个别会话里会抛异常 (此时 .out/.err 是 0 字节, 原实现只报 "FAIL (run error)",
# 把环境问题伪装成测试失败 —— 假阴性)。这里按两条路径依次尝试:
#   路径 A: .NET Process + 管道捕获 (只依赖 CreateProcess, 不依赖宿主重定向)
#   路径 B: Start-Process -Redirect* (保留原行为作为后备)
# 两条都失败才算真失败, 并把异常原文一并输出, 避免"失败但不说为什么"。
function Invoke-TestExe {
    param(
        [string]$ExePath,
        [string]$WorkDir,
        [string]$Name
    )

    # 工作目录统一设为 output\: 部分测试用 Open ... For Output 写相对路径文件
    # (scores.txt / test_output.txt / *.dat 等), 不指定就会落进仓库根目录。
    $stdoutFile = Join-Path $WorkDir "$Name.out"
    $stderrFile = Join-Path $WorkDir "$Name.err"
    Remove-Item $stdoutFile, $stderrFile -ErrorAction SilentlyContinue

    $errors = @()

    # --- 路径 A: .NET Process + 管道 ---
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
        $proc.WaitForExit()
        $stdout = $soTask.Result
        $stderr = $seTask.Result

        # 落盘保留产物, 供人工复查 (编码与系统 ANSI 一致, 中文输出不乱码)
        [System.IO.File]::WriteAllText($stdoutFile, [string]$stdout, [System.Text.Encoding]::Default)
        [System.IO.File]::WriteAllText($stderrFile, [string]$stderr, [System.Text.Encoding]::Default)

        return @{
            Ok       = $true
            ExitCode = $proc.ExitCode
            Output   = (Get-Content $stdoutFile -ErrorAction SilentlyContinue)
            Detail   = "dotnet"
        }
    } catch {
        $errors += ("[dotnet] " + $_.Exception.Message)
    }

    # --- 路径 B: Start-Process (原实现, 后备) ---
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

    return @{
        Ok       = $false
        ExitCode = $null
        Output   = @()
        Detail   = ($errors -join " | ")
    }
}

# === 运行测试 (编译+运行+输出校验) ===
function Test-Run {
    param(
        [string]$Name, 
        [string]$Source,
        [string[]]$ExpectedOutputs,  # 预期输出行
        [string]$Arch = ""            # 可选架构参数 (x86/x64)
    )
    $script:total++
    Write-Host -NoNewline "  [RUN] $Name ... "
    
    # 编译
    if ($Arch) {
        $compileResult = & $C3 $Source --arch $Arch --output-dir $OutDir 2>&1
    } else {
        $compileResult = & $C3 $Source --output-dir $OutDir 2>&1
    }
    if ($LASTEXITCODE -ne 0) {
        $script:fail++
        Write-Host "FAIL (compile)" -ForegroundColor Red
        if ($Verbose) { Write-Host ($compileResult | Out-String) }
        return
    }
    
    # 确定exe路径
    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($Source)
    $exePath = Join-Path $OutDir "$baseName.exe"
    if (-not (Test-Path $exePath)) {
        $script:fail++
        Write-Host "FAIL (no exe)" -ForegroundColor Red
        return
    }
    
    # 运行 (等待至结束)
    $run = Invoke-TestExe -ExePath $exePath -WorkDir $OutDir -Name $baseName
    if (-not $run.Ok) {
        $script:fail++
        Write-Host "FAIL (run error)" -ForegroundColor Red
        Write-Host ("    " + $run.Detail) -ForegroundColor Red
        return
    }
    $runOutput = $run.Output
    
    # 校验输出
    if ($ExpectedOutputs -and $ExpectedOutputs.Count -gt 0) {
        $allMatch = $true
        foreach ($expected in $ExpectedOutputs) {
            $found = $runOutput | Where-Object { $_ -like "*$expected*" }
            if (-not $found) {
                $allMatch = $false
                break
            }
        }
        if ($allMatch) {
            $script:pass++
            Write-Host "PASS" -ForegroundColor Green
        } else {
            $script:fail++
            Write-Host "FAIL (output mismatch)" -ForegroundColor Red
            if ($Verbose) {
                Write-Host "  Expected: $($ExpectedOutputs -join ', ')"
                Write-Host "  Got: $($runOutput -join '`n')"
            }
        }
    } else {
        # 无预期输出，只要不崩溃就算通过
        $script:pass++
        Write-Host "PASS" -ForegroundColor Green
    }
}

# === VBP工程测试 (编译+运行+输出校验) ===
function Test-Vbp {
    param(
        [string]$Name,
        [string]$VbpFile,
        [string[]]$ExpectedOutputs,
        [string]$Arch = "",           # 可选架构参数 (x86/x64)
        [string]$RequiresCom = ""     # 依赖的外部 COM ProgId (未注册则 SKIP, 不计 FAIL)
    )
    $script:total++
    Write-Host -NoNewline "  [VBP] $Name ... "

    # 外部 COM 依赖缺失 → SKIP (环境缺失, 非编译器缺陷)
    if ($RequiresCom -and -not (Test-ComRegistered $RequiresCom $Arch)) {
        $script:skip++
        $view = if ($Arch -eq "x86") { "WOW6432Node (32-bit)" } else { "64-bit" }
        Write-Host "SKIP (COM '$RequiresCom' 未注册于 $view 视图)" -ForegroundColor Yellow
        return
    }

    # 编译VBP工程
    if ($Arch) {
        $compileResult = & $C3 $VbpFile --arch $Arch --output-dir $OutDir 2>&1
    } else {
        $compileResult = & $C3 $VbpFile --output-dir $OutDir 2>&1
    }
    if ($LASTEXITCODE -ne 0) {
        $script:fail++
        Write-Host "FAIL (compile)" -ForegroundColor Red
        if ($Verbose) { Write-Host ($compileResult | Out-String) }
        return
    }

    # 从VBP文件名推导exe路径
    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($VbpFile)
    $exePath = Join-Path $OutDir "$baseName.exe"
    if (-not (Test-Path $exePath)) {
        $script:fail++
        Write-Host "FAIL (no exe)" -ForegroundColor Red
        return
    }

    # 运行 (等待至结束)
    $run = Invoke-TestExe -ExePath $exePath -WorkDir $OutDir -Name $baseName
    if (-not $run.Ok) {
        $script:fail++
        Write-Host "FAIL (run error)" -ForegroundColor Red
        Write-Host ("    " + $run.Detail) -ForegroundColor Red
        return
    }
    $runOutput = $run.Output

    # 校验输出
    if ($ExpectedOutputs -and $ExpectedOutputs.Count -gt 0) {
        $allMatch = $true
        foreach ($expected in $ExpectedOutputs) {
            $found = $runOutput | Where-Object { $_ -like "*$expected*" }
            if (-not $found) {
                $allMatch = $false
                break
            }
        }
        if ($allMatch) {
            $script:pass++
            Write-Host "PASS" -ForegroundColor Green
        } else {
            $script:fail++
            Write-Host "FAIL (output mismatch)" -ForegroundColor Red
            if ($Verbose) {
                Write-Host "  Expected: $($ExpectedOutputs -join ', ')"
                Write-Host "  Got: $($runOutput -join '`n')"
            }
        }
    } else {
        $script:pass++
        Write-Host "PASS" -ForegroundColor Green
    }
}

# === 语法测试 ===
function Test-Syntax {
    param([string]$Name, [string]$Source)
    $script:total++
    Write-Host -NoNewline "  [SYNTAX] $Name ... "
    
    $result = & $C3 $Source --syntax-only 2>&1
    if ($LASTEXITCODE -eq 0) {
        $script:pass++
        Write-Host "PASS" -ForegroundColor Green
    } else {
        $script:fail++
        Write-Host "FAIL" -ForegroundColor Red
        if ($Verbose) { Write-Host ($result | Out-String) }
    }
}

# =============================================
# 运行测试
# =============================================

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  C3 Compiler Test Suite" -ForegroundColor Cyan
Write-Host "  $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# --- 冒烟测试 ---
# 重建 C3.exe 后先跑这个: 只验证 "解析 -> 生成C -> cl/link -> 运行" 全链路。
# 用例见 tests\smoke.bas (文件头写了维护约束: 禁止 MsgBox 等阻塞语句)。
if ($Category -in @("all", "smoke")) {
    Write-Host "--- Smoke Test (C3.exe end-to-end) ---" -ForegroundColor Yellow

    Test-Run "smoke" "$Tests\smoke.bas" @("SMOKE-1:OK", "SMOKE-2:OK", "SMOKE-3:OK", "SMOKE PASS")
    Write-Host ""
}

# --- 回归测试 (必须始终通过) ---
if ($Category -in @("all", "run")) {
    Write-Host "--- Regression (Compile+Run) ---" -ForegroundColor Yellow
    
    Test-Run "hello" "$Tests\hello.bas"
    Test-Run "test_m5" "$Tests\test_m5.bas"
    Test-Run "test_rtl" "$Tests\test_rtl.bas"
    Test-Run "test_array" "$Tests\test_array.bas"
    Test-Run "test_fileio" "$Tests\test_fileio.bas"
    Test-Run "test_error" "$Tests\test_error.bas"
    Test-Run "test_now" "$Tests\test_now.bas"
    Test-Run "test_getput" "$Tests\test_getput.bas" @("PASS1a", "PASS1b", "PASS1c", "PASS2", "PASS3", "PASS4")
    Test-Run "test_onerror" "$Tests\test_onerror.bas" @("PASS1", "PASS2", "PASS3a", "PASS3b", "Done")
    Test-Run "test_ndarray" "$Tests\test_ndarray.bas" @("2D sum=270", "P8.1 ALL TESTS DONE")
    Test-Run "test_foreach" "$Tests\test_foreach4.bas" @("Long For Each: 150", "String For Each: Hello World")
    Test-Run "test_softkeyword" "$Tests\test_softkeyword.bas" @("Get=42", "Step=5", "Name=test")
    Test-Run "test_date" "$Tests\test_date.bas" @("PASS_Year", "PASS_Month", "PASS_Day", "PASS_NowYear", "Done")
    Test-Run "test_colon" "$Tests\test_colon.bas" @("PASS1", "PASS2", "PASS3", "Done")
    Test-Run "test_variant" "$Tests\test_variant.bas" @("PASS1a", "PASS1c", "PASS5", "Done")
    Write-Host ""
    
    # --- P5.5 兼容性测试 ---
    Write-Host "--- Compat Tests (P5.5) ---" -ForegroundColor Yellow
    
    Test-Run "test_compat" "$Tests\test_compat.bas"
    Test-Run "test_types" "$Tests\test_types.bas"
    Test-Run "test_control" "$Tests\test_control.bas"
    Test-Run "test_declare" "$Tests\test_declare.bas"
    Write-Host ""
    
    # --- P5.7 已知限制修复测试 ---
    Write-Host "--- Bugfix Tests (P5.7) ---" -ForegroundColor Yellow
    
    Test-Run "test_fixes" "$Tests\test_fixes.bas" @("FIX1:OK", "FIX2:OK", "FIX3:OK", "All fixes passed!")
    Write-Host ""
    
    # --- VBP工程测试 (P5) ---
    Write-Host "--- VBP Project Tests (P5) ---" -ForegroundColor Yellow
    
    Test-Vbp "test_class" "$Tests\test_class.vbp" @("3", "0")

    Test-Vbp "M6Test" "$Tests\M6Test.vbp" @("M6A:OK", "M6B:OK", "M6C:OK", "M6D:OK", "M6 PASSED")
    Test-Vbp "modulemethod" "$Tests\test_modulemethod.vbp" @("30", "21")
    Write-Host ""
    
    # --- P6 COM 测试 ---
    Write-Host "--- COM Tests (P6) ---" -ForegroundColor Yellow
    
    Test-Run "test_com" "$Tests\test_com.bas" @("COM-1:OK", "COM-2:OK", "COM-3:OK", "COM:3/3")
    Test-Run "test_com2" "$Tests\test_com2.bas" @("Users")
    Test-Run "test_com3" "$Tests\test_com3.bas" @("COM3-1:OK", "COM3-2:OK", "COM3-3:OK", "COM3-4:OK", "COM3:4/4")
    Test-Run "test_earlybound" "$Tests\test_earlybound.bas" @("EB-1:OK", "EB-2:OK", "EB:2/2")
    Test-Run "test_p1324" "$Tests\test_p1324.bas" @("P13-1:OK", "P13-3:OK", "P13-5:OK", "P13:8/8")
    Test-Vbp "test_implements" "$Tests\test_implements.vbp" @("IMPL1:OK", "IMPL2:OK", "Implements test PASSED")
    Test-Vbp "test_events" "$Tests\test_events\test_events.vbp" @("Events test PASSED")
    Test-Vbp "M7Test" "$Tests\m7_test\M7Test.vbp" @("4/4 PASSED")
    
    # --- P24 COM优化专项测试 ---
    Write-Host "--- P24 COM Optimization Tests ---" -ForegroundColor Yellow
    
    Test-Run "test_p24" "$Tests\test_p24.bas" @("P24-01a:OK", "P24-01b:OK", "P24-01c:OK", "P24-03a:OK", "P24-03b:OK", "P24:5/5")
    # test_vbman 依赖外部 COM 组件 VBMANLIB (x86 DLL, 需 32 位注册)
    Test-Vbp "test_vbman" "$Tests\test_vbman\test_vbman.vbp" @("P24-04a:OK", "P24-04b:OK", "P24-04:2/2") -Arch "x86" -RequiresCom "VBMANLIB.cVBMAN"
    Test-Run "test_earlybound2" "$Tests\test_earlybound2.bas" @("EB2-1:OK", "EB2-7:DriveType=2", "EB2-8:OK", "EB2-10:OK", "EB2:10/10") -Arch "x86"
    Test-Run "test_not_com" "$Tests\test_not_com.bas" @("NOT-COM:OK", "NOT-COM2:OK", "NOT-COM:PASS") -Arch "x86"
    Test-Run "test_err_obj" "$Tests\test_err_obj.bas" @("ERR-1:OK", "ERR-6:OK", "ERR:6/6")
    Test-Run "test_variant_cmp" "$Tests\test_variant_cmp.bas" @("VC-1:OK", "VC-4:OK", "VC:4/4")
    Test-Run "test_com_default_prop" "$Tests\test_com_default_prop.bas" @("DP-1:OK", "DP-4:OK", "P24-10: 4/4")
    Test-Run "test_com_optional" "$Tests\test_com_optional.bas" @("OP-1:OK", "OP-4:OK", "P24-11: 4/4")
    Write-Host ""
}

if ($Category -in @("all", "compile")) {
    # --- 综合编译测试 (能编译但不一定有Main) ---
    Write-Host "--- Compile Tests ---" -ForegroundColor Yellow
    
    Test-Compile "test_comprehensive" "$Tests\test_comprehensive.bas"
    Test-Compile "test_comprehensive2" "$Tests\test_comprehensive2.bas"
    Write-Host ""
    
    # --- P7 窗体编译测试 (GUI程序只验证编译通过) ---
    Write-Host "--- Form Compile Tests (P7) ---" -ForegroundColor Yellow
    
    $formTests = @(
        "test_form\empty_form.frm",
        "test_form\form_test_p74.frm",
        "form_test_p75.frm",
        "form_test_p76.frm",
        "form_test_p78.frm",
        "form_test_p79.frm",
        "form_test_m8.frm",
        "form_mdi_parent.frm"
    )
    
    foreach ($t in $formTests) {
        $path = Join-Path $Tests $t
        if (Test-Path $path) {
            $name = [System.IO.Path]::GetFileNameWithoutExtension($t)
            Test-Compile $name $path
        }
    }
    Write-Host ""
}

if ($Category -in @("all", "syntax")) {
    # --- 语法/语义测试 ---
    Write-Host "--- Syntax/Semantic Tests ---" -ForegroundColor Yellow
    
    $syntaxTests = @(
        "test_basic.bas",
        "test_for.bas", "test_for2.bas",
        "test_goto.bas", "test_gosub.bas",
        "test_select.bas",
        "test_onerror.bas",
        "test_redim.bas",
        "test_setlet.bas", "test_setonly.bas",
        "test_assign.bas",
        "test_sem_minimal.bas", "test_sem_proc.bas", "test_semantic.bas"
    )
    
    foreach ($t in $syntaxTests) {
        $path = Join-Path $Tests $t
        if (Test-Path $path) {
            $name = [System.IO.Path]::GetFileNameWithoutExtension($t)
            Test-Syntax $name $path
        }
    }
    Write-Host ""
    
    # --- 预处理器测试 ---
    Write-Host "--- Preprocessor Tests ---" -ForegroundColor Yellow
    
    $ppTests = @(
        "test_preprocess.bas",
        "test_pp_minimal.bas",
        "test_pp2.bas", "test_pp3.bas", "test_pp4.bas", "test_pp5.bas",
        "test_pp6.bas", "test_pp7.bas", "test_pp8.bas", "test_pp9.bas",
        "test_pp10.bas", "test_pp11.bas", "test_pp12.bas", "test_pp13.bas",
        "test_pp14.bas", "test_pp15.bas", "test_pp16.bas", "test_pp17.bas",
        "test_pp18.bas"
    )
    
    foreach ($t in $ppTests) {
        $path = Join-Path $Tests $t
        if (Test-Path $path) {
            $name = [System.IO.Path]::GetFileNameWithoutExtension($t)
            Test-Syntax $name $path
        }
    }
    Write-Host ""
}

# =============================================
# 汇总
# =============================================
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Results: PASS=$script:pass FAIL=$script:fail SKIP=$script:skip TOTAL=$script:total" -ForegroundColor $(if ($script:fail -gt 0) { "Red" } else { "Green" })
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

if ($script:fail -gt 0) { exit 1 } else { exit 0 }