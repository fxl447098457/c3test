# c3 ���������ɲ��Կ��
# �÷�: .\run_tests.ps1 [-Category <all|compile|run|syntax>] [-Verbose]
#
# ���Է���:
#   smoke   - ð�̲��� (����+����+���У��, ������֤ .build\C3.exe ����)
#   compile - ������� (c3 .bas -> .exe, ������)
#   run     - ���в��� (����+����+У�����)
#   syntax  - �﷨���� (--syntax-only, �����ɴ���)
#   all     - ȫ�� (Ĭ��)

param(
    [string]$Category = "all",
    [switch]$Verbose,
    [string]$OutputDirectory = ""
)

$ErrorActionPreference = "SilentlyContinue"

# === ���� ===
# ·��ȫ���ɽű�����λ���Ƶ�, ����Ӳ����ֿ����·�� (��ֵ D:\vb6pro �Ѳ�����)
$Root = Split-Path -Parent $PSScriptRoot
$C3 = Join-Path $Root ".build\C3.exe"
$Tests = $PSScriptRoot
$OutDir = if ($OutputDirectory) { $OutputDirectory } else { Join-Path $Root "output" }
# vcvarsall ·��: �������� C3_VCVARSALL ����, δ����ʱ vswhere �Զ�̽��
# (���� Community/Professional/Enterprise/BuildTools ��ʵ���� CI ����). ��� scripts\README.md
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
    Write-Host "[ERROR] δ�ҵ� vcvarsall.bat" -ForegroundColor Red
    Write-Host "        �밲װ VS2022 ����ѡ [ʹ�� C++ �����濪��] ��������," -ForegroundColor Red
    Write-Host "        �����û������� C3_VCVARSALL ָ��������·�� (��� scripts\README.md)" -ForegroundColor Red
    exit 1
}

# === ����MSVC���� ===
$msvcOutput = cmd /c "call `"$VcVars`" x64 >nul 2>&1 && echo MSVC_OK" 2>&1
if ($msvcOutput -notcontains "MSVC_OK") {
    Write-Host "[ERROR] �޷���ʼ��MSVC����" -ForegroundColor Red
    exit 1
}

# ����MSVC�������� (ͨ����ʱbat����)
$tempBat = "$env:TEMP\vcvars_env.bat"
cmd /c "call `"$VcVars`" x64 >nul 2>&1 && set" | Out-File $tempBat -Encoding ASCII
Get-Content $tempBat | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') {
        [Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
    }
}
Remove-Item $tempBat -ErrorAction SilentlyContinue

if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir | Out-Null }

# === ���Լ��� ===
$script:pass = 0
$script:fail = 0
$script:skip = 0
$script:total = 0

# === �ⲿ COM ������� ===
# ���ֲ�������������ע����ⲿ COM ��� (�� VBMANLIB)�����δע��ʱ���б�Ȼʧ��
# (VB6 �����ڴ��� 429), �����ǻ���ȱʧ, ���Ǳ�����ȱ��, Ӧ SKIP ���� FAIL��
# 32 λ���� (Arch=x86) ��ע����ض���Ӱ��, ֻ�ܿ��� WOW6432Node ��ͼ��
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

# === ������� ===
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

# === ���б�������� exe (ͳһ����) ===
# ���� @{ Ok; ExitCode; Output; Detail }
#
# ΪʲôҪ���������: �����ⲿ����������ڲ�ͬ�����Ự�¿ɿ��Բ�ͬ��
# Start-Process �� -RedirectStandardOutput �������������ӽ��̼̳п���̨�ض���,
# ����Ự������쳣 (��ʱ .out/.err �� 0 �ֽ�, ԭʵ��ֻ�� "FAIL (run error)",
# �ѻ�������αװ�ɲ���ʧ�� ���� ������)�����ﰴ����·�����γ���:
#   ·�� A: .NET Process + �ܵ����� (ֻ���� CreateProcess, �����������ض���)
#   ·�� B: Start-Process -Redirect* (����ԭ��Ϊ��Ϊ��)
# ������ʧ�ܲ�����ʧ��, �����쳣ԭ��һ�����, ����"ʧ�ܵ���˵Ϊʲô"��
function Invoke-TestExe {
    param(
        [string]$ExePath,
        [string]$WorkDir,
        [string]$Name
    )

    # ����Ŀ¼ͳһ��Ϊ output\: ���ֲ����� Open ... For Output д���·���ļ�
    # (scores.txt / test_output.txt / *.dat ��), ��ָ���ͻ�����ֿ��Ŀ¼��
    $stdoutFile = Join-Path $WorkDir "$Name.out"
    $stderrFile = Join-Path $WorkDir "$Name.err"
    Remove-Item $stdoutFile, $stderrFile -ErrorAction SilentlyContinue

    $errors = @()

    # --- ·�� A: .NET Process + �ܵ� ---
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
            return @{
                Ok       = $false
                ExitCode = $null
                Output   = @()
                Detail   = "run timeout: 5s"
            }
        }
        $stdout = $soTask.Result
        $stderr = $seTask.Result

        # ���̱�������, ���˹����� (������ϵͳ ANSI һ��, �������������)
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

    # --- ·�� B: Start-Process (ԭʵ��, ��) ---
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

# GUI smoke: require a visible main window, then close only the process we launch.
# This checks startup, not screenshot correctness or QR decoding.
function Test-GuiVbp {
    param([string]$Name, [string]$VbpFile, [string]$ExeName = "", [string]$Arch = "", [int]$AutoExitSec = 0)
    $script:total++
    Write-Host -NoNewline "  [GUI] $Name ... "
    $guiOut = Join-Path $OutDir $Name
    New-Item -ItemType Directory -Path $guiOut -Force | Out-Null
    if ($Arch) {
        $compileResult = & $C3 $VbpFile --arch $Arch --output-dir $guiOut 2>&1
    } else {
        $compileResult = & $C3 $VbpFile --output-dir $guiOut 2>&1
    }
    if ($LASTEXITCODE -ne 0) {
        $script:fail++
        Write-Host "FAIL (compile)" -ForegroundColor Red
        if ($Verbose) { Write-Host ($compileResult | Out-String) }
        return
    }
    # VBP ExeName32 may differ from the vbp file name; pass -ExeName to override.
    $exeBase = if ($ExeName) { $ExeName } else { [IO.Path]::GetFileNameWithoutExtension($VbpFile) }
    $exe = Join-Path $guiOut ($exeBase + ".exe")
    $proc = $null
    try {
        $proc = Start-Process -FilePath $exe -WorkingDirectory $guiOut -PassThru -ErrorAction Stop
        $watch = [Diagnostics.Stopwatch]::StartNew()
        $windowSeen = $false
        while ($watch.ElapsedMilliseconds -lt 5000) {
            $proc.Refresh()
            if ($proc.HasExited) { break }
            if ($proc.MainWindowHandle -ne [IntPtr]::Zero) { $windowSeen = $true; break }
            Start-Sleep -Milliseconds 100
        }
        if (-not $windowSeen) { throw "Main window not available within 5s" }
        if ($AutoExitSec -gt 0) {
            # GUI demo with no clean-exit contract: window shown is enough;
            # auto-kill after the timeout so the suite never hangs.
            $watch = [Diagnostics.Stopwatch]::StartNew()
            while ($watch.ElapsedMilliseconds -lt $AutoExitSec * 1000) {
                $proc.Refresh()
                if ($proc.HasExited) { break }
                Start-Sleep -Milliseconds 100
            }
            if (-not $proc.HasExited) { $proc.Kill(); $proc.WaitForExit(5000) | Out-Null }
            $script:pass++
            Write-Host "PASS (compile, window, auto-exit after ${AutoExitSec}s)" -ForegroundColor Green
        } else {
            if (-not $proc.CloseMainWindow()) { throw "Main window refused close" }
            if (-not $proc.WaitForExit(2000)) { throw "Application did not exit after close" }
            $proc.Refresh()
            if ($proc.ExitCode -ne 0) { throw "Exit code $($proc.ExitCode)" }
            $script:pass++
            Write-Host "PASS (compile, window, clean exit)" -ForegroundColor Green
        }
    } catch {
        $script:fail++
        Write-Host "FAIL ($($_.Exception.Message))" -ForegroundColor Red
    } finally {
        if ($proc -and -not $proc.HasExited) { $proc.Kill(); $proc.WaitForExit() }
    }
}

# === ���в��� (����+����+���У��) ===
function Test-Run {
    param(
        [string]$Name, 
        [string]$Source,
        [string[]]$ExpectedOutputs,  # Ԥ�������
        [string]$Arch = ""            # ��ѡ�ܹ����� (x86/x64)
    )
    $script:total++
    Write-Host -NoNewline "  [RUN] $Name ... "
    
    # ����
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
    
    # ȷ��exe·��
    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($Source)
    $exePath = Join-Path $OutDir "$baseName.exe"
    if (-not (Test-Path $exePath)) {
        $script:fail++
        Write-Host "FAIL (no exe)" -ForegroundColor Red
        return
    }
    
    # ���� (�ȴ�������)
    $run = Invoke-TestExe -ExePath $exePath -WorkDir $OutDir -Name $baseName
    if (-not $run.Ok) {
        $script:fail++
        Write-Host "FAIL (run error)" -ForegroundColor Red
        Write-Host ("    " + $run.Detail) -ForegroundColor Red
        return
    }
    $runOutput = $run.Output
    
    # У�����
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
        # ��Ԥ�������ֻҪ����������ͨ��
        $script:pass++
        Write-Host "PASS" -ForegroundColor Green
    }
}

# === VBP���̲��� (����+����+���У��) ===
function Test-Vbp {
    param(
        [string]$Name,
        [string]$VbpFile,
        [string[]]$ExpectedOutputs,
        [string]$Arch = "",           # ��ѡ�ܹ����� (x86/x64)
        [string]$RequiresCom = ""     # �������ⲿ COM ProgId (δע���� SKIP, ���� FAIL)
    )
    $script:total++
    Write-Host -NoNewline "  [VBP] $Name ... "

    # �ⲿ COM ����ȱʧ �� SKIP (����ȱʧ, �Ǳ�����ȱ��)
    if ($RequiresCom -and -not (Test-ComRegistered $RequiresCom $Arch)) {
        $script:skip++
        $view = if ($Arch -eq "x86") { "WOW6432Node (32-bit)" } else { "64-bit" }
        Write-Host "SKIP (COM '$RequiresCom' δע���� $view ��ͼ)" -ForegroundColor Yellow
        return
    }

    # ����VBP����
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

    # ��VBP�ļ����Ƶ�exe·��
    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($VbpFile)
    $exePath = Join-Path $OutDir "$baseName.exe"
    if (-not (Test-Path $exePath)) {
        $script:fail++
        Write-Host "FAIL (no exe)" -ForegroundColor Red
        return
    }

    # ���� (�ȴ�������)
    $run = Invoke-TestExe -ExePath $exePath -WorkDir $OutDir -Name $baseName
    if (-not $run.Ok) {
        $script:fail++
        Write-Host "FAIL (run error)" -ForegroundColor Red
        Write-Host ("    " + $run.Detail) -ForegroundColor Red
        return
    }
    $runOutput = $run.Output

    # У�����
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

# === �﷨���� ===
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
# ���в���
# =============================================

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  C3 Compiler Test Suite" -ForegroundColor Cyan
Write-Host "  $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# --- ð�̲��� ---
# �ؽ� C3.exe ���������: ֻ��֤ "���� -> ����C -> cl/link -> ����" ȫ��·��
# ������ tests\smoke.bas (�ļ�ͷд��ά��Լ��: ��ֹ MsgBox ���������)��
if ($Category -in @("all", "smoke")) {
    Write-Host "--- Smoke Test (C3.exe end-to-end) ---" -ForegroundColor Yellow

    Test-Run "smoke" "$Tests\smoke.bas" @("SMOKE-1:OK", "SMOKE-2:OK", "SMOKE-3:OK", "SMOKE PASS")
    Write-Host ""
}

# --- �ع���� (����ʼ��ͨ��) ---
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
    
    # --- P5.5 �����Բ��� ---
    Write-Host "--- Compat Tests (P5.5) ---" -ForegroundColor Yellow
    
    Test-Run "test_compat" "$Tests\test_compat.bas"
    Test-Run "test_types" "$Tests\test_types.bas"
    Test-Run "test_control" "$Tests\test_control.bas"
    Test-Run "test_declare" "$Tests\test_declare.bas"
    Write-Host ""
    
    # --- P5.7 ��֪�����޸����� ---
    Write-Host "--- Bugfix Tests (P5.7) ---" -ForegroundColor Yellow
    
    Test-Run "test_fixes" "$Tests\test_fixes.bas" @("FIX1:OK", "FIX2:OK", "FIX3:OK", "All fixes passed!")
    Write-Host ""
    
    # --- VBP���̲��� (P5) ---
    Write-Host "--- VBP Project Tests (P5) ---" -ForegroundColor Yellow
    
    Test-Vbp "test_class" "$Tests\test_class.vbp" @("3", "0")

    Test-Vbp "M6Test" "$Tests\M6Test.vbp" @("M6A:OK", "M6B:OK", "M6C:OK", "M6D:OK", "M6 PASSED")
    Test-Vbp "modulemethod" "$Tests\test_modulemethod.vbp" @("30", "21")

    # QR code project (tests\VbQRCodegen-master): form loads, sets Image1.Picture via Stretch
    Test-GuiVbp "VbQRCodegen" "$Tests\VbQRCodegen-master\test\Project1.vbp"
    # BalloonTooltips: form loads with controls + creates its common-controls tooltip windows (x64).
    Test-GuiVbp "BalloonTooltips" "$Tests\BalloonTooltips\prjBalloonTooltips.vbp" -ExeName "BalloonTooltips"
    # Charts 2020 demo (3rd-party UserControl charts): windowless chart controls (x86 first;
    # x64 after LongPtr port of API pointers/handles in the .ctl/.cls sources).
    Test-GuiVbp "Charts2020" "$Tests\Charts 2020\Proyecto1.vbp" -Arch "x86" -AutoExitSec 5
    Write-Host ""
    
    # --- P6 COM ���� ---
    Write-Host "--- COM Tests (P6) ---" -ForegroundColor Yellow
    
    Test-Run "test_com" "$Tests\test_com.bas" @("COM-1:OK", "COM-2:OK", "COM-3:OK", "COM:3/3")
    Test-Run "test_com2" "$Tests\test_com2.bas" @("Users")
    Test-Run "test_com3" "$Tests\test_com3.bas" @("COM3-1:OK", "COM3-2:OK", "COM3-3:OK", "COM3-4:OK", "COM3:4/4")
    Test-Run "test_earlybound" "$Tests\test_earlybound.bas" @("EB-1:OK", "EB-2:OK", "EB:2/2")
    Test-Run "test_p1324" "$Tests\test_p1324.bas" @("P13-1:OK", "P13-3:OK", "P13-5:OK", "P13:8/8")
    Test-Vbp "test_implements" "$Tests\test_implements.vbp" @("IMPL1:OK", "IMPL2:OK", "Implements test PASSED")
    Test-Vbp "test_events" "$Tests\test_events\test_events.vbp" @("Events test PASSED")
    Test-Vbp "M7Test" "$Tests\m7_test\M7Test.vbp" @("4/4 PASSED")
    
    # --- P24 COM�Ż�ר����� ---
    Write-Host "--- P24 COM Optimization Tests ---" -ForegroundColor Yellow
    
    Test-Run "test_p24" "$Tests\test_p24.bas" @("P24-01a:OK", "P24-01b:OK", "P24-01c:OK", "P24-03a:OK", "P24-03b:OK", "P24:5/5")
    # test_vbman �����ⲿ COM ��� VBMANLIB (x86 DLL, �� 32 λע��)
    Test-Vbp "test_vbman" "$Tests\test_vbman\test_vbman.vbp" @("P24-04a:OK", "P24-04b:OK", "P24-04:2/2") -Arch "x86" -RequiresCom "VBMANLIB.cVBMAN"
    Test-Run "test_earlybound2" "$Tests\test_earlybound2.bas" @("EB2-1:OK", "EB2-7:DriveType=2", "EB2-8:OK", "EB2-10:OK", "EB2:10/10") -Arch "x86"
    Test-Run "test_not_com" "$Tests\test_not_com.bas" @("NOT-COM:OK", "NOT-COM2:OK", "NOT-COM:PASS") -Arch "x86"
    Test-Run "test_err_obj" "$Tests\test_err_obj.bas" @("ERR-1:OK", "ERR-6:OK", "ERR:6/6")
    Test-Run "test_variant_cmp" "$Tests\test_variant_cmp.bas" @("VC-1:OK", "VC-4:OK", "VC:4/4")
    Test-Run "test_com_default_prop" "$Tests\test_com_default_prop.bas" @("DP-1:OK", "DP-4:OK", "P24-10: 4/4")
    Test-Run "test_com_optional" "$Tests\test_com_optional.bas" @("OP-1:OK", "OP-4:OK", "P24-11: 4/4")
    Test-Run "test_bstr_concat_scalar" "$Tests\test_bstr_concat_scalar.bas" @("BCS:16/16")
    Write-Host ""
}

if ($Category -in @("all", "compile")) {
    # --- �ۺϱ������ (�ܱ��뵫��һ����Main) ---
    Write-Host "--- Compile Tests ---" -ForegroundColor Yellow
    
    Test-Compile "test_comprehensive" "$Tests\test_comprehensive.bas"
    Test-Compile "test_comprehensive2" "$Tests\test_comprehensive2.bas"
    Write-Host ""
    
    # --- P7 ���������� (GUI����ֻ��֤����ͨ��) ---
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
    # --- �﷨/������� ---
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
    
    # --- Ԥ���������� ---
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
# ����
# =============================================
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Results: PASS=$script:pass FAIL=$script:fail SKIP=$script:skip TOTAL=$script:total" -ForegroundColor $(if ($script:fail -gt 0) { "Red" } else { "Green" })
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

if ($script:fail -gt 0) { exit 1 } else { exit 0 }