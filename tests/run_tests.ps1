# c3 编译器自动化测试框架
# 用途: .\run_tests.ps1 [-Category <all|compile|run|syntax>] [-Verbose]
#
# 参数说明:
#   smoke   - 冒烟测试 (编译+链接+运行; 主要验证 .build\C3.exe 能被正常生成并运行)
#   compile - 编译测试 (c3 .bas -> .exe, 只编译不运行)
#   run     - 运行测试 (编译+链接后运行, 校验输出是否符合预期)
#   syntax  - 语法测试 (仅 --syntax-only, 不生成可执行文件)
#   all     - 全部测试 (编译+链接+运行所有分类)

param(
    [string]$Category = "all",
    [switch]$Verbose,
    [string]$OutputDirectory = "",
    [int]$Jobs = 1,          # >1 时并行运行纯 .bas 用例 (每 worker 独立输出目录); GUI/VBP 始终串行
    [int]$BasShard = 0,      # bas 分片当前编号 (1..BasShardTotal); 0=不分片整队跑
    [int]$BasShardTotal = 1  # bas 分片总数 (供 CI 用多 runner 并行跑 bas 用例)
)

$ErrorActionPreference = "SilentlyContinue"

# 输出编码说明: PS5.1 终端按 Windows 控制台代码页解释输出; 本脚本统一以 UTF-8 写入
# 兼容 VS2022 的 Community/Professional/Enterprise/BuildTools 任一版本 (供 CI 使用)。详见 scripts\README.md
$Root = Split-Path -Parent $PSScriptRoot
$C3 = Join-Path $Root ".build\C3.exe"
$Tests = $PSScriptRoot
# T0 拆分 (2026-09-20): 语法/冒烟用例 git mv 至 tests_github\t0_cases\, 跨目录引用同一份文件 (无副本)
$GHTests = Join-Path $Tests "..\tests_github\t0_cases"
$OutDir = if ($OutputDirectory) { $OutputDirectory } else { Join-Path $Root "output" }
# === 获取 MSVC 编译环境 (通用) ===
# 设计原则: 不依赖 cmd.exe / vcvarsall 解析 (易因安全策略/编码/PATH 大小写失效),
# 不硬编码 VS 版本 (2019/2022/2026 均可) 与 Windows SDK 版本号 (动态发现)。
# 优先用环境变量 C3_VCVARSALL 指向的 vcvarsall.bat 推导 VS 根; 否则用 vswhere 取最新已装 VS。
function Get-MsvcToolset {
    # 返回 @{ VsRoot; ToolVer; SdkVer; BinX64; BinX86; Include; LibX64; LibX86 }
    # 1) C3_VCVARSALL -> 上溯 4 级 (...\<Edition>\VC\Auxiliary\Build\vcvarsall.bat) 得 VS 根目录
    $vsRoot = $null
    if ($env:C3_VCVARSALL -and (Test-Path $env:C3_VCVARSALL)) {
        $p = $env:C3_VCVARSALL
        for ($i = 0; $i -lt 4; $i++) { $p = Split-Path -Parent $p }
        $vsRoot = $p
    }
    # 2) 否则 vswhere 探测已安装的最新 VS (Community/Pro/Enterprise/BuildTools 任一版本)
    if (-not $vsRoot) {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
        if (Test-Path $vswhere) {
            $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
            if ($vsPath) { $vsRoot = $vsPath }
        }
    }
    if (-not $vsRoot -or -not (Test-Path $vsRoot)) {
        Write-Host "[ERROR] 未找到 Visual Studio (含 VC.Tools 工作负载)" -ForegroundColor Red
        Write-Host "        请安装任意版本 VS 并勾选 [使用 C++ 的桌面开发], 或设置环境变量 C3_VCVARSALL 指向 vcvarsall.bat" -ForegroundColor Red
        exit 1
    }
    # 3) 动态发现 MSVC toolset 版本 (VC\Tools\MSVC\<ver>) -- 不硬编码
    $msvcRoot = Join-Path $vsRoot "VC\Tools\MSVC"
    $toolVer = Get-ChildItem $msvcRoot -Directory -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending | Select-Object -First 1 -ExpandProperty Name
    if (-not $toolVer) { Write-Host "[ERROR] 未找到 MSVC toolset ($msvcRoot)" -ForegroundColor Red; exit 1 }
    # 4) 动态发现 Windows SDK 根 (Windows Kits\10) -- 不硬编码盘符/版本
    #    优先读注册表 KitsRoot10 (vcvarsall 自身定位方式), 其次标准 Program Files 位置,
    #    再回退扫描常见盘符。
    $kitRoot = $null
    foreach ($base in @(${env:ProgramFiles(x86)}, $env:ProgramFiles)) {
        if ($base -and (Test-Path (Join-Path $base "Windows Kits\10\Include"))) {
            $kitRoot = Join-Path $base "Windows Kits\10"; break
        }
    }
    if (-not $kitRoot) {
        foreach ($rp in @("HKLM:\SOFTWARE\Microsoft\Windows Kits\Installed Roots",
                          "HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows Kits\Installed Roots")) {
            if (Test-Path $rp) {
                $kr = (Get-ItemProperty -Path $rp -ErrorAction SilentlyContinue).KitsRoot10
                if ($kr) {
                    $kr = $kr.TrimEnd('\')
                    if (Test-Path (Join-Path $kr "Include")) { $kitRoot = $kr; break }
                }
            }
        }
    }
    if (-not $kitRoot) {
        foreach ($d in @("D:","E:","F:")) {
            $cand = Join-Path $d "Windows Kits\10"
            if (Test-Path (Join-Path $cand "Include")) { $kitRoot = $cand; break }
        }
    }
    if (-not $kitRoot) { Write-Host "[ERROR] 未找到 Windows SDK (Windows Kits\10\Include)" -ForegroundColor Red; exit 1 }
    $sdkVer = Get-ChildItem (Join-Path $kitRoot "Include") -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '^\d+\.\d+\.\d+\.\d+$' } |
        Sort-Object Name -Descending | Select-Object -First 1 -ExpandProperty Name
    if (-not $sdkVer) { Write-Host "[ERROR] 未找到 Windows SDK Include 版本 ($kitRoot\Include)" -ForegroundColor Red; exit 1 }

    $tool   = Join-Path $msvcRoot $toolVer
    $binX64 = Join-Path $tool "bin\Hostx64\x64"
    $binX86 = Join-Path $tool "bin\Hostx64\x86"
    $incDir = Join-Path $kitRoot "Include"
    $libDir = Join-Path $kitRoot "Lib"
    $Include = "$tool\include;$(Join-Path $incDir $sdkVer um);$(Join-Path $incDir $sdkVer ucrt);$(Join-Path $incDir $sdkVer shared);$(Join-Path $incDir $sdkVer winrt);$(Join-Path $incDir $sdkVer cppwinrt)"
    $LibX64  = "$(Join-Path $tool lib x64);$(Join-Path $libDir $sdkVer um x64);$(Join-Path $libDir $sdkVer ucrt x64)"
    $LibX86  = "$(Join-Path $tool lib x86);$(Join-Path $libDir $sdkVer um x86);$(Join-Path $libDir $sdkVer ucrt x86)"
    return [pscustomobject]@{ VsRoot=$vsRoot; ToolVer=$toolVer; SdkVer=$sdkVer;
        BinX64=$binX64; BinX86=$binX86; Include=$Include; LibX64=$LibX64; LibX86=$LibX86 }
}

$msvc = Get-MsvcToolset
# 原生 $env: 赋值: 子进程(含 C3 启动的 cl/link)可靠继承; 同时含 x64 与 x86 交叉工具链
$env:PATH    = "$($msvc.BinX64);$($msvc.BinX86);$env:PATH"
$env:INCLUDE = $msvc.Include
$env:LIB     = "$($msvc.LibX64);$($msvc.LibX86)"
$clCmd = Get-Command cl.exe -ErrorAction SilentlyContinue
if (-not $clCmd) { Write-Host "[ERROR] 设置 MSVC 环境后仍找不到 cl.exe (PATH 前段=$($msvc.BinX64))" -ForegroundColor Red; exit 1 }
Write-Host ("  MSVC 环境: VS=$($msvc.VsRoot)  toolset=$($msvc.ToolVer)  SDK=$($msvc.SdkVer)") -ForegroundColor Gray
Write-Host ("  cl.exe: $($clCmd.Source)") -ForegroundColor Gray

if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir | Out-Null }

# === 测试基础函数 ===
$script:pass = 0
$script:fail = 0
$script:skip = 0
$script:total = 0

# === COM 测试前: 检查相关 COM 组件是否已注册 ===
# 仅当所需的 COM 组件已注册时, 才执行对应的 COM 测试 (例如 VBMANLIB)
# 若所需 COM 组件缺失: 相关用例 SKIP 而非 FAIL
# 32 位程序 (Arch=x86) 需读取 32 位注册表视图 (WOW6432Node), 本脚本统一处理
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

# === 冒烟测试 (编译+链接+运行) ===
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

# === 编译测试 (能生成 exe 但没有 Main, 仅验证编译通过) ===
# 返回 @{ Ok; ExitCode; Output; Detail } 供调用方判定编译/运行结果
#
# 输出日志说明: 不同终端编码下, 输出内容可能略有差异,
# 使用方法: 参考 codes\smoke.bas 中带主入口的示例
#   start ".\build\C3.exe" 由调用方传入, 这里统一封装运行逻辑
#   启动并等待, 捕获标准输出/错误, 汇总为 @{ Ok; ExitCode; Output; Detail }
#   方案 A: .NET Process + Diagnostic (ReadToEndAsync, 不阻塞)
#   方案 B: Start-Process -RedirectStandardOutput (简单但不支持实时读取)
# 若启用了 VCVARSALL, 会自动按它配置编译环境, 生成的目标写于 "output\" 目录
function Invoke-TestExe {
    param(
        [string]$ExePath,
        [string]$WorkDir,
        [string]$Name
    )

    # 日志统一写入 output 目录: 使用 Open ... For Output 追加写入同一日志文件
    # (scores.txt / test_output.txt / *.dat 等), 不同进程写入不同实时文件
    $stdoutFile = Join-Path $WorkDir "$Name.out"
    $stderrFile = Join-Path $WorkDir "$Name.err"
    Remove-Item $stdoutFile, $stderrFile -ErrorAction SilentlyContinue

    $errors = @()

    # --- 方案 A: .NET Process + 重定向 (实时读取, 推荐) ---
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

        # 输出可能包含 ANSI/UTF-8 混合编码, 按标准输出逐行读取以降低乱码 (当前按 ANSI 处理)
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

    # --- 方案 B: Start-Process -RedirectStandard* (简单, 但编码不可控) ---
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
    # 自动把工程目录下的原生依赖 (OCX/DLL/TLB) 复制到 exe 所在目录 (与产物同目录),
    # 以支持免注册便携部署: 如第三方 OCX 控件无需本机注册即可 LoadLibrary 加载
    $srcDir = Split-Path $VbpFile -Parent
    if (Test-Path $srcDir) {
        Get-ChildItem $srcDir -File | Where-Object { $_.Extension -match '\.(ocx|dll|tlb)$' } | ForEach-Object {
            Copy-Item -Force $_.FullName $guiOut | Out-Null
        }
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
            # Fix 189: 但"它自己先退了"不是我们让它退的。旧代码在这里无条件
            # pass++ 且不读退出码, 于是运行期崩溃 (实测窗口出现后 18s 的
            # 0xC0000005) 也算 PASS —— 只有超时兜底那条路径才该免检。
            $watch = [Diagnostics.Stopwatch]::StartNew()
            while ($watch.ElapsedMilliseconds -lt $AutoExitSec * 1000) {
                $proc.Refresh()
                if ($proc.HasExited) { break }
                Start-Sleep -Milliseconds 100
            }
            $proc.Refresh()
            if ($proc.HasExited) {
                $selfSec = [int]($watch.ElapsedMilliseconds / 1000)
                $code = $proc.ExitCode
                if ($code -ne 0) {
                    throw ("exited on its own at ~{0}s with code 0x{1:X8}" -f $selfSec, $code)
                }
                Write-Host "PASS (compile, window, self-exit at ~${selfSec}s, code 0)" -ForegroundColor Green
            } else {
                $proc.Kill(); $proc.WaitForExit(5000) | Out-Null
                Write-Host "PASS (compile, window, auto-exit after ${AutoExitSec}s)" -ForegroundColor Green
            }
            $script:pass++
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

# === 编译冒烟测试 (编译+链接, 不运行生成物) ===
function Test-Run {
    param(
        [string]$Name, 
        [string]$Source,
        [string[]]$ExpectedOutputs,  # 预期输出 (可含多个子串, 逐一匹配)
        [string]$Arch = ""            # 可选架构参数 (x86/x64)
    )
    $script:total++
    Write-Host -NoNewline "  [RUN] $Name ... "
    
    # 编译: 输入源文件, 经中间C代码 -> cl/link -> 生成目标 (默认输出到 output 目录)
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
    
    # 验证编译产物 exe 是否存在: 若缺失则标记 FAIL (no exe) 并中断本次用例
    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($Source)
    $exePath = Join-Path $OutDir "$baseName.exe"
    if (-not (Test-Path $exePath)) {
        $script:fail++
        Write-Host "FAIL (no exe)" -ForegroundColor Red
        return
    }
    
    # 运行冒烟测试: 校验输出 (详见 smoke 用例; 若超时则按 SKIP 处理)
    $run = Invoke-TestExe -ExePath $exePath -WorkDir $OutDir -Name $baseName
    if (-not $run.Ok) {
        $script:fail++
        Write-Host "FAIL (run error)" -ForegroundColor Red
        Write-Host ("    " + $run.Detail) -ForegroundColor Red
        return
    }
    $runOutput = $run.Output
    
    # 编译通过 + 冒烟运行通过
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
        # 该测试用例预期会编译失败 (负向用例)
        $script:pass++
        Write-Host "PASS" -ForegroundColor Green
    }
}

# === 纯 .bas 用例串行执行 (默认路径; 与 Tests 逐个调用 Test-Run 等价) ===
function Invoke-BasSetSerial {
    param([object[]]$Items)
    foreach ($it in $Items) {
        Test-Run $it.Name $it.Source $it.Expected $it.Arch
    }
}

# === 纯 .bas 用例并行执行 (多个 C3 实例同时编译+运行) ===
# 每个 worker 独立输出目录 (避免 exe/日志文件互相覆盖); 仅限 PowerShell 7+
# (ForEach-Object -Parallel); 每个 worker 返回汇总对象, 由调用方合并计数.
function Invoke-BasSetParallel {
    param([object[]]$Items, [int]$Jobs)
    if ($Items.Count -eq 0) { return }

    # 均分 (按遍历顺序切片, 每片尽可能均匀)
    $per = [int][Math]::Ceiling($Items.Count / [double]$Jobs)
    $shards = @()
    for ($i = 0; $i -lt $Items.Count; $i += $per) {
        $end = [Math]::Min($i + $per - 1, $Items.Count - 1)
        $shards += ,@(@( $Items[$i..$end] ), (Join-Path $OutDir ("job" + $shards.Count)))
    }
    if ($shards.Count -eq 0) { return }

    $results = $shards | ForEach-Object -Parallel {
        $shardItems = $_[0]
        $workDir    = $_[1]
        New-Item -ItemType Directory -Path $workDir -Force | Out-Null
        $c3 = $using:C3
        $p = 0; $f = 0; $details = @()
        foreach ($it in $shardItems) {
            if ($it.Arch) {
                $cr = & $c3 $it.Source --arch $it.Arch --output-dir $workDir 2>&1
            } else {
                $cr = & $c3 $it.Source --output-dir $workDir 2>&1
            }
            $ec = $LASTEXITCODE
            if ($ec -ne 0) { $f++; $details += "$($it.Name): compile FAIL"; continue }

            $baseName = [IO.Path]::GetFileNameWithoutExtension($it.Source)
            $exePath = Join-Path $workDir "$baseName.exe"
            if (-not (Test-Path $exePath)) { $f++; $details += "$($it.Name): no exe"; continue }

            # --- 运行 (语义与 Invoke-TestExe 一致; 5s 超时) ---
            $stdoutFile = Join-Path $workDir "$($it.Name).out"
            $stderrFile = Join-Path $workDir "$($it.Name).err"
            $runOk = $false
            try {
                $psi = New-Object System.Diagnostics.ProcessStartInfo
                $psi.FileName = $exePath
                $psi.WorkingDirectory = $workDir
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
                    $f++; $details += "$($it.Name): run timeout 5s"; continue
                }
                [IO.File]::WriteAllText($stdoutFile, [string]$soTask.Result, [Text.Encoding]::Default)
                [IO.File]::WriteAllText($stderrFile, [string]$seTask.Result, [Text.Encoding]::Default)
                $runOk = $true
            } catch {
                $f++; $details += "$($it.Name): run error ($($_.Exception.Message))"; continue
            }

            if ($it.Expected -and $it.Expected.Count -gt 0 -and $runOk) {
                $runOut = @(Get-Content $stdoutFile -ErrorAction SilentlyContinue)
                $allMatch = $true
                foreach ($exp in $it.Expected) {
                    $found = $runOut | Where-Object { $_ -like "*$exp*" }
                    if (-not $found) { $allMatch = $false; break }
                }
                if ($allMatch) { $p++ } else { $f++; $details += "$($it.Name): output mismatch" }
            } else {
                $p++
            }
        }
        [pscustomobject]@{ Pass = $p; Fail = $f; Details = $details }
    } -ThrottleLimit $Jobs

    foreach ($r in $results) {
        $script:pass += $r.Pass
        $script:fail += $r.Fail
        $script:total += ($r.Pass + $r.Fail)
        foreach ($d in $r.Details) {
            Write-Host "  [RUN] $d" -ForegroundColor Red
        }
    }
    $sumPass = ($results | Measure-Object -Property Pass -Sum).Sum
    $sumFail = ($results | Measure-Object -Property Fail -Sum).Sum
    Write-Host "  (parallel: $($results.Count) worker(s), pass=$sumPass fail=$sumFail)"
}

# === VBP 工程测试 (编译+链接+运行) ===
function Test-Vbp {
    param(
        [string]$Name,
        [string]$VbpFile,
        [string[]]$ExpectedOutputs,
        [string]$Arch = "",           # 可选架构参数 (x86/x64)
        [string]$RequiresCom = ""     # 依赖的 COM ProgId (未注册则 SKIP, 否则 FAIL)
    )
    $script:total++
    Write-Host -NoNewline "  [VBP] $Name ... "

    # 若依赖的 COM 组件未注册 (即缺失): 标记 SKIP (不影响通过率, 不计 FAIL)
    if ($RequiresCom -and -not (Test-ComRegistered $RequiresCom $Arch)) {
        $script:skip++
        $view = if ($Arch -eq "x86") { "WOW6432Node (32-bit)" } else { "64-bit" }
        Write-Host "SKIP (COM '$RequiresCom' 未注册 $view 视图)" -ForegroundColor Yellow
        return
    }

    # 编译 VBP 工程: 输入 VBP 文件, 经 cl/link 生成可执行文件
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

    # 处理 VBP 编译输出: 校验是否包含 expected 输出 (可含多个子串)
    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($VbpFile)
    $exePath = Join-Path $OutDir "$baseName.exe"
    if (-not (Test-Path $exePath)) {
        $script:fail++
        Write-Host "FAIL (no exe)" -ForegroundColor Red
        return
    }

    # 编译失败则标记 FAIL (compile); 生成物缺失标记 FAIL (no exe)
    $run = Invoke-TestExe -ExePath $exePath -WorkDir $OutDir -Name $baseName
    if (-not $run.Ok) {
        $script:fail++
        Write-Host "FAIL (run error)" -ForegroundColor Red
        Write-Host ("    " + $run.Detail) -ForegroundColor Red
        return
    }
    $runOutput = $run.Output

    # 编译失败提示
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

# === 语法检查测试 ===
# Negative syntax case: --syntax-only must FAIL and report the given (ASCII) text.
# Used by the tB-extension contract diagnostics (ai/022 B01+).
# Multi-source --syntax-only probes (ai/022 B07): C3.exe accepts several positional files
# and driver_frontend picks the module kind per extension, so class-Inherits cases that need
# the base class in a *second* module are testable without a full vbp build.
function Invoke-SyntaxProj {
    param([array]$Sources)
    $argList = (($Sources | ForEach-Object { '"' + $_ + '"' }) -join ' ')
    $out = & cmd /c ('"' + $C3 + '" ' + $argList + ' --syntax-only 2>&1')
    $script:syntaxProjExit = $LASTEXITCODE
    return (($out | Out-String) -replace '\s+', ' ')
}

function Test-SyntaxFailMulti {
    param([string]$Name, [array]$Sources, [string]$Needle)
    $script:total++
    Write-Host -NoNewline "  [SYNTAX-FAIL] $Name ... "
    $text = Invoke-SyntaxProj $Sources
    if ($script:syntaxProjExit -ne 0 -and $text.Contains($Needle)) {
        $script:pass++
        Write-Host "PASS" -ForegroundColor Green
    } else {
        $script:fail++
        Write-Host "FAIL" -ForegroundColor Red
        Write-Host "  expected failing compile containing: $Needle" -ForegroundColor DarkGray
        if ($Verbose) { Write-Host $text }
    }
}

# ai/022 B11/C04: --syntax-only must SUCCEED and the merged output must carry every needle
# (and none of the -Absent ones). Folding legacy header attributes is a read-only information
# channel, so "what got folded" and "what deliberately did NOT" is asserted here, not by
# an exit code (D52).
function Test-SyntaxNote {
    param([string]$Name, [array]$Sources, [array]$Needles, [array]$Absent = @())
    $script:total++
    Write-Host -NoNewline "  [SYNTAX-NOTE] $Name ... "
    $text = Invoke-SyntaxProj $Sources
    $bad = @($Needles | Where-Object { -not $text.Contains($_) })
    $hit = @($Absent | Where-Object { $text.Contains($_) })
    if ($script:syntaxProjExit -eq 0 -and $bad.Count -eq 0 -and $hit.Count -eq 0) {
        $script:pass++
        Write-Host "PASS" -ForegroundColor Green
    } else {
        $script:fail++
        Write-Host "FAIL" -ForegroundColor Red
        Write-Host ("  exit=" + $script:syntaxProjExit + " missing: " + ($bad -join ' | ') +
                    " unexpected: " + ($hit -join ' | ')) -ForegroundColor DarkGray
        if ($Verbose) { Write-Host $text }
    }
}

function Test-SyntaxMulti {
    param([string]$Name, [array]$Sources)
    $script:total++
    Write-Host -NoNewline "  [SYNTAX] $Name ... "
    $text = Invoke-SyntaxProj $Sources
    if ($script:syntaxProjExit -eq 0) {
        $script:pass++
        Write-Host "PASS" -ForegroundColor Green
    } else {
        $script:fail++
        Write-Host "FAIL" -ForegroundColor Red
        if ($Verbose) { Write-Host $text }
    }
}

# Codegen-stage probes (ai/022 B08e-6): diagnostics raised while C is being emitted never
# reach --syntax-only (driver_compile.cpp returns before codegen), and a full build would
# pay for cl.exe + link. `--emit-c` runs the whole front end plus codegen and stops there.
function Invoke-CodegenProj {
    param([array]$Sources)
    $argList = (($Sources | ForEach-Object { '"' + $_ + '"' }) -join ' ')
    $out = & cmd /c ('"' + $C3 + '" ' + $argList + ' --emit-c 2>&1')
    $script:codegenProjExit = $LASTEXITCODE
    return (($out | Out-String) -replace '\s+', ' ')
}

function Test-CompileFail {
    param([string]$Name, [array]$Sources, [string]$Needle)
    $script:total++
    Write-Host -NoNewline "  [COMPILE-FAIL] $Name ... "
    $text = Invoke-CodegenProj $Sources
    if ($script:codegenProjExit -ne 0 -and $text.Contains($Needle)) {
        $script:pass++
        Write-Host "PASS" -ForegroundColor Green
    } else {
        $script:fail++
        Write-Host "FAIL" -ForegroundColor Red
        Write-Host "  expected failing codegen containing: $Needle" -ForegroundColor DarkGray
        if ($Verbose) { Write-Host $text }
    }
}

function Test-Compile {
    param([string]$Name, [array]$Sources)
    $script:total++
    Write-Host -NoNewline "  [COMPILE] $Name ... "
    $text = Invoke-CodegenProj $Sources
    if ($script:codegenProjExit -eq 0) {
        $script:pass++
        Write-Host "PASS" -ForegroundColor Green
    } else {
        $script:fail++
        Write-Host "FAIL" -ForegroundColor Red
        if ($Verbose) { Write-Host $text }
    }
}

function Test-SyntaxFail {
    param([string]$Name, [string]$Source, [string]$Needle)
    $script:total++
    Write-Host -NoNewline "  [SYNTAX-FAIL] $Name ... "
    # Native stderr under SilentlyContinue is dropped by `& 2>&1`; let cmd.exe merge the
    # streams, and collapse whitespace so long diagnostics cannot be word-wrapped apart.
    $result = & cmd /c ('"' + $C3 + '" "' + $Source + '" --syntax-only 2>&1')
    $text = (($result | Out-String) -replace '\s+', ' ')
    if ($LASTEXITCODE -ne 0 -and $text.Contains($Needle)) {
        $script:pass++
        Write-Host "PASS" -ForegroundColor Green
    } else {
        $script:fail++
        Write-Host "FAIL" -ForegroundColor Red
        Write-Host "  expected failing compile containing: $Needle" -ForegroundColor DarkGray
        if ($Verbose) { Write-Host $text }
    }
}

# ai/022 B11/C02: CoClass identity assertions. The resolver prints one info line per block on
# stderr -- note-level *diagnostics* cannot be the surface here, because Diagnostics::toString()
# is only dumped when a stage fails, so a note never reaches a successful compile (ai/022 D46).
function Invoke-IdentityText {
    param([string]$Proj)
    $out = & cmd /c ('"' + $C3 + '" "' + $Proj + '" --syntax-only 2>&1')
    $script:identityExit = $LASTEXITCODE
    return (((@($out | Where-Object { "$_" -match 'identity:' })) | Out-String) -replace '\s+', ' ')
}

function Test-IdentityNote {
    param([string]$Name, [string]$Proj, [array]$Needles)
    $script:total++
    Write-Host -NoNewline "  [IDENTITY] $Name ... "
    $text = Invoke-IdentityText $Proj
    $bad = @($Needles | Where-Object { -not $text.Contains($_) })
    if ($script:identityExit -eq 0 -and $bad.Count -eq 0) {
        $script:pass++
        Write-Host "PASS" -ForegroundColor Green
    } else {
        $script:fail++
        Write-Host "FAIL" -ForegroundColor Red
        Write-Host ("  exit=" + $script:identityExit + " missing: " + ($bad -join ' | ')) -ForegroundColor DarkGray
        if ($Verbose) { Write-Host $text }
    }
}

function Test-IdentityStable {
    param([string]$Name, [string]$Proj)
    $script:total++
    Write-Host -NoNewline "  [IDENTITY] $Name ... "
    $a = Invoke-IdentityText $Proj
    $b = Invoke-IdentityText $Proj
    if ($a.Length -gt 0 -and $a -ceq $b) {
        $script:pass++
        Write-Host "PASS" -ForegroundColor Green
    } else {
        $script:fail++
        Write-Host "FAIL" -ForegroundColor Red
        Write-Host "  two runs of the same project differ (or printed nothing)" -ForegroundColor DarkGray
    }
}

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

# ai/023 S01: vbp-level negative case. The package hard checks run in driver stage 0
# (before the pipeline), so --syntax-only is enough: compile must FAIL and the output
# must contain the given ASCII needle.
function Test-VbpFail {
    param([string]$Name, [string]$VbpFile, [string]$Needle)
    $script:total++
    Write-Host -NoNewline "  [VBP-FAIL] $Name ... "
    $result = & cmd /c ('"' + $C3 + '" "' + $VbpFile + '" --syntax-only 2>&1')
    $text = (($result | Out-String) -replace '\s+', ' ')
    if ($LASTEXITCODE -ne 0 -and $text.Contains($Needle)) {
        $script:pass++
        Write-Host "PASS" -ForegroundColor Green
    } else {
        $script:fail++
        Write-Host "FAIL" -ForegroundColor Red
        Write-Host "  expected failing compile containing: $Needle" -ForegroundColor DarkGray
        if ($Verbose) { Write-Host $text }
    }
}

# ai/023 S03: vbp-level negative case that only surfaces in the FULL pipeline
# (package export boundary fires at visit time / stage 3.5, not syntax-only).
function Test-VbpBuildFail {
    param([string]$Name, [string]$VbpFile, [string]$Needle)
    $script:total++
    Write-Host -NoNewline "  [VBP-BUILD-FAIL] $Name ... "
    $result = & cmd /c ('"' + $C3 + '" "' + $VbpFile + '" --output-dir "' + $OutDir + '" 2>&1')
    $text = (($result | Out-String) -replace '\s+', ' ')
    if ($LASTEXITCODE -ne 0 -and $text.Contains($Needle)) {
        $script:pass++
        Write-Host "PASS" -ForegroundColor Green
    } else {
        $script:fail++
        Write-Host "FAIL" -ForegroundColor Red
        Write-Host "  expected failing build containing: $Needle" -ForegroundColor DarkGray
        if ($Verbose) { Write-Host $text }
    }
}

# ai/023 S05: build must SUCCEED but emit the given warning text (D7: 校验不拒收,
# 警告必须可见 —— 成功路径吞警告的坑在 S01 已修, 本用例防回归).
function Test-VbpWarn {
    param([string]$Name, [string]$VbpFile, [string]$Needle)
    $script:total++
    Write-Host -NoNewline "  [VBP-WARN] $Name ... "
    $result = & cmd /c ('"' + $C3 + '" "' + $VbpFile + '" --output-dir "' + $OutDir + '" 2>&1')
    $text = (($result | Out-String) -replace '\s+', ' ')
    if ($LASTEXITCODE -eq 0 -and $text.Contains($Needle)) {
        $script:pass++
        Write-Host "PASS" -ForegroundColor Green
    } else {
        $script:fail++
        Write-Host "FAIL" -ForegroundColor Red
        Write-Host "  expected successful build containing: $Needle" -ForegroundColor DarkGray
        if ($Verbose) { Write-Host $text }
    }
}

# ai/023 S05/S06: generic CLI check — exit 0 and output contains needle.
function Test-CliOk {
    param([string]$Name, [string[]]$C3Args, [string]$Needle)
    $script:total++
    Write-Host -NoNewline "  [CLI] $Name ... "
    $result = & cmd /c (('"' + $C3 + '" ' + ($C3Args -join ' ') + ' 2>&1'))
    $text = (($result | Out-String) -replace '\s+', ' ')
    if ($LASTEXITCODE -eq 0 -and $text.Contains($Needle)) {
        $script:pass++
        Write-Host "PASS" -ForegroundColor Green
    } else {
        $script:fail++
        Write-Host "FAIL" -ForegroundColor Red
        Write-Host "  expected exit 0 containing: $Needle" -ForegroundColor DarkGray
        if ($Verbose) { Write-Host $text }
    }
}

# ai/023 S06: pack -> unpack -> every file byte-identical (acceptance: 逐文件 cmp).
function Test-PackRoundtrip {
    param([string]$Name, [string]$PkgDir)
    $script:total++
    Write-Host -NoNewline "  [PACK-RT] $Name ... "
    $tmp = Join-Path $OutDir ("packrt_" + [guid]::NewGuid().ToString("N").Substring(0,8))
    try {
        New-Item -ItemType Directory -Path $tmp | Out-Null
        Copy-Item -Recurse -Path "$PkgDir\*" -Destination $tmp
        & cmd /c (('"' + $C3 + '" --pack "' + $tmp + '" 2>&1')) | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "pack failed" }
        $pkgFile = Get-ChildItem -Path $tmp -Filter "*.c3pkg" | Select-Object -First 1
        if (-not $pkgFile) { throw "no .c3pkg produced" }
        $outDir = Join-Path $tmp "unpacked"
        & cmd /c (('"' + $C3 + '" --unpack "' + $pkgFile.FullName + '" --output-dir "' + $outDir + '" 2>&1')) | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "unpack failed" }
        foreach ($f in (Get-ChildItem -Path $PkgDir -File)) {
            if ($f.Extension -eq ".c3pkg") { continue }
            $srcHash = (Get-FileHash -Algorithm SHA1 $f.FullName).Hash
            $dst = Join-Path $outDir $f.Name
            if (-not (Test-Path $dst)) { throw "missing after unpack: $($f.Name)" }
            $dstHash = (Get-FileHash -Algorithm SHA1 $dst).Hash
            if ($srcHash -ne $dstHash) { throw "content differs: $($f.Name)" }
        }
        $script:pass++
        Write-Host "PASS" -ForegroundColor Green
    } catch {
        $script:fail++
        Write-Host "FAIL" -ForegroundColor Red
        Write-Host "  $($_.Exception.Message)" -ForegroundColor DarkGray
    } finally {
        if (Test-Path $tmp) { Remove-Item -Recurse -Force $tmp }
    }
}

# =============================================
# 语法检查: 用 -syntax-only 验证源码合法性; 未生成目标时仍计为通过
# =============================================

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  C3 Compiler Test Suite" -ForegroundColor Cyan
Write-Host "  $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# --- 语法测试 (--syntax-only) ---
# 说明 C3.exe 的构建链路: "源文件.bas -> 中间C -> cl/link -> 可执行文件"
# 参考 tests_github\t0_cases\smoke.bas (内含防呆提醒: 运行中弹 MsgBox 的用例需获得前台焦点)
if ($Category -in @("all", "smoke")) {
    Write-Host "--- Smoke Test (C3.exe end-to-end) ---" -ForegroundColor Yellow

    Test-Run "smoke" "$GHTests\smoke.bas" @("SMOKE-1:OK", "SMOKE-2:OK", "SMOKE-3:OK", "SMOKE PASS")
    Write-Host ""
}

# --- 纯 .bas 用例 (编译+运行; 支持 -Jobs 并行) ---
if ($Category -in @("all", "run", "bas")) {
    # 纯 .bas 用例统一入队; VBP/GUI 用例在独立分类 (vbp) 中保持串行
    $basQueue = @()
    function Add-BasTest {
        param([string]$Name, [string]$Source, $Expected = @(), [string]$Arch = "")
        $script:basQueue += @{
            Name     = $Name
            Source   = $Source
            Expected = @($Expected)
            Arch     = $Arch
        }
    }

    Write-Host "--- Regression (Compile+Run) ---" -ForegroundColor Yellow

    Add-BasTest "hello" "$Tests\hello.bas"
    Add-BasTest "test_m5" "$Tests\test_m5.bas"
    Add-BasTest "test_rtl" "$Tests\test_rtl.bas"
    Add-BasTest "test_array" "$Tests\test_array.bas" @("wa-clone=22", "wa-ub=3", "wa-str=65", "wa-rt=65", "=== Array Tests PASSED ===")
    Add-BasTest "test_fileio" "$Tests\test_fileio.bas"
    Add-BasTest "test_error" "$Tests\test_error.bas"
    Add-BasTest "test_now" "$Tests\test_now.bas"
    Add-BasTest "test_getput" "$Tests\test_getput.bas" @("PASS1a", "PASS1b", "PASS1c", "PASS2", "PASS3", "PASS4")
    Add-BasTest "test_onerror" "$Tests\test_onerror.bas" @("PASS1", "PASS2", "PASS3a", "PASS3b", "Done")
    Add-BasTest "test_ndarray" "$Tests\test_ndarray.bas" @("2D sum=270", "P8.1 ALL TESTS DONE")
    Add-BasTest "test_foreach" "$Tests\test_foreach4.bas" @("Long For Each: 150", "String For Each: Hello World")
    Add-BasTest "test_softkeyword" "$Tests\test_softkeyword.bas" @("Get=42", "Step=5", "Name=test")
    Add-BasTest "test_date" "$Tests\test_date.bas" @("PASS_Year", "PASS_Month", "PASS_Day", "PASS_NowYear", "Done")
    Add-BasTest "test_colon" "$Tests\test_colon.bas" @("PASS1", "PASS2", "PASS3", "Done")
    Add-BasTest "test_goto" "$GHTests\test_goto.bas" @("GOTO-PASS1", "GOTO-PASS2", "GOTO-PASS3", "GOTO-PASS4", "GOTO-PASS5", "GOTO-PASS6", "GOTO-PASS7", "GOTO DONE")
    Add-BasTest "test_gosub" "$GHTests\test_gosub.bas" @("GOSUB-PASS1", "GOSUB-PASS2", "GOSUB-PASS3", "GOSUB DONE")
    Add-BasTest "test_nested_udt_array" "$Tests\test_nested_udt_array.bas" @("NA1=1;NB1=D0;NC1=1.2", "NA5=5;NB5=D4;NC5=5.2", "HDR@ABC", "NESTED-DONE")
    Add-BasTest "test_udt_assign" "$Tests\test_udt_assign.bas" @("A1=1;S1=hello;N1=42", "A2=99;S2=world;N2=7", "H1=11;HS1=alpha", "E1=5;ES1=five", "L1=2;LS1=hello", "UDT-ASSIGN-DONE")
    Add-BasTest "test_date_display" "$Tests\test_date_display.bas" @("D1-noserial=Y", "D2-year=Y", "D2b-notime=Y", "D3-nextday=Y", "D4-nextyear=Y", "D5-diff0=Y", "D6-param=Y", "D7-longparam=Y", "D8-cstr=Y", "D9-format=Y", "DATE-DONE")
    Add-BasTest "test_variant" "$Tests\test_variant.bas" @("PASS1a", "PASS1c", "PASS5", "Done")
    Write-Host ""

        # --- P5.5 数据类型兼容性测试 ---
    Write-Host "--- Compat Tests (P5.5) ---" -ForegroundColor Yellow

    Add-BasTest "test_compat" "$Tests\test_compat.bas"
    Add-BasTest "test_types" "$Tests\test_types.bas"
    Add-BasTest "test_control" "$Tests\test_control.bas"
    Add-BasTest "test_declare" "$Tests\test_declare.bas"
    Write-Host ""

    # --- P5.7 语法/语义检查用例组 ---
    Write-Host "--- Bugfix Tests (P5.7) ---" -ForegroundColor Yellow

    Add-BasTest "test_fixes" "$Tests\test_fixes.bas" @("FIX1:OK", "FIX2:OK", "FIX3:OK", "All fixes passed!")
    Write-Host ""

    # --- P6 预处理器/冒烟测试用例组 ---
    Write-Host "--- COM Tests (P6) ---" -ForegroundColor Yellow

    Add-BasTest "test_com" "$Tests\test_com.bas" @("COM-1:OK", "COM-2:OK", "COM-3:OK", "COM:3/3")
    Add-BasTest "test_com2" "$Tests\test_com2.bas" @("Users")
    Add-BasTest "test_com3" "$Tests\test_com3.bas" @("COM3-1:OK", "COM3-2:OK", "COM3-3:OK", "COM3-4:OK", "COM3:4/4")
    Add-BasTest "test_earlybound" "$Tests\test_earlybound.bas" @("EB-1:OK", "EB-2:OK", "EB:2/2")
    Add-BasTest "test_p1324" "$Tests\test_p1324.bas" @("P13-1:OK", "P13-3:OK", "P13-5:OK", "P13:8/8")

    # --- P24 COM 测试用例 ---
    Write-Host "--- P24 COM Optimization Tests ---" -ForegroundColor Yellow

    Add-BasTest "test_p24" "$Tests\test_p24.bas" @("P24-01a:OK", "P24-01b:OK", "P24-01c:OK", "P24-03a:OK", "P24-03b:OK", "P24:5/5")
    Add-BasTest "test_earlybound2" "$Tests\test_earlybound2.bas" @("EB2-1:OK", "EB2-7:DriveType=2", "EB2-8:OK", "EB2-10:OK", "EB2:10/10") -Arch "x86"
    Add-BasTest "test_not_com" "$Tests\test_not_com.bas" @("NOT-COM:OK", "NOT-COM2:OK", "NOT-COM:PASS") -Arch "x86"
    Add-BasTest "test_err_obj" "$Tests\test_err_obj.bas" @("ERR-1:OK", "ERR-6:OK", "ERR:6/6")
    Add-BasTest "test_variant_cmp" "$Tests\test_variant_cmp.bas" @("VC-1:OK", "VC-4:OK", "VC:4/4")
    Add-BasTest "test_com_default_prop" "$Tests\test_com_default_prop.bas" @("DP-1:OK", "DP-4:OK", "P24-10: 4/4")
    Add-BasTest "test_com_optional" "$Tests\test_com_optional.bas" @("OP-1:OK", "OP-4:OK", "P24-11: 4/4")
    Add-BasTest "test_bstr_concat_scalar" "$Tests\test_bstr_concat_scalar.bas" @("BCS:16/16")
    # Fix 190: Declare "As Any" ByRef 的下标链实参必须取地址, 不能把元素值当指针
    Add-BasTest "test_asany_subscript" "$Tests\test_asany_subscript.bas" @("WITH-SUB=Y", "EXPR-SUB=Y", "SCALAR=Y", "CHAIN=Y", "ASANY-DONE")
    # Delegate (tB extension): typed function pointers, stdcall/cdecl thunks, both arches
    Add-BasTest "test_delegate" "$Tests\test_delegate.bas" @("CALL=Y", "INIT=Y", "REASSIGN=Y", "BITCMP=Y", "APICB=Y", "QSORT=Y", "MODVAR=Y", "DELEGATE-DONE")
    Add-BasTest "test_delegate_x86" "$Tests\test_delegate.bas" @("CALL=Y", "INIT=Y", "REASSIGN=Y", "BITCMP=Y", "APICB=Y", "QSORT=Y", "MODVAR=Y", "DELEGATE-DONE") -Arch "x86"
    # Overloading (tB extension): same-name by-type/arity resolution, Optional span, Variant tier
    Add-BasTest "test_overload" "$Tests\test_overload.bas" @("F-L5", "F-Shi", "F-L6", "3", "5", "O0", "O17", "21", "OVERLOAD-DONE")
    Add-BasTest "test_overload_x86" "$Tests\test_overload.bas" @("F-L5", "F-Shi", "F-L6", "3", "5", "O0", "O17", "21", "OVERLOAD-DONE") -Arch "x86"
    # Generics (tB extension): monomorphizing prepass — generic UDT (nested/multi-arity),
    # generic Function/Sub with explicit instantiation + call-site type inference.
    Add-BasTest "test_generics" "$Tests\test_generics.bas" @("G-A=12", "G-B=hi", "G-C=21 abc!", "G-D=10", "G-E=10", "G-F=ab", "G-G=20", "LEN=2", "G-H=0", "G-I=20", "GENERICS-DONE")
    Add-BasTest "test_generics_x86" "$Tests\test_generics.bas" @("G-A=12", "G-B=hi", "G-C=21 abc!", "G-D=10", "G-E=10", "G-F=ab", "G-G=20", "LEN=2", "G-H=0", "G-I=20", "GENERICS-DONE") -Arch "x86"
    # Interface (tB extension, ai/022 B01): contract-block syntax layer - the blocks
    # parse, the new keywords stay soft, and the codegen path is still untouched.
    Add-BasTest "test_interface" "$Tests\test_interface.bas" @("ITF-SOFT:12", "ITF-1:OK", "ITF-2:OK", "INTERFACE-DONE")
    Add-BasTest "test_interface_x86" "$Tests\test_interface.bas" @("ITF-SOFT:12", "ITF-1:OK", "ITF-2:OK", "INTERFACE-DONE") -Arch "x86"

    # 分片: CI 用多 runner 并行跑 bas 用例时, 各 runner 只取第 BasShard 片
    if ($BasShardTotal -gt 1) {
        if ($BasShard -lt 1 -or $BasShard -gt $BasShardTotal) {
            Write-Host "[ERROR] BasShard 需在 1..BasShardTotal" -ForegroundColor Red
            exit 1
        }
        $shardLen = [Math]::Ceiling($basQueue.Count / $BasShardTotal)
        $shardStart = ($BasShard - 1) * $shardLen
        $basQueue = @($basQueue[$shardStart..([Math]::Min($shardStart + $shardLen - 1, $basQueue.Count - 1))])
        Write-Host "  (bas shard ${BasShard}/${BasShardTotal}: $($basQueue.Count) tests)" -ForegroundColor Cyan
    }

    # 执行纯 .bas 用例 (串行或并行)
    $basSw = [Diagnostics.Stopwatch]::StartNew()
    if ($Jobs -gt 1 -and $PSVersionTable.PSVersion.Major -ge 7) {
        Write-Host "--- Bas Tests (parallel, jobs=$Jobs) ---" -ForegroundColor Yellow
        Invoke-BasSetParallel -Items $basQueue -Jobs $Jobs
    } else {
        if ($Jobs -gt 1) {
            Write-Host "[WARN] -Jobs>1 需要 PowerShell 7+, 当前 $($PSVersionTable.PSVersion) 回退串行" -ForegroundColor Yellow
        }
        Invoke-BasSetSerial -Items $basQueue
    }
    $basSw.Stop()
    Write-Host "  (bas tests took $([Math]::Round($basSw.Elapsed.TotalSeconds))s)"
}

# --- VBP 工程测试 (串行; GUI 窗口效果无法通过自动校验并行确认) ---
if ($Category -in @("all", "run", "vbp")) {
    # --- VBP 工程测试 (P5) --- (串行)
    Write-Host "--- VBP Project Tests (P5) ---" -ForegroundColor Yellow
    $vbpSw = [Diagnostics.Stopwatch]::StartNew()

    Test-Vbp "test_class" "$Tests\test_class.vbp" @("3", "0")

    # Cross-module overload resolution (O3): modA exports Sub/Function overload
    # groups; modB Main calls them. Deferred re-resolution must pick variants.
    Test-Vbp "ovl_xmod" "$Tests\ovl_xmod\test_ovl_xmod.vbp" @("XM1=105", "XM2=203", "XM3=SUB7", "XM3=STRhi", "XMOD-DONE")

    # Cross-module generics (G3): generic UDT + Function/Sub exported from mod A,
    # consumed in mod B via explicit instantiation and bare-call inference.
    Test-Vbp "gen_xmod" "$Tests\gen_xmod\gen_xmod.vbp" @("XM1=35", "XM2=ab", "XM3=12", "XM4=hi", "XM5=7", "XM6=99", "XLEN=2", "XM7=34", "XMOD-DONE")
    # Generic classes (G4): .cls `Class Name(Of T)` header template + whole-module
    # specialization clone (two specializations coexist; self-referential LNode).
    Test-Vbp "gen_cls" "$Tests\gen_cls\gen_cls.vbp" @("GC1=42", "GC2=box:42", "GC3=hey", "GC4=box:hey", "GC5=33", "GC6=y", "GCLS-DONE")

    Test-Vbp "M6Test" "$Tests\M6Test.vbp" @("M6A:OK", "M6B:OK", "M6C:OK", "M6D:OK", "M6 PASSED")
    Test-Vbp "modulemethod" "$Tests\test_modulemethod.vbp" @("30", "21")

    # QR code project (tests\VbQRCodegen-master): form loads, sets Image1.Picture via Stretch
    Test-GuiVbp "VbQRCodegen" "$Tests\VbQRCodegen-master\test\Project1.vbp"
    # BalloonTooltips: form loads with controls + creates its common-controls tooltip windows (x64).
    Test-GuiVbp "BalloonTooltips" "$Tests\BalloonTooltips\prjBalloonTooltips.vbp" -ExeName "BalloonTooltips"
    # Charts 2020 demo (3rd-party UserControl charts): windowless chart controls (x86 first;
    # x64 after LongPtr port of API pointers/handles in the .ctl/.cls sources).
    Test-GuiVbp "Charts2020" "$Tests\Charts 2020\Proyecto1.vbp" -Arch "x86" -AutoExitSec 3
    # czUI (czForm): 自定义 GDI+ UserControl (.ctl) 无边框窗体 demo, 需 -Arch x86 (32 位)
    Test-GuiVbp "czUI" "$Tests\czUI-main\czFormDemo.vbp" -Arch "x86" -AutoExitSec 3
    # NewTab: 第三方 OCX 控件 (NewTab01.ocx, 32 位) 真宿主验证. 免注册便携部署 (OCX 在工程目录, 由 harness 复制到 exe 旁, 不依赖本机注册);
    # 无边框窗体无关闭按钮/无自动退出逻辑, 用 -AutoExitSec 3 收尾避免阻塞后续测试
    Test-GuiVbp "NewTab" "$Tests\NewTab-test\Test.vbp" -Arch "x86" -AutoExitSec 3
    # ExtShow: 跨模块窗体默认实例"无参" Show (Fix 146 回归靶, 2026-09-20 vbman C2198):
    # .bas caller 调 Form2.Show, 定义侧签名 (hMDIClient, modal) 后调用侧须补 modal=0
    Test-GuiVbp "ExtShow" "$Tests\ext_show_test\test_ext_show.vbp" -AutoExitSec 3
    Write-Host ""

    Test-Vbp "test_implements" "$Tests\test_implements.vbp" @("IMPL1:OK", "IMPL2:OK", "Implements test PASSED")
    Test-Vbp "test_events" "$Tests\test_events\test_events.vbp" @("Events test PASSED")
    Test-Vbp "M7Test" "$Tests\m7_test\M7Test.vbp" @("4/4 PASSED")
    # ai/022 B03: cross-module new-style contract reached through an Interface head-line host
    Test-Vbp "itf_xmod_writer" "$Tests\itf_xmod\XWriter.vbp" @("XMOD1:OK", "XMOD2:OK", "IFV1:OK", "IFV2:OK", "IFV3:OK", "LIFE1:OK", "LIFE2:OK", "LIFE3:OK", "LIFE9:OK", "TERM last=bye", "TERM last=scoped", "QI1:OK", "QI2:OK", "QI3:OK", "QI4:OK", "TOF1:OK", "TOF2:OK", "TOF3:OK", "DN0:OK", "DN1:OK", "DN2:OK", "DN3:OK", "TOC1:OK", "TOC2:OK", "TOC3:OK")
    # ai/022 B10: `Implements IViaNamed Via m_h` -- six slots served by generated
    # adapters over the holder's own interface table (no forwarding member written).
    # Both architectures: the adapter dereferences a field whose type is another
    # class's struct, so a layout slip would only surface on x86 (022 D40 rule).
    $viaExpected = @("VIA1:OK", "VIA2:OK", "VIA3:OK", "VIA4:OK", "VIA5:OK",
        "VIA6:OK", "VIA7:OK", "VIA8:OK", "VIA9:OK", "VIA-DONE")
    Test-Vbp "itf_via_pair" "$Tests\itf_via\Via.vbp" $viaExpected
    Test-Vbp "itf_via_x86" "$Tests\itf_via\Via.vbp" $viaExpected -Arch "x86"
    # ai/022 B11/C05 (ai/026 section 5, items 3-5): a CoClass block name used AS A TYPE --
    # `As Circle` / `New Circle` / `CreateObject("ActApp.Circle")` all bind to the block's
    # [Implementation] class. CC2/CC3 walk the other type positions (module field, parameter,
    # return type); CC6 proves the group view keeps the virtual table (an overridden Area on an
    # Inherits chain must answer), CC4/CC5/CC7 the ProgID rewrite including case.
    # Both architectures: the rewritten variable's C type is a class struct pointer, so a layout
    # slip would show up on x86 only (022 D40 rule).
    $ccActExpected = @("CC1:OK", "CC2:OK", "CC3:OK", "CC4:OK", "CC5:OK", "CC6:OK", "CC7:OK",
        "CC8:OK", "CC9:OK", "CC-DONE")
    Test-Vbp "cc_act_pair" "$Tests\cc_act\Act.vbp" $ccActExpected
    Test-Vbp "cc_act_x86" "$Tests\cc_act\Act.vbp" $ccActExpected -Arch "x86"

    # test_vbman 用于验证外部 COM 组件 VBMANLIB (x86 DLL, 供 32 位程序调用)
    # ai/022 B07b: INH2..INH11 cover the merged member face + prefix-copied fields +
    # forwarding stubs (private Long/UDT/BSTR fields, Optional params, Property Get/Let,
    # 3-level chain, child-wins shadowing, base/derived instance isolation).
    # ai/022 B09b: run the same project on BOTH architectures. An inherited field is embedded by
    # value, so the derived struct's prefix must be byte-exact against the base struct; a wrong
    # field type is invisible on x64 when sizeof(void*) happens to equal that type's size, and only
    # the x86 build/run catches it (022 D39: INH35/INH36/INH39 failed on x86 for exactly that).
    $inhExpected = @(
        "INH0:derived", "INH1:OK", "INH2:OK", "INH3:OK", "INH4:OK", "INH5:OK",
        "INH6:OK", "INH7:OK", "INH8:OK", "INH9:OK", "INH10:OK", "INH11:OK",
        "INH12:OK", "INH13:OK", "INH14:OK", "INH15:OK", "INH16:OK",
        "INH17:OK", "INH18:OK", "INH19:OK", "INH20:OK", "INH21:OK",
        "INH22:OK", "INH23:OK", "INH24:OK", "INH25:OK", "INH26:OK",
        # ai/022 B09: INH44/INH45 = MyBase de-virtualized (the same members answer "derived"
        # through Me./obj.), INH49/INH50 = construction chain root->leaf + MyBase.Class_Initialize,
        # INH51 = Overrides returning a project class (com_entry.c forward-decl ordering).
        "INH44:OK", "INH45:OK", "INH46:OK", "INH47:OK", "INH48:OK", "INH49:OK", "INH50:OK",
        "INH51:OK", "INH52:OK",
        # ai/022 B09c: INH53/INH54 = `Set MyBase.<Property Set>` target side, INH55 = property
        # write through a UDT object field, INH56/INH57 = immediate-base Class_Initialize,
        # INH58 = root's Private UDT field used two levels down (the x86 stride case).
        "INH53:OK", "INH54:OK", "INH55:OK", "INH56:OK", "INH57:OK", "INH58:OK", "INH59:OK")
    Test-Vbp "cls_inh_pair" "$Tests\cls_inh\Inh.vbp" $inhExpected
    Test-Vbp "cls_inh_x86" "$Tests\cls_inh\Inh.vbp" $inhExpected -Arch "x86"
    Test-Vbp "test_vbman" "$Tests\test_vbman\test_vbman.vbp" @("P24-04a:OK", "P24-04b:OK", "P24-04:2/2") -Arch "x86" -RequiresCom "VBMANLIB.cVBMAN"
    $vbpSw.Stop()
    Write-Host "  (vbp/gui tests took $([Math]::Round($vbpSw.Elapsed.TotalSeconds))s)"
    Write-Host ""
}

if ($Category -in @("all", "compile")) {
    # --- 综合测试 (编译+运行, 以 Main 为程序入口) ---
    Write-Host "--- Compile Tests ---" -ForegroundColor Yellow
    
    Test-Compile "test_comprehensive" "$Tests\test_comprehensive.bas"
    Test-Compile "test_comprehensive2" "$Tests\test_comprehensive2.bas"
    Write-Host ""
    
    # --- P7 窗体测试 (GUI 验证: 窗体正常加载即可) ---
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
    # --- 生成物 / 输出目录说明 ---
    Write-Host "--- Syntax/Semantic Tests ---" -ForegroundColor Yellow
    
    # T0 拆分: 11 个用例移入 tests_github\t0_cases\; test_onerror 留 tests\ (run 类双登记, 不可挪)
    # test_goto / test_gosub 已升级为「编译+运行+输出比对」用例, 移入 bas 队列
    $syntaxTests = @(
        "$GHTests\test_basic.bas",
        "$GHTests\test_for.bas", "$GHTests\test_for2.bas",
        "$GHTests\test_select.bas",
        "$Tests\test_onerror.bas",
        "$GHTests\test_redim.bas",
        "$GHTests\test_setlet.bas", "$GHTests\test_setonly.bas",
        "$GHTests\test_assign.bas",
        "$GHTests\test_sem_minimal.bas", "$GHTests\test_sem_proc.bas", "$GHTests\test_semantic.bas"
    )

    foreach ($t in $syntaxTests) {
        $path = $t
        if (Test-Path $path) {
            $name = [System.IO.Path]::GetFileNameWithoutExtension($t)
            Test-Syntax $name $path
        }
    }
    # tB extension (ai/022 B01): Interface contract-block diagnostics must fire.
    $itfNeg = @(
        @("itf_n01_member_body", "$Tests\itf_neg\n01_member_body.bas", "must not contain an implementation body"),
        @("itf_n02_visibility", "$Tests\itf_neg\n02_visibility.bas", "must not carry an access modifier"),
        @("itf_n03_field", "$Tests\itf_neg\n03_field.bas", "accepts only Sub/Function/Property signatures"),
        @("itf_n04_missing_end", "$Tests\itf_neg\n04_missing_end.bas", "expected 'End Interface'"),
        @("itf_n05_unknown_attr", "$Tests\itf_neg\n05_unknown_attr.bas", "Unrecognized attribute line [NotAnAttr]"),
        @("itf_n06_attr_no_target", "$Tests\itf_neg\n06_attr_no_target.bas", "Attribute line must precede an Interface or CoClass declaration"),
        @("itf_n07_generic", "$Tests\itf_neg\n07_generic.bas", "does not support generic type parameters"),
        # ai/022 B02 (semantic layer): stage 2.7 contract registry + Implements checks
        @("itf_n08_missing_slot", "$Tests\itf_neg\n08_missing_slot.cls", "is not implemented by class"),
        @("itf_n09_sig_mismatch", "$Tests\itf_neg\n09_sig_mismatch.cls", "signature mismatch"),
        @("itf_n10_unknown_parent", "$Tests\itf_neg\n10_unknown_parent.bas", "extends unknown interface"),
        @("itf_n11_extends_cycle", "$Tests\itf_neg\n11_extends_cycle.bas", "Circular Extends chain"),
        @("itf_n12_dup_member", "$Tests\itf_neg\n12_dup_member.bas", "cannot be overloaded"),
        @("itf_n13_module_collision", "$Tests\itf_neg\n13_module_collision.bas", "collides with a module of the same name"),
        # ai/022 B02b: member-level Implements I.M[, I.N] trailing clause
        @("itf_n14_clause_no_member", "$Tests\itf_neg\n14_clause_no_member.cls", "has no member"),
        @("itf_n15_clause_not_implemented", "$Tests\itf_neg\n15_clause_not_implemented.cls", "does not implement interface"),
        @("itf_n16_clause_in_bas", "$Tests\itf_neg\n16_clause_in_bas.bas", "only valid in a class module"),
        @("itf_n17_clause_unqualified", "$Tests\itf_neg\n17_clause_unqualified.bas", "needs a qualified name"),
        @("itf_n18_clause_sig_mismatch", "$Tests\itf_neg\n18_clause_sig_mismatch.cls", "signature mismatch"),
        @("itf_n19_iface_in_generic", "$Tests\itf_neg\n19_iface_in_generic.cls", "not allowed inside a generic class template"),
        # ai/022 B03: Interface head-line host form (.cls named after its single block)
        @("itf_n20_host_extra_decl", "$Tests\itf_neg\n20_host_extra_decl.cls", "may contain only the Interface block"),
        # ai/022 B10 (Implements .. Via): the delegate clause is resolved in stage 2.7
        # Pass D, and both rejection reasons are error-level.
        @("itf_n21_via_no_field", "$Tests\itf_neg\n21_via_no_field.cls", "is not a module-level field"),
        @("itf_n23_via_not_iface", "$Tests\itf_neg\n23_via_not_iface.cls", "not an Interface block"),
        @("itf_n24_via_in_bas", "$Tests\itf_neg\n24_via_in_bas.bas", "only allowed in a class module"),
        # ai/022 B11/C01 (CoClass block, syntax layer): malformed clauses must be refused
        # at the parser -- C01 does no validation beyond block structure.
        @("itf_n25_coclass_no_name", "$Tests\itf_neg\n25_coclass_no_name.bas", "expected CoClass name"),
        @("itf_n26_coclass_missing_end", "$Tests\itf_neg\n26_coclass_missing_end.bas", "expected 'End CoClass'"),
        @("itf_n27_coclass_bad_member", "$Tests\itf_neg\n27_coclass_bad_member.bas", "CoClass block accepts only attribute lines"),
        @("itf_n28_coclass_ref_no_name", "$Tests\itf_neg\n28_coclass_ref_no_name.bas", "expected interface name after 'Interface' in CoClass block"),
        # ai/022 B11/C03a (CoClass shape + name validation, stage 2.7 Pass F): every refused
        # shape must report its own reason, and none of them may be silent any more.
        @("itf_n29_coclass_dup_name", "$Tests\itf_neg\n29_coclass_dup_name.bas", "is declared twice"),
        @("itf_n30_coclass_unknown_iface", "$Tests\itf_neg\n30_coclass_unknown_iface.bas", "is not an Interface block in this project"),
        @("itf_n31_coclass_two_defaults", "$Tests\itf_neg\n31_coclass_two_defaults.bas", "marks 2 interfaces [Default]"),
        @("itf_n32_coclass_dup_entry", "$Tests\itf_neg\n32_coclass_dup_entry.bas", "more than once (the contract set is a set)"),
        @("itf_n33_coclass_impl_missing", "$Tests\itf_neg\n33_coclass_impl_missing.bas", "is not a class module of this project"),
        @("itf_n34_coclass_exe_creatable", "$Tests\itf_neg\n34_coclass_exe_creatable.bas", "marks [ComCreatable(True)] in an EXE project"),
        # ai/022 B11/C03b (CoClass contract aggregation, stage 3.4c): the block binds a
        # class, and every slot of every listed interface must be met by that class or an
        # ancestor -- same VB3012/VB3017 family the Implements checker uses.
        @("itf_n37_coclass_missing_slot", "$Tests\itf_neg\n37_coclass_missing_slot.cls", "is not implemented by class"),
        @("itf_n38_coclass_sig_mismatch", "$Tests\itf_neg\n38_coclass_sig_mismatch.cls", "signature mismatch (interface:")
    )
    foreach ($c in $itfNeg) {
        if (Test-Path $c[1]) { Test-SyntaxFail $c[0] $c[1] $c[2] }
        else { Write-Host "  [SYNTAX-FAIL] $($c[0]) ... SKIP (missing case file)" -ForegroundColor DarkGray }
    }
    # ai/022 B10: the Via holder field must name a class that implements the interface
    # itself -- a same-named member is not enough (there is no slot field to delegate to).
    Test-SyntaxFailMulti "itf_n22_via_holder_not_impl" @("$Tests\itf_neg\n22_via_base.cls", "$Tests\itf_neg\n22_via_deleg.cls") "does not implement interface"
    # ai/022 B11/C03a: two more refusals only exist once a second module is in scope --
    # a class used as an interface (VB6 habit), and a block name shadowing another module.
    Test-SyntaxFailMulti "itf_n35_coclass_legacy_cls" @("$Tests\itf_neg\n35_legacy_cls_iface.bas", "$Tests\itf_neg\n35_cls.cls") "but that is a class module"
    Test-SyntaxFailMulti "itf_n36_coclass_vs_module" @("$Tests\itf_neg\n36_coclass_vs_module.bas", "$Tests\itf_neg\n36_other.bas") "collides with a module of the same name"
    # ai/022 B11/C03b: inheriting a CoClass block name was already refused, but the sentence
    # blamed a name that does exist in the project; the new wording names the real mistake.
    Test-SyntaxFailMulti "itf_n39_inherits_coclass" @("$Tests\itf_neg\n39_coclass_as_base.bas", "$Tests\itf_neg\n39_coclass_as_base_der.cls") "which is a CoClass block"
    # Positive guard (ai/022 B02): a class satisfying a new-style contract (
    # Extends-inherited slot + property tri-slot keys) must stay silent.
    if (Test-Path "$Tests\itf_pos\p01_contract_ok.cls") { Test-Syntax "itf_p01_contract_ok" "$Tests\itf_pos\p01_contract_ok.cls" }
    # ai/022 B02b positive guard: explicit clause binding (cross-interface slot, inherited
    # slot named via child interface, property tri-slot keys, arbitrary member names).
    if (Test-Path "$Tests\itf_pos\p02_clause_binding.cls") { Test-Syntax "itf_p02_clause_binding" "$Tests\itf_pos\p02_clause_binding.cls" }
    # ai/022 B03 positive guard: a host module name is no longer a name collision.
    if (Test-Path "$Tests\itf_pos\p03_headline_host.cls") { Test-Syntax "itf_p03_headline_host" "$Tests\itf_pos\p03_headline_host.cls" }
    # ai/022 B10 positive guard: Via is registered as a SOFT keyword, so an existing
    # program that uses Via as a variable name still parses.
    if (Test-Path "$Tests\itf_pos\p04_via_soft_ident.bas") { Test-Syntax "itf_p04_via_soft_ident" "$Tests\itf_pos\p04_via_soft_ident.bas" }
    # ai/022 B11/C01 positive guards: the CoClass block form parses end to end, and CoClass
    # stays usable as an ordinary identifier (soft keyword registration).
    if (Test-Path "$Tests\itf_pos\p05_coclass_block.bas") { Test-Syntax "itf_p05_coclass_block" "$Tests\itf_pos\p05_coclass_block.bas" }
    if (Test-Path "$Tests\itf_pos\p06_coclass_soft_ident.bas") { Test-Syntax "itf_p06_coclass_soft_ident" "$Tests\itf_pos\p06_coclass_soft_ident.bas" }
    if (Test-Path "$Tests\itf_pos\p07_coclass_cls_host.cls") { Test-Syntax "itf_p07_coclass_cls_host" "$Tests\itf_pos\p07_coclass_cls_host.cls" }
    # ai/022 B11/C03b positive guards: the contract may be met by an ancestor that merely
    # declares the members (p08), or delegated whole to a holder object (p09, B10 x B11).
    if (Test-Path "$Tests\itf_pos\p08_coclass_base.cls") {
        Test-SyntaxMulti "itf_p08_coclass_via_ancestor" @("$Tests\itf_pos\p08_coclass_ancestor_contract.cls", "$Tests\itf_pos\p08_coclass_base.cls", "$Tests\itf_pos\p08_coclass_der.cls")
    }
    if (Test-Path "$Tests\itf_pos\p09_coclass_holder.cls") {
        Test-SyntaxMulti "itf_p09_coclass_via_delegated" @("$Tests\itf_pos\p09_coclass_holder.cls", "$Tests\itf_pos\p09_coclass_impl.cls")
    }
    # ai/022 B11/C04 (026 section 6, D52): a VB6 class module that never says "CoClass".
    # Its header attribute lines fold into one CoClass record, solved by the SAME Pass E, and
    # the fold must stay read-only: VB_Creatable=True in an EXE project is the corpus' normal
    # shape (134 of 143 lines) and must NOT inherit C03a's VB3033; a folded name used as an
    # Inherits base must NOT get the "that is a CoClass block" wording (D52-3). C05 adds the
    # third file: the folded name used as a TYPE must still mean the class, so it must also
    # produce no activation line (D54-3).
    if (Test-Path "$Tests\itf_pos\p10_coclass_fold_base.cls") {
        Test-SyntaxNote "itf_p10_coclass_fold" @("$Tests\itf_pos\p10_coclass_fold_base.cls", "$Tests\itf_pos\p10_coclass_fold_der.cls", "$Tests\itf_pos\p10_coclass_fold_use.bas") @(
            "C3: CoClass 'FoldBase' identity: CLSID={96466C30-E240-55A4-9434-24F1C884C2AB} (minted) IID=- (missing) ProgID=VB6EXE.FoldBase (minted) impl='FoldBase' comCreatable=True folded-from-legacy: VB_Creatable=True VB_Exposed=False VB_PredeclaredId=False VB_GlobalNameSpace=False",
            "C3: CoClass 'FoldDer' identity:") @("VB_VarHelpID", "VB_Description", "is a CoClass block", "VB3033", "activated in-project")
    }
    # Both shapes in one module: the hand-written block wins, so the record carries no fold
    # tag and [ComCreatable(False)] -- not VB_Creatable=True -- is what reaches the identity.
    if (Test-Path "$Tests\itf_pos\p11_coclass_block_wins.cls") {
        Test-SyntaxNote "itf_p11_coclass_block_wins" @("$Tests\itf_pos\p11_coclass_block_wins.cls") @(
            "C3: class 'FoldWins' has both a CoClass block and 4 legacy header attribute line(s): the block wins, the attributes are not folded",
            "CoClass 'FoldWins' identity: CLSID={EA2B2FD6-E5C6-5192-D0C9-A13BC6FC7859} (minted) IID=- (missing) ProgID=VB6EXE.FoldWins (minted) impl='' comCreatable=False") @("folded-from-legacy")
    }
    # ai/022 B11/C05 (ai/026 section 5, items 3-5): the observable face of in-project
    # activation is one information line per block that is ACTUALLY USED as a type -- and no
    # line at all for a block nobody binds to (cc_id declares three and uses none, see the
    # byte guard). Asserting the line rather than the exit code, because the stage succeeds.
    Test-SyntaxNote "cc_act_group_names" @("$Tests\cc_act\Act.vbp") @(
        "CoClass 'Circle' activated in-project: type name -> class 'ShapeAct'",
        "CoClass 'Ring' activated in-project: type name -> class 'RingAct'",
        "ProgID=ActApp.Circle") @("declares no [Implementation]", "VB3039")
    # A block without [Implementation] stays legal, but binding a variable to it has no
    # answer -- the use site is where the refusal lands (D54-2: today that shape is a silent
    # late-bound call on a null pointer, which is worse than an error).
    Test-SyntaxFail "itf_n40_coclass_type_no_impl" "$Tests\itf_neg\n40_coclass_type_no_impl.bas" "declares no [Implementation] class"
    # ai/022 B11/C02: the three identity tiers, asserted against expected GUIDs computed by an
    # independent FNV-1a re-implementation of the seed strings "coc:<proj>.<coclass>" and
    # "itf:<proj>.<iface>" (both lowered) -- NOT scraped from this compiler's own output, or the
    # test could only ever re-state the implementation (ai/022 D46).
    $ccShapes = "$Tests\cc_id\Id.vbp"
    $ccOther = "$Tests\cc_id\Id2.vbp"
    Test-IdentityNote "cc_id_explicit_tier" $ccShapes @(
        "CoClass 'CCCircle' identity: CLSID={11111111-1111-1111-1111-111111111111} (explicit) IID={22222222-3333-4444-5555-666666666666} (explicit) ProgID=Shapes.Circle (explicit) impl='CircleImpl' comCreatable=False",
        "CoClass 'CCVbp' identity: CLSID={33333333-4444-5555-6666-777777777777} (vbp) IID={62D63A9A-7316-DAB9-B2D1-5DDFB57EDB17} (minted) ProgID=ShapesApp.CCVbp (minted) impl='VbpImpl' comCreatable=False",
        "CoClass 'CCMint' identity: CLSID={A5B36375-4E3A-D5F9-D2A2-1912F3EDC498} (minted) IID={62D63A9A-7316-DAB9-B2D1-5DDFB57EDB17} (minted) ProgID=ShapesApp.CCMint (minted) impl='' comCreatable=False")
    # Same sources, only the vbp Name= differs: the minted tier moves while the explicit line
    # stays character-identical to the case above -- that split IS the reproducibility claim.
    Test-IdentityNote "cc_id_project_name_scope" $ccOther @(
        "CoClass 'CCCircle' identity: CLSID={11111111-1111-1111-1111-111111111111} (explicit)",
        "CoClass 'CCVbp' identity: CLSID={33333333-4444-5555-6666-777777777777} (vbp) IID={28519764-65C8-D639-C831-604BAD706603} (minted) ProgID=OtherApp.CCVbp (minted) impl='VbpImpl' comCreatable=False",
        "CoClass 'CCMint' identity: CLSID={CE88DE91-E77D-563D-D74F-5CA0DF3902D2} (minted) IID={28519764-65C8-D639-C831-604BAD706603} (minted) ProgID=OtherApp.CCMint (minted)")
    Test-IdentityStable "cc_id_repeatable" $ccShapes
    # ai/022 B11/C04: a class that writes NO block but is listed in the .vbp the VB6 way
    # (Class=Name; file.cls; {CLSID}). Folding has to hand that entry to the same resolver,
    # so the vbp tier lights up for legacy projects too -- the proof that there is still only
    # one identity channel (D47) rather than a legacy side door.
    Test-IdentityNote "cc_id_fold_vbp_tier" "$Tests\cc_id\IdFold.vbp" @(
        "CoClass 'FoldVbp' identity: CLSID={77777777-8888-9999-AAAABBBBBBBBBBBB} (vbp) IID=- (missing) ProgID=FoldApp.FoldVbp (minted) impl='FoldVbp' comCreatable=True folded-from-legacy: VB_Creatable=True VB_Exposed=False VB_PredeclaredId=False VB_GlobalNameSpace=False")
    Test-IdentityStable "cc_id_fold_repeatable" "$Tests\cc_id\IdFold.vbp"
    # ai/022 B07a (class Inherits, P3): chain diagnostics must fire. Single-file cases ride
    # the existing Test-SyntaxFail path; the two-module cases need Test-SyntaxFailMulti.
    $clsInhNeg = @(
        @("ci_n01_unknown_base", "$Tests\cls_neg\ci_n01_unknown_base.cls", "inherits unknown base class"),
        @("ci_n02_not_class", "$Tests\cls_neg\ci_n02_not_class.bas", "only allowed in a class module"),
        @("ci_n03_duplicate_clause", "$Tests\cls_neg\ci_n03_duplicate_clause.cls", "more than one Inherits clause"),
        @("ci_n04_self_cycle", "$Tests\cls_neg\ci_n04_self_cycle.cls", "Circular Inherits chain"),
        @("ci_n05_generic_template", "$Tests\cls_neg\ci_n05_generic_template.cls", "not allowed inside a generic class template"),
        @("ci_n11_protected_in_interface", "$Tests\cls_neg\ci_n11_protected_in_interface.bas", "must not carry an access modifier")
    )
    foreach ($c in $clsInhNeg) {
        if (Test-Path $c[1]) { Test-SyntaxFail $c[0] $c[1] $c[2] }
        else { Write-Host "  [SYNTAX-FAIL] $($c[0]) ... SKIP (missing case file)" -ForegroundColor DarkGray }
    }
    if (Test-Path "$Tests\cls_neg\ci_n06_pair_a.cls") {
        Test-SyntaxFailMulti "ci_n06_pair_cycle" @("$Tests\cls_neg\ci_n06_pair_a.cls", "$Tests\cls_neg\ci_n06_pair_b.cls") "Circular Inherits chain"
    }
    if (Test-Path "$Tests\cls_neg\ci_n07_iface_host.cls") {
        Test-SyntaxFailMulti "ci_n07_base_is_iface_host" @("$Tests\cls_neg\ci_n07_iface_host.cls", "$Tests\cls_neg\ci_n07_derives_host.cls") "inherits unknown base class"
    }
    # ai/022 B07b (v1 boundaries): unqualified inherited call, inherited field redeclared,
    # and an event-bearing base. All three are two-module cases -> Test-SyntaxFailMulti.
    if (Test-Path "$Tests\cls_neg\ci_n08_base.cls") {
        Test-SyntaxFailMulti "ci_n08_bare_inherited_call" @("$Tests\cls_neg\ci_n08_base.cls", "$Tests\cls_neg\ci_n08_derived.cls") "cannot be called unqualified"
    }
    if (Test-Path "$Tests\cls_neg\ci_n09_base.cls") {
        Test-SyntaxFailMulti "ci_n09_redeclared_field" @("$Tests\cls_neg\ci_n09_base.cls", "$Tests\cls_neg\ci_n09_derived.cls") "redeclares inherited field"
    }
    if (Test-Path "$Tests\cls_neg\ci_n10_base.cls") {
        Test-SyntaxFailMulti "ci_n10_event_base" @("$Tests\cls_neg\ci_n10_base.cls", "$Tests\cls_neg\ci_n10_derived.cls") "declares an Event"
    }
    # ai/022 B08b/B08d (Overridable/Overrides): the contract shapes are two-module cases,
    # the three placement cases are single-file. All ride the existing helpers.
    $ovNeg = @(
        @("ci_n12_override_ghost", "ci_n12", "no matching member in the inherited class chain"),
        @("ci_n13_not_overridable", "ci_n13", "not declared Overridable"),
        @("ci_n14_override_signature", "ci_n14", "does not match the Overridable member"),
        @("ci_n19_propertylet_virtual", "ci_n19", "is a Property Let/Set, which this build cannot dispatch"),
        @("ci_n20_bare_virtual_call", "ci_n20", "Bare (unqualified) call to overridable member")
    )
    foreach ($c in $ovNeg) {
        $a = "$Tests\cls_neg\" + $c[1] + "_base.cls"
        $b = "$Tests\cls_neg\" + $c[1] + "_derived.cls"
        if ((Test-Path $a) -and (Test-Path $b)) {
            Test-SyntaxFailMulti $c[0] @($a, $b) $c[2]
        } else {
            Write-Host "  [SYNTAX-FAIL] $($c[0]) ... SKIP (missing case files)" -ForegroundColor DarkGray
        }
    }
    if (Test-Path "$Tests\cls_neg\ci_n16_override_no_inherits.cls") {
        Test-SyntaxFail "ci_n16_override_no_base" "$Tests\cls_neg\ci_n16_override_no_inherits.cls" "has no base class to override"
    }
    if (Test-Path "$Tests\cls_neg\ci_n17_overridable_in_bas.bas") {
        Test-SyntaxFail "ci_n17_overridable_in_bas" "$Tests\cls_neg\ci_n17_overridable_in_bas.bas" "only allowed in a class module"
    }
    if (Test-Path "$Tests\cls_neg\ci_n18_overridable_in_interface.bas") {
        Test-SyntaxFail "ci_n18_overridable_in_iface" "$Tests\cls_neg\ci_n18_overridable_in_interface.bas" "must not carry a virtual modifier"
    }
    # B07a positive guard: a base class in another module resolves and stays silent.
    if (Test-Path "$Tests\cls_neg\ci_pos_base.cls") {
        Test-SyntaxMulti "ci_pos_pair" @("$Tests\cls_neg\ci_pos_base.cls", "$Tests\cls_neg\ci_pos_derived.cls")
    }
    # ai/022 B08c (Protected access from outside the class family): the three out-of-family
    # shapes below must be rejected at the call site, while in-family access (a base-typed
    # variable used from the derived class, and from the declaring class itself) stays silent.
    $protNeg = @(
        @("ci_n21_prot_field_write", "ci_n21_base.cls", "ci_n21_stranger.cls"),
        @("ci_n22_prot_sub_stdmod", "ci_n22_base.cls", "ci_n22_outsider.bas"),
        @("ci_n23_prot_property_get", "ci_n23_base.cls", "ci_n23_stranger.cls")
    )
    foreach ($c in $protNeg) {
        $a = "$Tests\cls_neg\" + $c[1]
        $b = "$Tests\cls_neg\" + $c[2]
        if ((Test-Path $a) -and (Test-Path $b)) {
            Test-SyntaxFailMulti $c[0] @($a, $b) "is Protected"
        } else {
            Write-Host "  [SYNTAX-FAIL] $($c[0]) ... SKIP (missing case files)" -ForegroundColor DarkGray
        }
    }
    if (Test-Path "$Tests\cls_neg\ci_pos2_base.cls") {
        Test-SyntaxMulti "ci_pos2_prot_in_family" @("$Tests\cls_neg\ci_pos2_base.cls", "$Tests\cls_neg\ci_pos2_derived.cls")
    }
    # ai/022 B08e-6 (class-vtable dispatch map, codegen stage): the three shapes below carry a
    # call inside the receiver, so a virtual call there would need the receiver twice. Sites 12
    # and 10 used to compile and bind statically to the base body; they now report VB3027. Site
    # 8 already reported it through the priority-2 dispatcher -- pinned here so a refactor
    # cannot lose it. Site 9's receiver is a plain return variable, so it must still compile.
    $cgenVirtNeg = @(
        @("ci_n24_chain_receiver", "ci_n24", "cannot be dispatched in this build"),
        @("ci_n25_prop_chain_receiver", "ci_n25", "cannot be dispatched in this build"),
        @("ci_n26_prop_receiver", "ci_n26", "cannot be dispatched in this build"),
        # ai/022 B09: `MyBase` where there is no base class, and `MyBase.<name>` the base face
        # does not have -> VB3028. Both used to compile into an undefined `vb6_MyBase_<name>`.
        @("ci_n27_mybase_no_inherits", "ci_n27", "has no Inherits clause"),
        @("ci_n28_mybase_no_such_member", "ci_n28", "has no such member")
    )
    foreach ($c in $cgenVirtNeg) {
        $a = "$Tests\cls_neg\" + $c[1] + "_base.cls"
        $b = "$Tests\cls_neg\" + $c[1] + "_derived.cls"
        if ((Test-Path $a) -and (Test-Path $b)) {
            Test-CompileFail $c[0] @($a, $b) $c[2]
        } else {
            Write-Host "  [COMPILE-FAIL] $($c[0]) ... SKIP (missing case files)" -ForegroundColor DarkGray
        }
    }
    if (Test-Path "$Tests\cls_neg\ci_pos3_base.cls") {
        Test-Compile "ci_pos3_retval_receiver" @("$Tests\cls_neg\ci_pos3_base.cls", "$Tests\cls_neg\ci_pos3_derived.cls")
    }
    Write-Host ""
    
    # --- 生成环境检查与汇总 (冒烟+语法+VBP+run 计数) ---
    Write-Host "--- Preprocessor Tests ---" -ForegroundColor Yellow
    
    # T0 拆分: pp 系列全部移入 tests_github\t0_cases\, 跨目录引用
    $ppTests = @(
        "$GHTests\test_preprocess.bas",
        "$GHTests\test_pp_minimal.bas",
        "$GHTests\test_pp2.bas", "$GHTests\test_pp3.bas", "$GHTests\test_pp4.bas", "$GHTests\test_pp5.bas",
        "$GHTests\test_pp6.bas", "$GHTests\test_pp7.bas", "$GHTests\test_pp8.bas", "$GHTests\test_pp9.bas",
        "$GHTests\test_pp10.bas", "$GHTests\test_pp11.bas", "$GHTests\test_pp12.bas", "$GHTests\test_pp13.bas",
        "$GHTests\test_pp14.bas", "$GHTests\test_pp15.bas", "$GHTests\test_pp16.bas", "$GHTests\test_pp17.bas",
        "$GHTests\test_pp18.bas"
    )

    foreach ($t in $ppTests) {
        $path = $t
        if (Test-Path $path) {
            $name = [System.IO.Path]::GetFileNameWithoutExtension($t)
            Test-Syntax $name $path
        }
    }
    Write-Host ""
}

# ai/023 S01: package/project-reference hard checks (diagnostics only, no codegen).
# Negatives ride --syntax-only (stage-0 checks fire before the pipeline); positives
# build+run the host to prove the checks don't break a normal build (D7: warnings
# never reject a build).
if ($Category -in @("all", "pkg")) {
    Write-Host "--- Package Reference Tests (ai/023 S01) ---" -ForegroundColor Yellow
    Test-VbpFail "pkg_n01_not_found"    "$Tests\pkg_neg\n01_not_found.vbp"    "package not found in any search root"
    Test-VbpFail "pkg_n02_escape_name"  "$Tests\pkg_neg\n02_escape_name.vbp"  "package name contains path characters"
    Test-VbpFail "pkg_n03_conflict"     "$Tests\pkg_neg\n03_conflict.vbp"     "package name conflicts with project name"
    Test-VbpFail "pkg_n04_bad_manifest" "$Tests\pkg_neg\n04_bad_manifest.vbp" "unknown key in [C3Package]"
    Test-VbpFail "pkg_n05_name_mismatch" "$Tests\pkg_neg\n05_name_mismatch.vbp" "manifest Name mismatch"
    # ai/023 S02: package source loading — a package module may not collide with a
    # host module name (checked at load time, before the pipeline).
    Test-VbpFail "pkg_n06_module_conflict" "$Tests\pkg_neg\n06_module_conflict.vbp" "package module name conflicts with host module"
    if (Test-Path "$Tests\pkg_pos\p01_ok.vbp") {
        Test-Vbp "pkg_p01_ok" "$Tests\pkg_pos\p01_ok.vbp" @("PKG-S01:OK")
    }
    if (Test-Path "$Tests\pkg_pos\p02_missing_file_warn.vbp") {
        Test-Vbp "pkg_p02_missing_file_warn" "$Tests\pkg_pos\p02_missing_file_warn.vbp" @("PKG-S01:OK")
    }
    # ai/023 S02: host calls a package Function and Property Get across modules.
    if (Test-Path "$Tests\pkg_xmod\pkg_xmod_ok.vbp") {
        Test-Vbp "pkg_s02_xmod" "$Tests\pkg_xmod\pkg_xmod_ok.vbp" @("PKG-XMOD:42", "PKG-XMOD:XM")
    }
    # ai/023 S03: package export boundary — Friend member invisible to host (VB7006),
    # visible again with manifest Friend=True.
    if (Test-Path "$Tests\pkg_xmod\friend_bad.vbp") {
        Test-VbpBuildFail "pkg_s03_friend_blocked" "$Tests\pkg_xmod\friend_bad.vbp" "is not exported by package"
    }
    if (Test-Path "$Tests\pkg_xmod\friend_open_ok.vbp") {
        Test-Vbp "pkg_s03_friend_open" "$Tests\pkg_xmod\friend_open_ok.vbp" @("PKG-FRIEND-OK")
    }
    # ai/023 S04: class modules across the package boundary. An exported class binds
    # statically (PKG-C1 = New + method + property); a class the manifest does not
    # export must be a hard VB7006 error, not a silent late-bound COM fallback
    # (codegen emits vb6_NewObject(L"Cls") when the class symbol is missing).
    if (Test-Path "$Tests\pkg_cls\cls_ok.vbp") {
        Test-Vbp "pkg_s04_cls_ok" "$Tests\pkg_cls\cls_ok.vbp" @("PKG-C1:42")
    }
    if (Test-Path "$Tests\pkg_cls\cls_neg.vbp") {
        Test-VbpBuildFail "pkg_s04_cls_blocked" "$Tests\pkg_cls\cls_neg.vbp" "does not export it"
    }
    if (Test-Path "$Tests\pkg_cls\dimonly.vbp") {
        # `Dim x As Cls` alone (no New) must also be rejected — otherwise the type
        # silently degrades to Object/void*.
        Test-VbpBuildFail "pkg_s04_cls_dim_only" "$Tests\pkg_cls\dimonly.vbp" "does not export it"
    }
    # ai/084a M3: member-level Friend boundary on exported package classes —
    # obj.<Friend member> from the host is a hard 7008 unless the manifest
    # declares Friend=True (same-package / package-to-package stay unrestricted).
    if (Test-Path "$Tests\pkg_cls\cls_friend_obj_bad.vbp") {
        Test-VbpBuildFail "acc_m3_pkg_friend_obj_blocked" "$Tests\pkg_cls\cls_friend_obj_bad.vbp" "Friend"
    }
    if (Test-Path "$Tests\pkg_cls\cls_friend_obj_open_ok.vbp") {
        Test-Vbp "acc_m3_pkg_friend_obj_open" "$Tests\pkg_cls\cls_friend_obj_open_ok.vbp" @("FR-OPEN:12")
    }
    if (Test-Path "$Tests\pkg_cls\samepkg_ok.vbp") {        # Same-package module uses the non-exported class: must NOT be blocked.
        Test-Vbp "pkg_s04_cls_same_package" "$Tests\pkg_cls\samepkg_ok.vbp" @("PKG-C3:OK")
    }
    # ai/023 S05: per-file sha1/size verification. Clean package = no warning;
    # tampered byte = warning VB7007 and build CONTINUES (D7 校验不拒收);
    # --check-packages prints resolution read-only (OK / MISMATCH) and exits.
    if (Test-Path "$Tests\pkg_chk\chk_ok.vbp") {
        Test-Vbp "pkg_s05_chk_ok" "$Tests\pkg_chk\chk_ok.vbp" @("PKG-S05:55")
        Test-CliOk "pkg_s05_check_packages_ok" @('"' + "$Tests\pkg_chk\chk_ok.vbp" + '"', "--check-packages") "sha1=OK"
    }
    if (Test-Path "$Tests\pkg_chk\tamper.vbp") {
        Test-VbpWarn "pkg_s05_tamper_warn_builds" "$Tests\pkg_chk\tamper.vbp" "hash mismatch"
        Test-CliOk "pkg_s05_check_packages_tampered" @('"' + "$Tests\pkg_chk\tamper.vbp" + '"', "--check-packages") "sha1=MISMATCH"
    }
    # ai/023 S06: pack -> unpack roundtrip must be byte-identical.
    if (Test-Path "$Tests\pkg_chk\packages\ChkPkg-1.0\package.c3d") {
        Test-PackRoundtrip "pkg_s06_pack_roundtrip" "$Tests\pkg_chk\packages\ChkPkg-1.0"
    }
    Write-Host ""
}

# ai/084a M1/M2: class member access levels. Positives prove same-class (Me. /
# other instance), Friend-same-project and inherited-Private-field access stay
# legal; the negative proves Private proc access from outside the class is a
# hard 3028 (previously a silent C2129 at MSVC stage).
if ($Category -in @("all", "acc")) {
    Write-Host "--- Member Access Level Tests (ai/084a M1/M2) ---" -ForegroundColor Yellow
    if (Test-Path "$Tests\acc\acc_ok.vbp") {
        Test-Vbp "acc_m2_ok" "$Tests\acc\acc_ok.vbp" @("ACC-USE:6", "ACC-OK")
    }
    if (Test-Path "$Tests\acc\acc_fam.vbp") {
        Test-Vbp "acc_m2_family_field" "$Tests\acc\acc_fam.vbp" @("ACC-FAM:3")
    }
    if (Test-Path "$Tests\acc\acc_neg.vbp") {
        Test-VbpBuildFail "acc_m2_private_blocked" "$Tests\acc\acc_neg.vbp" "Private"
    }
    # ai/084a: Protected — family-internal access (Me. and obj.) stays legal, a
    # stranger reaching a Protected member is a hard 3023 (placeholder now landed).
    if (Test-Path "$Tests\acc\acc_prot_ok.vbp") {
        Test-Vbp "acc_prot_family_ok" "$Tests\acc\acc_prot_ok.vbp" @("PROT:fam")
    }
    if (Test-Path "$Tests\acc\acc_prot_neg.vbp") {
        Test-VbpBuildFail "acc_prot_stranger_blocked" "$Tests\acc\acc_prot_neg.vbp" "Protected"
    }
    Write-Host ""
}

# =============================================
# ctor: ai/084c 类构造函数 — 带参 (New Cls(args) → _NewParams) 与无参 (Class_Initialize)
# =============================================
if ($Category -in @("all", "ctor")) {
    Write-Host "--- ctor (ai/084c class constructors) ---" -ForegroundColor Yellow
    if (Test-Path "$Tests\ctor\ctor_ok.vbp") {
        Test-Vbp "ctor_ok" "$Tests\ctor\ctor_ok.vbp" @("amt:42", "CNT:7")
    }
    if (Test-Path "$Tests\ctor\ctor_neg.vbp") {
        Test-VbpBuildFail "ctor_neg_arity" "$Tests\ctor\ctor_neg.vbp" "3035"
    }
    if (Test-Path "$Tests\ctor\ctor_neg2.vbp") {
        Test-VbpBuildFail "ctor_neg2_no_params" "$Tests\ctor\ctor_neg2.vbp" "3035"
    }
    Write-Host ""
}

# =============================================
# asm: ai/vb-asm-extension-spec — Asm 块完整形态
#   x64: 生成 .asm → ml64 → 链接 (基本块/ByRef/Naked/自动保存/Clobber/单行)
#   x86: __asm{} 内联块 (同一份语法换后端)
#   混排 (项2/项3): Asm 片段与 VB 语句混排 + 片段内引用 VB 局部变量
#   负例: 3037 (类方法里的 Asm) / 3036 (x86 <Naked> 引用参数) / 2012 (<Naked> 修饰非过程) /
#         2014 (Clobber 参数非字符串) / 3040 ([X] 解析不到) / 3041 (x64 引用 >4 个变量)
# =============================================
if ($Category -in @("all", "asm")) {
    Write-Host "--- Asm Block Tests (ai/vb-asm-extension-spec) ---" -ForegroundColor Yellow
    if (Test-Path "$Tests\asm\asm_ok.vbp") {
        Test-Vbp "asm_ok" "$Tests\asm\asm_ok.vbp" @(
            "ASM-ADD:42", "ASM-ATOMIC-OLD:10", "ASM-ATOMIC-NEW:15",
            "ASM-KEEP-RBX:37", "ASM-CLOBBER:12", "ASM-NAKED:100", "ASM-ONELINE:1234",
            # 项4 x64 浮点 (XMM0/xmm1 + xmm0 返回) / 项5 x64 栈传参 (shadow space 之后) /
            # 项6 x64 int64 返回 (RAX) —— 见 spec §12.1/12.2
            "ASM-DBL:3.75", "ASM-SUM6:21", "ASM-BIG64:4000000000", "ASM-DONE")
    }
    if (Test-Path "$Tests\asm\asm_x86.vbp") {
        Test-Vbp "asm_x86_inline" "$Tests\asm\asm_x86.vbp" @(
            "X86-ADD:42", "X86-KEEP-EBX:37", "X86-CLOBBER:12",
            "X86-NAKED:5", "X86-ONELINE:1234",
            # 项4 x86 浮点返回 (ST0 → fstp) / 项5 x86 全栈参数按名解析 /
            # 项6 x86 int64 返回 (EDX:EAX + [Function+4]) —— 见 spec §12.1/12.3
            "X86-DBL:3.75", "X86-SUM5:15", "X86-BIG:4000000000",
            "X86-MAKE64:4294967297", "X86-DONE") -Arch "x86"
    }
    # --- 项2/项3 混排 + 片段引用 VB 局部变量 (spec §12.5) ---
    if (Test-Path "$Tests\asm\asm_mixed_basic.vbp") {
        # 最小混排: 片段 + 一条 VB 语句; VB 语句覆盖片段写的返回值
        Test-Vbp "asm_mixed_basic" "$Tests\asm\asm_mixed_basic.vbp" @("MIXED:1")
    }
    if (Test-Path "$Tests\asm\asm_mixed.vbp") {
        # x64: 片段降级为独立 MASM 过程, 变量以地址传入 ([X] → [rcx])
        #   局部变量读写 / 多片段 / ByRef 参数 / [Function] / callee-saved 自动保存 /
        #   Clobber / [X+4] 偏移形态
        Test-Vbp "asm_mixed_x64" "$Tests\asm\asm_mixed.vbp" @(
            "MIX-ACC:175", "MIX-TWO:22", "MIX-BUMP:15", "MIX-RET:42",
            "MIX-KEEP-RBX:80", "MIX-CLOBBER:11", "MIX-OFFSET:4294967297",
            "MIX-GLOBAL:1005", "MIX-DONE")
    }
    if (Test-Path "$Tests\asm\asm_mixed_x86.vbp") {
        # x86: 片段就地发 __asm{} 内联块, [X] 按名解析 (ByRef 参数经本地副本对齐 x64 语义);
        #      另加一例引用 5 个变量 (x86 无 4 个上限)
        Test-Vbp "asm_mixed_x86_inline" "$Tests\asm\asm_mixed_x86.vbp" @(
            "XMIX-ACC:175", "XMIX-TWO:22", "XMIX-BUMP:15", "XMIX-RET:42",
            "XMIX-KEEP-EBX:80", "XMIX-CLOBBER:11", "XMIX-OFFSET:4294967297",
            "XMIX-SUMMIX:1006", "XMIX-GLOBAL:1005", "XMIX-DONE") -Arch "x86"
    }
    if (Test-Path "$Tests\asm\asm_mixed_neg.vbp") {
        # 3040: [X] 既不是寄存器也不是可见的 VB 变量 (两个架构都触发)
        # 3041: x64 单个片段引用 >4 个 VB 变量 (x64 专属, 见下)
        Test-VbpBuildFail "asm_mixed_neg_unresolved_ref" "$Tests\asm\asm_mixed_neg.vbp" "3040"
    }
    if (Test-Path "$Tests\asm\asm_mixed_neg.vbp") {
        # 3041 只在 x64 出现 (x86 名字解析走栈帧, 不占参数寄存器)
        $script:total++
        Write-Host -NoNewline "  [VBP-BUILD-FAIL] asm_mixed_neg_ref_limit ... "
        $result = & cmd /c ('"' + $C3 + '" "' + "$Tests\asm\asm_mixed_neg.vbp" + '" --output-dir "' + $OutDir + '" 2>&1')
        $text = (($result | Out-String) -replace '\s+', ' ')
        if ($LASTEXITCODE -ne 0 -and $text.Contains("3041")) {
            $script:pass++
            Write-Host "PASS" -ForegroundColor Green
        } else {
            $script:fail++
            Write-Host "FAIL" -ForegroundColor Red
            Write-Host "  expected failing build containing: 3041" -ForegroundColor DarkGray
            if ($Verbose) { Write-Host $text }
        }
    }
    if (Test-Path "$Tests\asm\asm_alias_neg.vbp") {
        # 项1: cmpxchg/mul 的隐含累加器与指针/基址同族 (RAX/EAX) → 3042
        # (实测的静默死循环/段错误, 现在编译期拦住)
        Test-VbpBuildFail "asm_neg_accum_alias" "$Tests\asm\asm_alias_neg.vbp" "3042"
    }
    if (Test-Path "$Tests\asm\asm_alias_x86_neg.vbp") {
        # 同上, x86 内联块 (Test-VbpBuildFail 不带自定义参数, 就地内联判据)
        $script:total++
        Write-Host -NoNewline "  [VBP-BUILD-FAIL] asm_neg_accum_alias_x86 ... "
        $result = & cmd /c ('"' + $C3 + '" "' + "$Tests\asm\asm_alias_x86_neg.vbp" + '" --arch x86 --output-dir "' + $OutDir + '" 2>&1')
        $text = (($result | Out-String) -replace '\s+', ' ')
        if ($text -match "3042") { $script:passed++; Write-Host "PASS" -ForegroundColor Green }
        else { $script:failed++; Write-Host "FAIL (expected 3042)" -ForegroundColor Red }
    }
    if (Test-Path "$Tests\asm\asm_width_neg.vbp") {
        # 宽度不一致 (mov rax, edx) → 3038 (宽度校验前移, 不再漏到 ml64 的 A2022)
        Test-VbpBuildFail "asm_neg_operand_width" "$Tests\asm\asm_width_neg.vbp" "3038"
    }
    if (Test-Path "$Tests\asm\asm_cls_neg.vbp") {
        # 类方法里的 Asm 块 → 3037 (v2 边界: 只支持标准模块过程)
        Test-VbpBuildFail "asm_neg_class_method" "$Tests\asm\asm_cls_neg.vbp" "3037"
    }
    if (Test-Path "$Tests\asm\asm_attr_neg.vbp") {
        Test-VbpBuildFail "asm_neg_naked_on_nonproc" "$Tests\asm\asm_attr_neg.vbp" "2012"
        Test-VbpBuildFail "asm_neg_clobber_nonstring" "$Tests\asm\asm_attr_neg.vbp" "2014"
    }
    if (Test-Path "$Tests\asm\asm_x86_neg.vbp") {
        # x86 负例需要额外 --arch x86, Test-VbpBuildFail 不带自定义参数, 就地内联同款判据
        $script:total++
        Write-Host -NoNewline "  [VBP-BUILD-FAIL] asm_x86_neg ... "
        $result = & cmd /c ('"' + $C3 + '" "' + "$Tests\asm\asm_x86_neg.vbp" + '" --arch x86 --output-dir "' + $OutDir + '" 2>&1')
        $text = (($result | Out-String) -replace '\s+', ' ')
        if ($LASTEXITCODE -ne 0 -and $text.Contains("3036")) {
            $script:pass++
            Write-Host "PASS" -ForegroundColor Green
        } else {
            $script:fail++
            Write-Host "FAIL" -ForegroundColor Red
            Write-Host "  expected failing build containing: 3036" -ForegroundColor DarkGray
            if ($Verbose) { Write-Host $text }
        }
    }
    Write-Host ""
}

# =============================================
# 若需调试单用例, 可设置 \$Verbose 后调用 Test-Run/Test-Compile/Test-Gui 子函数
# =============================================
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Results: PASS=$script:pass FAIL=$script:fail SKIP=$script:skip TOTAL=$script:total" -ForegroundColor $(if ($script:fail -gt 0) { "Red" } else { "Green" })
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

if ($script:fail -gt 0) { exit 1 } else { exit 0 }
