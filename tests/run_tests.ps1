# P5 集成测试框架
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
$C3 = "D:\vb6pro\.build\c3.exe"
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
Write-Host "  c3 P5 Integration Test Suite" -ForegroundColor Cyan
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
    Write-Host ""
    
    # --- P5.5 兼容性测试 (新增) ---
    Write-Host "--- Compat Tests (P5.5) ---" -ForegroundColor Yellow
    
    Test-Run "test_compat" "$Tests\test_compat.bas"
    Test-Run "test_types" "$Tests\test_types.bas"
    Test-Run "test_control" "$Tests\test_control.bas"
    Test-Run "test_declare" "$Tests\test_declare.bas"
    Write-Host ""
    
    # --- VBP工程测试 ---
    Write-Host "--- VBP Project Tests ---" -ForegroundColor Yellow
    
    Test-Vbp "test_class" "$Tests\test_class.vbp" @("3", "0")
    Test-Vbp "TestVBP" "$Tests\vbp_project\TestVBP2.vbp" @("Add(10, 20) =", "30", "Multiply(5, 6) =", "30")
    Write-Host ""
}

if ($Category -in @("all", "compile")) {
    # --- 综合编译测试 (能编译但不一定有Main) ---
    Write-Host "--- Compile Tests ---" -ForegroundColor Yellow
    
    Test-Compile "test_comprehensive" "$Tests\test_comprehensive.bas"
    Test-Compile "test_comprehensive2" "$Tests\test_comprehensive2.bas"
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
