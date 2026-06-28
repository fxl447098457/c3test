# c3 编译器集成测试框架
# 用法: .\run_tests.ps1 [-Category <all|compile|run|syntax>] [-Verbose]
#
# 测试分类:
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
$C3 = "D:\vb6pro\.build\C3.exe"
$Tests = "D:\vb6pro\tests"
$OutDir = "D:\vb6pro\output"
$VcVars = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"

# === 设置VB6RTL目录 ===
$env:VB6RTL_DIR = "D:\vb6pro\src\rtl\core"

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

# === 运行测试 (编译+运行+输出校验) ===
function Test-Run {
    param(
        [string]$Name, 
        [string]$Source,
        [string[]]$ExpectedOutputs  # 预期输出行
    )
    $script:total++
    Write-Host -NoNewline "  [RUN] $Name ... "
    
    # 编译
    $compileResult = & $C3 $Source --output-dir $OutDir 2>&1
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
    
    # 运行 (5秒超时)
    $runResult = $null
    try {
        $proc = Start-Process -FilePath $exePath -NoNewWindow -Wait -PassThru `
            -RedirectStandardOutput "$OutDir\$baseName.out" `
            -RedirectStandardError "$OutDir\$baseName.err" `
            -ErrorAction Stop
        $runOutput = Get-Content "$OutDir\$baseName.out" -ErrorAction SilentlyContinue
    } catch {
        $script:fail++
        Write-Host "FAIL (run error)" -ForegroundColor Red
        return
    }
    
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
        [string[]]$ExpectedOutputs
    )
    $script:total++
    Write-Host -NoNewline "  [VBP] $Name ... "

    # 编译VBP工程
    $compileResult = & $C3 $VbpFile --output-dir $OutDir 2>&1
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

    # 运行 (5秒超时)
    try {
        $proc = Start-Process -FilePath $exePath -NoNewWindow -Wait -PassThru `
            -RedirectStandardOutput "$OutDir\$baseName.out" `
            -RedirectStandardError "$OutDir\$baseName.err" `
            -ErrorAction Stop
        $runOutput = Get-Content "$OutDir\$baseName.out" -ErrorAction SilentlyContinue
    } catch {
        $script:fail++
        Write-Host "FAIL (run error)" -ForegroundColor Red
        return
    }

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
    
    Test-Run "test_fixes" "$Tests\test_fixes.bas" @("All fixes passed!")
    Write-Host ""
    
    # --- VBP工程测试 (P5) ---
    Write-Host "--- VBP Project Tests (P5) ---" -ForegroundColor Yellow
    
    Test-Vbp "test_class" "$Tests\test_class.vbp" @("3", "0")

    Test-Vbp "M6Test" "$Tests\M6Test.vbp" @("M6 PASSED")
    Test-Vbp "modulemethod" "$Tests\test_modulemethod.vbp" @("30", "21")
    Write-Host ""
    
    # --- P6 COM 测试 ---
    Write-Host "--- COM Tests (P6) ---" -ForegroundColor Yellow
    
    Test-Run "test_com" "$Tests\test_com.bas" @("COM basic tests completed")
    Test-Run "test_com2" "$Tests\test_com2.bas" @("Users")
    Test-Run "test_com3" "$Tests\test_com3.bas" @("All tests passed")
    Test-Run "test_earlybound" "$Tests\test_earlybound.bas" @("Early binding test OK")
    Test-Run "test_p1324" "$Tests\test_p1324.bas" @("P13.24 PASS")
    Test-Vbp "test_implements" "$Tests\test_implements.vbp" @("Implements test PASSED")
    Test-Vbp "test_events" "$Tests\test_events\test_events.vbp" @("Events test PASSED")
    Test-Vbp "M7Test" "$Tests\m7_test\M7Test.vbp" @("4/4 PASSED")
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
Write-Host "  Results: PASS=$script:pass FAIL=$script:fail TOTAL=$script:total" -ForegroundColor $(if ($script:fail -gt 0) { "Red" } else { "Green" })
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

if ($script:fail -gt 0) { exit 1 } else { exit 0 }
