# =====================================================================
# regress_vbman.ps1 - vbman 真实工程对照回归 (只测 vbman)
#
# 定位: regress_all.ps1 覆盖 tests 下 117 个用例但**排除 vbman**; 本脚本是
#       它的补集 -- 只对 tests\vbman 的权威工程 (src\VBMAN.vbp, Type=OleDll,
#       129 模块) 做 C3 vs 本机真 VB6 (VB6Mini /make) 的对照编译与产物比对。
#
# 用法 (在 tests 目录):
#   .\regress_vbman.ps1                       # 默认只测 VBMAN.vbp (x86)
#   .\regress_vbman.ps1 -List                 # 只列出目标, 不执行
#   .\regress_vbman.ps1 -SyntaxOnly           # 只跑 C3 前端 (秒级, 不调 cl)
#   .\regress_vbman.ps1 -CompileOnly          # 只比编译, 不比导出表
#   .\regress_vbman.ps1 -Target VBMAN_EXE.vbp # 换目标 (src 下的 vbp 文件名)
#   .\regress_vbman.ps1 -KeepWork             # 保留工作目录 (失败时默认保留)
#
# 环境: 与 regress_all.ps1 同一套约定 --
#   C3_VCVARSALL 优先, 否则 vswhere 自动探测 vcvarsall;
#   C3_EXE / VB6_EXE 可覆盖 C3.exe / VB6.EXE 路径;
#   C3_VBMAN_WORKROOT 可覆盖工作目录 (默认 tests\_vb6ref_vbman)。
#
# 判定矩阵 (OleDll 无运行阶段, 用导出表比对替代运行比对):
#   PASS         : 双方编译 OK, 且 VB6 的导出符号被 C3 全覆盖
#   REG-C3(红)   : 真 VB6 能编译, C3 失败          <-- 重点怀疑回归
#   C3-EXT(蓝)   : C3 能编译, 真 VB6 不支持
#   BOTH-FAIL    : 双方都编译失败
#   EXPORT-DIFF  : 双方编译 OK 但 C3 缺 VB6 的导出符号
#   DLL-MISSING  : 编译报成功但未产出产物
#
# 与 docs\vbman\001_first_compile.md 手工流程的差异 (本脚本的改进):
#   VB6 /make 会改写 src\VBMAN.vbp 的 Reference 绝对路径并覆盖 dist\ 下产物,
#   导致子模块工作区变脏 (需 git checkout 恢复)。本脚本把 VB6 侧放到**副本**
#   里编译, 原工作区不再被污染; C3 侧走 --output-dir 隔离, 同样不落盘到源树。
#
# 输出:
#   控制台 = 逐目标判定 + 汇总; CSV 报告落 <工作目录>\report-<时间戳>.csv
#   未达 PASS 时保留 <工作目录>\run-<时间戳>\, 内含:
#     <目标>.c3\c3_stderr.log      C3 的 VB error/warning (C3 诊断走 stderr)
#     <目标>.c3\c3-error.log       cl.exe 错误明细 (进入 cl 阶段才有)
#     <目标>.vb6\vb6_make_out.log  VB6 /make 的输出
# =====================================================================

param(
    [string[]]$Target = @("VBMAN.vbp"),   # src 下的 vbp 文件名, 可多个
    [ValidateSet("x86", "x64")]
    [string]$Arch = "x86",                # VB6 产物恒为 32 位, 对照用 x86
    [string]$C3 = "",                     # C3.exe 路径, 默认自动找 .build / publish
    [string]$VB6 = "",                    # VB6.EXE 路径, 默认自动找 VB6Mini
    [string]$WorkRoot = "",               # 工作目录, 默认 tests\_vb6ref_vbman
    [int]$BuildTimeoutSec = 1200,         # 单次编译超时 (vbman 129 模块, 放宽)
    [switch]$SyntaxOnly,                  # 只跑 C3 前端, 不调 cl / 不用 VB6
    [switch]$CompileOnly,                 # 只比编译, 不比导出表
    [switch]$KeepWork,                    # 保留工作目录
    [switch]$List                         # 只列出目标
)

$ErrorActionPreference = "SilentlyContinue"

# === 路径 ===
$Tests    = $PSScriptRoot
$Root     = Split-Path -Parent $Tests
$VbmanDir = Join-Path $Tests "vbman"
$VbmanSrc = Join-Path $VbmanDir "src"

if (-not (Test-Path $VbmanSrc)) {
    Write-Host "[ERROR] 未找到 vbman 源码目录: $VbmanSrc" -ForegroundColor Red
    Write-Host "        先执行 git submodule update --init --recursive" -ForegroundColor Red
    exit 1
}

if (-not $WorkRoot) { $WorkRoot = $env:C3_VBMAN_WORKROOT }
if (-not $WorkRoot) { $WorkRoot = Join-Path $Tests "_vb6ref_vbman" }

if (-not $C3) { $C3 = $env:C3_EXE }      # 环境变量优先, 其次按仓库相对路径推导
if (-not $C3) {
    foreach ($c in @((Join-Path $Root ".build\C3.exe"), (Join-Path $Root "publish\C3.exe"))) {
        if (Test-Path $c) { $C3 = $c; break }
    }
}
if (-not $C3 -or -not (Test-Path $C3)) {
    Write-Host "[ERROR] 未找到 C3.exe, 请用 -C3 或环境变量 C3_EXE 指定" -ForegroundColor Red; exit 1
}

# 读取 vbp 的键值 (vbp 是 GBK, 与 regress_all.ps1 一致用 Default 编码)
# 定义在 -List 之前: PowerShell 脚本自上而下执行, 函数须先定义后调用
function Get-VbpSetting {
    param([string]$VbpPath, [string]$Key)
    $lines = [IO.File]::ReadAllLines($VbpPath, [Text.Encoding]::Default)
    foreach ($ln in $lines) {
        if ($ln -match ("^\s*" + [regex]::Escape($Key) + "\s*=\s*(.+?)\s*$")) {
            return $matches[1].Trim('"')
        }
    }
    return ""
}

# 探测 VB6.EXE: 显式参数 > 环境变量 VB6_EXE > VB6Mini 默认安装位置
# 找不到返回空串 (由调用方决定是报错还是降级), 不在此处 exit
function Find-VB6Path {
    if ($VB6) { return $VB6 }
    if ($env:VB6_EXE) { return $env:VB6_EXE }
    foreach ($base in @(${env:ProgramFiles(x86)}, $env:ProgramFiles)) {
        if (-not $base) { continue }   # 受限会话下 ProgramFiles 可能为空, 空串会让 Join-Path 抛错
        $cand = Join-Path $base "VB6Mini\bin\VB6.EXE"
        if (Test-Path $cand) { return $cand }
    }
    return ""
}

# === 目标校验 ===
$targets = @()
foreach ($t in $Target) {
    $p = Join-Path $VbmanSrc $t
    if (-not (Test-Path $p)) {
        Write-Host "[ERROR] 目标不存在: $p" -ForegroundColor Red; exit 1
    }
    $targets += $p
}

# -List 只做静态枚举, 不校验 VB6 / MSVC 环境 (与 regress_all.ps1 的 -List 同口径)
if ($List) {
    Write-Host "vbman 对照目标 ($($targets.Count) 个):"
    foreach ($t in $targets) {
        $ty = Get-VbpSetting -VbpPath $t -Key "Type"
        $ex = Get-VbpSetting -VbpPath $t -Key "ExeName32"
        Write-Host ("  {0,-28} Type={1,-8} 产物={2}" -f (Split-Path -Leaf $t), $ty, $ex)
    }
    Write-Host "  C3 : $C3"
    $vb6Probe = Find-VB6Path
    if ($vb6Probe) { Write-Host "  VB6: $vb6Probe" } else { Write-Host "  VB6: (未探测到, 正式运行需 -VB6 或 VB6_EXE)" }
    Write-Host "  工作目录: $WorkRoot"
    exit 0
}

# 正式运行才要求 VB6 (-SyntaxOnly 模式不用 VB6)
if (-not $SyntaxOnly) {
    $VB6 = Find-VB6Path
    if (-not $VB6 -or -not (Test-Path $VB6)) {
        Write-Host "[ERROR] 未找到 VB6.EXE (VB6Mini), 请用 -VB6 或环境变量 VB6_EXE 指定" -ForegroundColor Red
        Write-Host "        只跑 C3 前端可用 -SyntaxOnly" -ForegroundColor Red
        exit 1
    }
}

# 架构守卫: 真 VB6 只有 32 位编译器, 产物恒为 32 位。若拿 x64 跑对照, 就是
# C3 的 64 位产物去比 VB6 的 32 位产物 —— 而**导出名不受架构影响**, 会得到
# 误导性的 PASS。故对照模式只允许 x86。
# 单纯验证 C3 能否以 x64 编译 vbman: 直接调
#   .build\C3.exe <vbp> --dll --arch x64 --output-dir <隔离目录>
if ($Arch -ne "x86" -and -not $SyntaxOnly) {
    Write-Host "[ERROR] 本脚本是 C3 vs 真 VB6 的对照回归, 而 VB6 只能产出 32 位 (无 x64 编译器)。" -ForegroundColor Red
    Write-Host "        -Arch $Arch 会让 C3(64 位) 与 VB6(32 位) 做无效比对 (导出名不受架构影响, 会误判 PASS)。" -ForegroundColor Red
    Write-Host "        只跑 C3 前端可用 -SyntaxOnly; 只验证 x64 能否编译请直接调 C3.exe --dll --arch x64。" -ForegroundColor Red
    exit 1
}

# === 工具函数 ===
# 带超时运行进程, 返回 @{ ExitCode; Stdout; Stderr; TimedOut; Err }
# 注意 1: 参数名不能用 $Args (PowerShell 自动变量)
# 注意 2: C3 的诊断(VB 错误/警告)走 **stderr**, 程序运行输出才走 stdout, 故两个流都要收;
#         读写用 ReadToEndAsync 并行进行, 避免单流缓冲区写满造成死锁。
function Invoke-Proc {
    param([string]$Exe, [string[]]$ArgList, [string]$Cwd, [int]$TimeoutSec, [switch]$Redirect, [switch]$ShellExecute)
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $Exe
    if ($ArgList) {
        # 仅含空格/引号的参数才加引号 (VB6.EXE 的 "/make" 带引号会不被识别)
        $psi.Arguments = ($ArgList | ForEach-Object {
            if ($_ -match '[\s"]') { '"{0}"' -f $_ } else { $_ }
        }) -join " "
    }
    $psi.WorkingDirectory = $Cwd
    # VB6.EXE 清单要求管理员, UseShellExecute=false 会报"需要提升", 这类进程走 ShellExecute
    $psi.UseShellExecute = [bool]$ShellExecute
    $redirOut = ([bool]$Redirect -and -not $ShellExecute)
    $psi.RedirectStandardOutput = $redirOut
    $psi.RedirectStandardError  = $redirOut
    $psi.CreateNoWindow = -not $ShellExecute
    # 显式下发 MSVC 环境 (含 PATH), 不依赖进程环境继承 (仅 UseShellExecute=false 支持)
    if (-not $ShellExecute -and $script:MsvcEnv -and $script:MsvcEnv.Count -gt 0) {
        foreach ($kv in $script:MsvcEnv.GetEnumerator()) {
            $psi.EnvironmentVariables[$kv.Key] = $kv.Value
        }
    }
    $p = $null; $startErr = ""
    try {
        $p = [System.Diagnostics.Process]::Start($psi)
    } catch {
        $startErr = $_.Exception.Message
    }
    if (-not $p) {
        return @{ ExitCode = -99; Stdout = ""; Stderr = ""; TimedOut = $false; Err = $startErr }
    }
    $outTask = $null; $errTask = $null
    if ($redirOut) {
        $outTask = $p.StandardOutput.ReadToEndAsync()
        $errTask = $p.StandardError.ReadToEndAsync()
    }
    if (-not $p.WaitForExit($TimeoutSec * 1000)) {
        try { $p.Kill() } catch {}
        $p.WaitForExit(5000) | Out-Null
        return @{ ExitCode = -1; Stdout = ""; Stderr = ""; TimedOut = $true; Err = "" }
    }
    $so = ""; $se = ""
    if ($redirOut) {
        try { $so = $outTask.Result } catch { $so = "" }
        try { $se = $errTask.Result } catch { $se = "" }
    }
    # ShellExecute 模式下 ExitCode 可能取不到, 调用方不依赖它 (以产物存在为准)
    $ec = -1
    try { $ec = $p.ExitCode } catch { $ec = -1 }
    return @{ ExitCode = $ec; Stdout = $so; Stderr = $se; TimedOut = $false; Err = "" }
}

# 在目录下找产物: 优先精确名, 其次任意 dll/exe
function Find-Product {
    param([string]$Dir, [string]$Want)
    if (-not (Test-Path $Dir)) { return "" }
    if ($Want) {
        $p = Join-Path $Dir $Want
        if (Test-Path $p) { return (Resolve-Path $p).Path }
        $f = Get-ChildItem -Path $Dir -Recurse -Filter $Want -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($f) { return $f.FullName }
    }
    $any = Get-ChildItem -Path $Dir -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Extension -eq ".dll" -or $_.Extension -eq ".exe" } |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if ($any) { return $any.FullName }
    return ""
}

# 读取 DLL 的导出符号名 (需 dumpbin)
# 返回用 `return ,$names` 抑制展开, 调用方须**直接赋值**(不能再套 @(), 否则会嵌套成单元素数组)
function Get-DllExports {
    param([string]$DllPath)
    if (-not $script:DumpBin -or -not (Test-Path $DllPath)) { return ,@() }
    $out = & $script:DumpBin /exports $DllPath 2>$null
    $names = @()
    foreach ($ln in $out) {
        # 形如: "     1    0 0006B17E DllCanUnloadNow"
        if ($ln -match '^\s*\d+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]{8}\s+(\S+)\s*$') { $names += $matches[1] }
    }
    return ,$names
}

# C3 编译: 直接编原始 vbp, 产物与日志落隔离输出目录 (不污染源树)
# 日志: c3_stdout.log = C3 的 stdout; c3_stderr.log = C3 的诊断(VB error/warning);
#       c3-error.log   = 进入 cl 阶段后由 C3 落盘的 cl.exe 错误明细
function Invoke-C3Build {
    param([string]$VbpAbs, [string]$OutDir, [string]$Want, [switch]$AsDll, [switch]$FrontOnly)
    New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
    $outLog = Join-Path $OutDir "c3_stdout.log"
    $errLog = Join-Path $OutDir "c3_stderr.log"
    $argList = @($VbpAbs)
    if ($FrontOnly) {
        $argList += "--syntax-only"
    } else {
        if ($AsDll) { $argList += "--dll" }
        $argList += @("--arch", $Arch)
    }
    $argList += @("--output-dir", $OutDir)

    $r = Invoke-Proc -Exe $C3 -ArgList $argList -Cwd $OutDir -TimeoutSec $BuildTimeoutSec -Redirect
    $enc = New-Object Text.UTF8Encoding $true
    [IO.File]::WriteAllText($outLog, [string]$r.Stdout, $enc)
    [IO.File]::WriteAllText($errLog, [string]$r.Stderr, $enc)

    # 诊断走 stderr, 但两个流都数一遍以防版本差异
    $warn = 0
    foreach ($f in @($errLog, $outLog)) {
        if (Test-Path $f) {
            $warn += (Select-String -Path $f -Pattern "warning VB" -AllMatches | Measure-Object).Count
        }
    }
    $clErrLog = Join-Path $OutDir "c3-error.log"
    $clErr = 0
    if (Test-Path $clErrLog) {
        $clErr = (Select-String -Path $clErrLog -Pattern "error C\d+" -AllMatches | Measure-Object).Count
    }
    $res = @{ Ok = $false; Product = ""; Warn = $warn; ClErr = $clErr; Note = "" }
    if ($r.Err)       { $res.Note = "无法启动 C3: $($r.Err)";    return $res }
    if ($r.TimedOut)  { $res.Note = "编译超时 (${BuildTimeoutSec}s)"; return $res }
    if ($r.ExitCode -ne 0) {
        # 摘一句首条 VB 错误, 便于直接看出失败原因
        $first = ""
        if (Test-Path $errLog) {
            $m = Select-String -Path $errLog -Pattern "error VB\d+" -AllMatches | Select-Object -First 1
            if ($m) { $first = $m.Line.Trim() }
        }
        $res.Note = "exit=$($r.ExitCode)"
        if ($first) { $res.Note += " | $first" }
        return $res
    }
    if ($FrontOnly)   { $res.Ok = $true; return $res }
    $p = Find-Product -Dir $OutDir -Want $Want
    if (-not $p)      { $res.Note = "无产物";                    return $res }
    $res.Ok = $true; $res.Product = $p
    return $res
}

# 真 VB6 编译: 在 tests\vbman 的**副本**里跑 /make
# (原树不被改写, 因此不需要 docs\vbman\001 里那条 git checkout 恢复步骤)
function Invoke-VB6Build {
    param([string]$Fixture, [string]$RelVbp, [string]$VbpDirRel, [string]$Want, [string]$Path32)
    $logAbs = Join-Path $Fixture "vb6_make_out.log"
    $r = Invoke-Proc -Exe $VB6 -ArgList @("/make", $RelVbp, "/out", $logAbs) -Cwd $Fixture `
        -TimeoutSec $BuildTimeoutSec -ShellExecute

    $res = @{ Ok = $false; Product = ""; Note = "" }
    if ($r.Err)      { $res.Note = "无法启动 VB6.EXE: $($r.Err)"; return $res }
    if ($r.TimedOut) { $res.Note = "编译超时 (${BuildTimeoutSec}s)"; return $res }

    # 产物位置 = <vbp 所在目录>\<Path32>\<ExeName32>
    $vbpDir = Join-Path $Fixture $VbpDirRel
    foreach ($c in @(
        $(if ($Want -and $Path32) { Join-Path $vbpDir (Join-Path $Path32 $Want) }),
        $(if ($Want)             { Join-Path $vbpDir $Want })
    )) {
        if ($c -and (Test-Path $c)) { $res.Ok = $true; $res.Product = (Resolve-Path $c).Path; return $res }
    }
    $p = Find-Product -Dir $Fixture -Want $Want
    if ($p) { $res.Ok = $true; $res.Product = $p; return $res }

    # 无产物: 从 /out 日志里摘一句原因
    $why = "无产物"
    if (Test-Path $logAbs) {
        $logText = [IO.File]::ReadAllText($logAbs, [Text.Encoding]::Default)
        if ($logText -match '([^\r\n]*(?:错误|error|无法|不能|失败)[^\r\n]*)') { $why = $matches[1].Trim() }
    }
    $res.Note = $why
    return $res
}

# === MSVC 环境 (与 regress_all.ps1 / run_tests.ps1 同一套约定) ===
# 除设置进程环境外, 还把变量存进 $script:MsvcEnv, 由 Invoke-Proc 显式传给子进程 --
# 显式传 env 可规避个别机器上进程环境块存在重复 PATH 条目 (PATH/Path) 导致
# 子进程解析到旧值的问题 (实测 cl.exe 找不到)。
# 不弹窗口: 这里不再用 `cmd /c ... | Out-File` (那会闪 cmd 窗口), 改为经 Invoke-Proc
# 以 CreateNoWindow=true 跑 cmd.exe 并回读 stdout。
# 注意: C3 自身 shell-out 调 cl.exe/link.exe/rc.exe 时未加 CREATE_NO_WINDOW
#       (src/backend/msvc_driver.cpp executeCommand 的 creation flags = 0), 那部分
#       窗口本脚本管不到, 属 C3 侧待修项。
$script:MsvcEnv = @{}
$VcVars = $env:C3_VCVARSALL
if (-not $VcVars) {
    $pf86 = ${env:ProgramFiles(x86)}
    $vswhere = ""
    if ($pf86) { $vswhere = Join-Path $pf86 "Microsoft Visual Studio\Installer\vswhere.exe" }
    if ($vswhere -and (Test-Path $vswhere)) {
        $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
        if ($vsPath) {
            $cand = Join-Path $vsPath "VC\Auxiliary\Build\vcvarsall.bat"
            if (Test-Path $cand) { $VcVars = $cand }
        }
    }
}
if ($VcVars -and (Test-Path $VcVars)) {
    if (-not (Test-Path $WorkRoot)) { New-Item -ItemType Directory -Path $WorkRoot | Out-Null }
    # 用临时 bat 承载 call+set: 规避 ProcessStartInfo.Arguments 里嵌套引号的转义地狱
    $tmpBat = Join-Path $WorkRoot "c3_vcvars_export.bat"
    [IO.File]::WriteAllText($tmpBat,
        "@echo off`r`ncall `"$VcVars`" x64 >nul 2>&1`r`nset`r`n", [Text.Encoding]::ASCII)
    $r = Invoke-Proc -Exe "cmd.exe" -ArgList @("/c", $tmpBat) -Cwd $WorkRoot -TimeoutSec 300 -Redirect
    Remove-Item $tmpBat -ErrorAction SilentlyContinue
    foreach ($ln in ([string]$r.Stdout -split "`r?`n")) {
        if ($ln -match '^([^=]+)=(.*)$') {
            $name = $matches[1]
            if ($name -ieq "path") { $name = "Path" }   # 统一大小写, 防止重复条目
            # 个别机器环境块同时存在 PATH/Path 两条, 同名时保留更长(vcvars 展开后)的那份
            if (-not $script:MsvcEnv.ContainsKey($name) -or $script:MsvcEnv[$name].Length -lt $matches[2].Length) {
                $script:MsvcEnv[$name] = $matches[2]
            }
            [Environment]::SetEnvironmentVariable($name, $script:MsvcEnv[$name], "Process")
        }
    }
} elseif (-not $env:INCLUDE) {
    Write-Host "[WARN] 无 C3_VCVARSALL 且未探测到 VS, 若 C3 编译失败请先设置 (同 regress_all.ps1)" -ForegroundColor Yellow
}
Write-Host ("  MSVC 环境: INCLUDE={0} cl-in-PATH={1}" -f [bool]$env:INCLUDE, ($script:MsvcEnv["Path"] -match "Hostx64"))

# dumpbin 用于导出表比对; 缺失时降级为"只比编译"
$script:DumpBin = ""
$dbCmd = Get-Command dumpbin.exe -ErrorAction SilentlyContinue
if ($dbCmd) { $script:DumpBin = $dbCmd.Source }
if (-not $CompileOnly -and -not $SyntaxOnly -and -not $script:DumpBin) {
    Write-Host "[WARN] 未找到 dumpbin, 跳过导出表比对 (只比编译)" -ForegroundColor Yellow
}

# === 主循环 ===
if (-not (Test-Path $WorkRoot)) { New-Item -ItemType Directory -Path $WorkRoot | Out-Null }
$stamp   = Get-Date -Format "yyyyMMdd-HHmmss"
$RunDir  = Join-Path $WorkRoot "run-$stamp"
$csvPath = Join-Path $WorkRoot "report-$stamp.csv"
$rows = @()

# CSV 增量写入: 全量编译较长, 中断时已跑部分的结果也保留
$script:csvInit = $false
function Add-Result {
    param([hashtable]$Row)
    $script:rows += [pscustomobject]$Row
    if (-not $script:csvInit) {
        $script:rows | Export-Csv $script:csvPath -NoTypeInformation -Encoding UTF8
        $script:csvInit = $true
    } else {
        $script:rows | Select-Object -Last 1 | Export-Csv $script:csvPath -NoTypeInformation -Encoding UTF8 -Append
    }
}

Write-Host ""
Write-Host "=== vbman 对照回归 (C3 vs 真 VB6) ===" -ForegroundColor Cyan
Write-Host "  C3 : $C3"
if (-not $SyntaxOnly) { Write-Host "  VB6: $VB6" }
Write-Host "  目标: $($targets.Count) 个  架构: $Arch  超时: ${BuildTimeoutSec}s"
if ($SyntaxOnly) { Write-Host "  模式: -SyntaxOnly (只跑 C3 前端)" -ForegroundColor DarkGray }
Write-Host ""

$n = 0
$bad = 0
foreach ($vbpAbs in $targets) {
    $n++
    $tag  = "[$n/$($targets.Count)]"
    $name = [IO.Path]::GetFileNameWithoutExtension($vbpAbs)

    $type    = Get-VbpSetting -VbpPath $vbpAbs -Key "Type"
    $want    = Get-VbpSetting -VbpPath $vbpAbs -Key "ExeName32"
    $path32  = Get-VbpSetting -VbpPath $vbpAbs -Key "Path32"
    $asDll   = ($type -match '(?i)dll')
    $relVbp  = Join-Path "src" ([IO.Path]::GetFileName($vbpAbs))

    $c3Out   = Join-Path $RunDir "$name.c3"
    $fixture = Join-Path $RunDir "$name.vb6"

    $c3b = Invoke-C3Build -VbpAbs $vbpAbs -OutDir $c3Out -Want $want -AsDll:$asDll -FrontOnly:$SyntaxOnly
    $c3c = "OK"; if (-not $c3b.Ok) { $c3c = "FAIL" }

    # --- 只跑前端: 判定只看 C3 退出码 ---
    # 注: --syntax-only 通过时不打印任何内容 (实测 stdout/stderr 均 0 字节), 故不报 warning 数
    if ($SyntaxOnly) {
        $verdict = "FRONTEND-FAIL"; $note = $c3b.Note
        $color = "Red"
        if ($c3b.Ok) { $verdict = "FRONTEND-OK"; $note = "前端无错误 (--syntax-only 静默)"; $color = "Green" }
        Write-Host ("{0} {1,-16} {2}  ({3})" -f $tag, $name, $verdict, $note) -ForegroundColor $color
        Add-Result @{ Case = $name; Type = $type; C3Compile = $c3c; VB6Compile = "SKIP";
            C3Warn = $c3b.Warn; C3ClErr = $c3b.ClErr; C3Size = ""; VB6Size = "";
            ExportsMissing = ""; Verdict = $verdict; Note = $note }
        if (-not $c3b.Ok) { $bad++ }
        continue
    }

    # --- 真 VB6: 复制 src + dist 到副本目录后编译 ---
    if (Test-Path $fixture) { Remove-Item $fixture -Recurse -Force -ErrorAction SilentlyContinue }
    New-Item -ItemType Directory -Force -Path $fixture | Out-Null
    foreach ($sub in @("src", "dist")) {
        $s = Join-Path $VbmanDir $sub
        if (Test-Path $s) { Copy-Item $s (Join-Path $fixture $sub) -Recurse -Force }
    }
    $vb6b = Invoke-VB6Build -Fixture $fixture -RelVbp $relVbp -VbpDirRel "src" -Want $want -Path32 $path32
    $v6c = "OK"; if (-not $vb6b.Ok) { $v6c = "FAIL" }

    $verdict = ""; $note = ""
    $missing = @()
    $c3Size = ""; $v6Size = ""

    if (-not $c3b.Ok -and -not $vb6b.Ok) {
        $verdict = "BOTH-FAIL"; $note = "C3:$($c3b.Note) | VB6:$($vb6b.Note)"
        Write-Host ("{0} {1,-16} BOTH-FAIL" -f $tag, $name) -ForegroundColor DarkYellow
    } elseif (-not $c3b.Ok -and $vb6b.Ok) {
        $verdict = "REG-C3"; $note = "真VB6可编译, C3失败: $($c3b.Note)"
        Write-Host ("{0} {1,-16} REG-C3  <-- 可疑回归" -f $tag, $name) -ForegroundColor Red
    } elseif ($c3b.Ok -and -not $vb6b.Ok) {
        $verdict = "C3-EXT"; $note = "C3可编译, 真VB6不支持: $($vb6b.Note)"
        Write-Host ("{0} {1,-16} C3-EXT" -f $tag, $name) -ForegroundColor Blue
    } else {
        # 双方编译 OK -> 比对产物与导出表
        $c3Size = (Get-Item $c3b.Product).Length
        $v6Size = (Get-Item $vb6b.Product).Length
        if (-not $CompileOnly -and $script:DumpBin) {
            $expV6 = Get-DllExports $vb6b.Product   # 直接赋值, 勿套 @() (见函数注释)
            $expC3 = Get-DllExports $c3b.Product
            $missing = @($expV6 | Where-Object { $expC3 -notcontains $_ })
            $extra   = @($expC3 | Where-Object { $expV6 -notcontains $_ })
            if ($missing.Count -gt 0) {
                $verdict = "EXPORT-DIFF"; $note = "C3 缺导出: $($missing -join ',')"
                Write-Host ("{0} {1,-16} EXPORT-DIFF  缺: {2}" -f $tag, $name, ($missing -join ',')) -ForegroundColor Red
            } else {
                $verdict = "PASS"
                $note = "导出 $($expV6.Count) 个全覆盖"
                if ($extra.Count -gt 0) { $note += " (C3 多导: $($extra -join ','))" }
                Write-Host ("{0} {1,-16} PASS  {2}" -f $tag, $name, $note) -ForegroundColor Green
            }
        } else {
            $verdict = "COMPILE-PASS"
            if ($CompileOnly) { $note = "-CompileOnly, 未比导出表" } else { $note = "dumpbin 不可用, 未比导出表" }
            Write-Host ("{0} {1,-16} COMPILE-PASS" -f $tag, $name) -ForegroundColor Green
        }
    }
    if ($verdict -ne "PASS" -and $verdict -ne "COMPILE-PASS") { $bad++ }

    Add-Result @{ Case = $name; Type = $type; C3Compile = $c3c; VB6Compile = $v6c;
        C3Warn = $c3b.Warn; C3ClErr = $c3b.ClErr; C3Size = $c3Size; VB6Size = $v6Size;
        ExportsMissing = ($missing -join ','); Verdict = $verdict; Note = $note }
}

# === 汇总 ===
Write-Host ""
Write-Host "=== 汇总 ===" -ForegroundColor Cyan
$rows | Group-Object Verdict | Sort-Object Count -Descending | ForEach-Object {
    Write-Host ("  {0,-16} {1}" -f $_.Name, $_.Count)
}
Write-Host ""
Write-Host "报告: $csvPath"

if ($bad -eq 0) {
    if (-not $KeepWork) { Remove-Item $RunDir -Recurse -Force -ErrorAction SilentlyContinue }
    exit 0
}

Write-Host ""
Write-Host "!! $bad 个目标未达 PASS, 工作目录已保留:" -ForegroundColor Red
Write-Host "   $RunDir" -ForegroundColor Red
Write-Host "   C3 诊断: <目标>.c3\c3_stderr.log (VB error/warning)" -ForegroundColor DarkGray
Write-Host "   cl 明细: <目标>.c3\c3-error.log (进入 cl 阶段才有)" -ForegroundColor DarkGray
exit 2
