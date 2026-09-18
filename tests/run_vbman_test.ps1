# =====================================================================
# run_vbman_test.ps1 - vbman 运行期冒烟测试 (C3 产物跑起来会不会挂)
#
# 定位: 与 regress_vbman.ps1 互补, 两者回答不同问题 --
#   regress_vbman.ps1 : 编译期对照 (C3 vs 真 VB6) -> "能不能编出来"
#   run_vbman_test.ps1: 运行期冒烟 (只有 C3)      -> "编出来的东西跑起来会不会挂"
# VBMAN.vbp 是 Type=OleDll, 自身没有独立运行阶段, 所以运行期必须借宿主:
#   注册 C3 编出的 VBMAN.dll -> 宿主 EXE 在进程内加载并调用 -> 看是否崩溃/挂起。
# 架构必须分开: x86 产物只能用 SysWOW64 的注册器 + x86 宿主; x64 反之。
#
# 为什么默认宿主是 tests\vbman_host 而不是 tests\test_vbman:
#   test_vbman 是 P24-04 的早绑定专项用例 (VBMAN.Version() 走 GlobalNameSpace),
#   当前 C3 把它生成成对不存在符号 vb6_VBMAN_Version 的调用, 链接期
#   error LNK2019 —— 宿主根本编不出来, 做不了运行期测试 (已作为独立缺陷记录)。
#   tests\vbman_host 只用晚绑定 CreateObject, 不引用任何类型库, 因此不依赖
#   编译期类型库解析, 纯粹测"运行时加载 C3 产物会不会挂"。
#   要换回 test_vbman 跑: -HostVbp tests\test_vbman\test_vbman.vbp -Expect P24-04:2/2
#
# 用法 (在 tests 目录):
#   .\run_vbman_test.ps1                    # x86 + x64 都跑
#   .\run_vbman_test.ps1 -Arch x86          # 只跑 32 位
#   .\run_vbman_test.ps1 -BuildOnly         # 只编译产物, 不注册不运行
#   .\run_vbman_test.ps1 -NoElevate         # 已在管理员会话时跳过自提权
#   .\run_vbman_test.ps1 -KeepWork          # 保留工作目录
#   .\run_vbman_test.ps1 -List              # 只列目标, 不执行
#   .\run_vbman_test.ps1 -Arch x86 -LibDll tests\vbman\dist\DLL\VBMAN.dll
#                                           # 对照基线: 拿现成的(真 VB6 产的) DLL
#                                           # 跑同一条运行期路径, 用来区分"C3 缺陷"
#                                           # 和"环境/宿主问题"
#
# 环境变量 (脚本内不写死任何绝对路径, 全部可覆盖):
#   C3_EXE              C3.exe 路径 (缺省按仓库相对路径找 .build / publish)
#   C3_VBMAN_RUNROOT    工作目录 (缺省 tests\_vb6ref_run)
#   C3_VBMAN_LIB_VBP    库工程   (缺省 tests\vbman\src\VBMAN.vbp)
#   C3_VBMAN_HOST_VBP   宿主工程 (缺省 tests\vbman_host\vbman_host.vbp)
#   C3_VCVARSALL        vcvarsall.bat 路径 (缺省 vswhere 自动探测)
#
# 判定矩阵:
#   PASS        : 编译 + 注册 + 运行全通, 退出码 0 且宿主断言通过
#   BUILD-FAIL  : C3 编译库或宿主失败
#   ARCH-DIFF   : C3 产物的 PE 架构与 -Arch 不符 (C3 静默降级时会被这里挡住)
#   REG-FAIL    : 注册 DLL 失败 (regsvr32 退出码非 0)
#   HANG        : 运行超时未退出              <-- 重点怀疑死锁/挂起
#   CRASH       : 进程异常退出 (退出码为 NTSTATUS 异常码)
#   NO-OUTPUT   : 退出码 0 但宿主无输出       <-- 入口没跑起来
#   ASSERT-FAIL : 有输出但断言不符
#   UNREG-FAIL  : 注销失败或注销后有残留 (系统洁净被破坏)
#   SCRIPT-ERROR: 脚本自身抛异常 (Note 里带异常消息)
#
# 系统洁净 (项目根 CLAUDE.md 的约定):
#   1) 注册进系统的 DLL 在 finally 里必注销, 无论成败; 注销后复核 ProgID 已消失。
#   2) 所有产物/日志只落工作目录 (tests\_vb6ref_run, 已在 .gitignore), 不碰系统
#      目录、不碰源码树 (C3 走 --output-dir 隔离, 宿主在副本里编译)。
#   3) 注册/注销要管理员权限, 故默认自提权 (单次 UAC); 用 -NoElevate 关闭。
#
# 输出:
#   控制台 = 逐架构判定 + 汇总; 日志落 <工作目录>\run-<时间戳>\
#     <架构>\lib.c3\c3_stderr.log      库编译的 VB error/warning
#     <架构>\lib.c3\c3-error.log       进入 cl 阶段后的 cl.exe 错误明细
#     <架构>\host.c3\*                 宿主编译日志
#     <架构>\host.stdout.txt           宿主运行输出 (断言依据)
#     <架构>\host.stderr.txt           宿主运行 stderr
#   自提权时父进程看不到子控制台, 子进程会写 <工作目录>\elevated-<token>.log.txt 回传。
#   注意: 实测本机(Windows + PowerShell 5.1)提权子进程跑完脚本后**不退出**, 因此
#   父进程不以"子进程退出"为准, 改认 <工作目录>\elevated-<token>.done.txt 这个
#   sentinel, 拿到后主动把残留的提权进程收掉 —— 否则脚本会永久卡在等待上。
# =====================================================================

param(
    [ValidateSet("x86", "x64")]
    [string[]]$Arch = @("x86", "x64"),
    [string]$C3 = "",                     # C3.exe 路径
    [string]$LibVbp = "",                 # 库工程 vbp
    [string]$LibDll = "",                 # 直接用现成 DLL (跳过编译); 跑对照基线用, 架构须匹配 -Arch
    [string]$HostVbp = "",                # 宿主工程 vbp
    [string]$WorkRoot = "",               # 工作目录
    [string[]]$Expect = @("vbman-host:ok=3"),  # 宿主 stdout 必须全部命中的正则 (空数组 = 只判崩溃)
    [int]$BuildTimeoutSec = 1800,         # 单次编译超时 (vbman 129 模块, 放宽)
    [int]$RunTimeoutSec = 60,             # 宿主运行超时
    [switch]$BuildOnly,                   # 只编译, 不注册不运行
    [switch]$NoElevate,                   # 不自提权
    [switch]$KeepWork,                    # 保留工作目录
    [string]$Token = "",                  # 内部用: 自提权会话标识 (决定 sentinel 文件名)
    [switch]$Elevated,                    # 内部用: 自提权后由父进程传入
    [switch]$List                         # 只列目标
)

$ErrorActionPreference = "SilentlyContinue"

# === 路径 (全部可被参数/环境变量覆盖, 脚本内无绝对路径字面量) ===
$Tests = $PSScriptRoot
$Root  = Split-Path -Parent $Tests

if (-not $LibVbp)   { $LibVbp   = $env:C3_VBMAN_LIB_VBP }
if (-not $LibVbp)   { $LibVbp   = Join-Path $Tests "vbman\src\VBMAN.vbp" }
if (-not $HostVbp)  { $HostVbp  = $env:C3_VBMAN_HOST_VBP }
if (-not $HostVbp)  { $HostVbp  = Join-Path $Tests "vbman_host\vbman_host.vbp" }
if (-not $WorkRoot) { $WorkRoot = $env:C3_VBMAN_RUNROOT }
if (-not $WorkRoot) { $WorkRoot = Join-Path $Tests "_vb6ref_run" }
if (-not $C3)       { $C3       = $env:C3_EXE }
if (-not $C3) {
    foreach ($c in @((Join-Path $Root ".build\C3.exe"), (Join-Path $Root "publish\C3.exe"))) {
        if (Test-Path $c) { $C3 = $c; break }
    }
}
$HostSrcDir = Split-Path -Parent $HostVbp

# === 目标校验 + -List (静态枚举, 不校验环境, 不提权) ===
foreach ($f in @($LibVbp, $HostVbp)) {
    if (-not (Test-Path $f)) {
        Write-Host "[ERROR] 未找到工程: $f" -ForegroundColor Red
        Write-Host "        vbman 子模块缺失时先执行 git submodule update --init --recursive" -ForegroundColor Red
        exit 1
    }
}

if ($List) {
    Write-Host "vbman 运行期冒烟目标:"
    Write-Host ("  库工程  : {0}" -f $LibVbp)
    Write-Host ("  宿主工程: {0}" -f $HostVbp)
    Write-Host ("  架构    : {0}" -f ($Arch -join ", "))
    Write-Host ("  断言    : {0}" -f $(if ($Expect -and $Expect.Count -gt 0) { $Expect -join ' | ' } else { "(空, 只判崩溃)" }))
    Write-Host ("  C3      : {0}" -f $(if ($C3) { $C3 } else { "(未找到)" }))
    Write-Host ("  工作目录: {0}" -f $WorkRoot)
    exit 0
}

if (-not $C3 -or -not (Test-Path $C3)) {
    Write-Host "[ERROR] 未找到 C3.exe, 请用 -C3 或环境变量 C3_EXE 指定" -ForegroundColor Red; exit 1
}

# === 自提权 ===
# 注册 COM DLL 要写 HKLM\Software\Classes, 非管理员必然失败。所以默认把整个脚本
# 重启到管理员上下文, 只弹一次 UAC; 已在管理员会话时不弹。父进程看不到子进程的
# 控制台, 故子进程用 Start-Transcript 落盘, 由父进程回读打印。
function Test-Admin {
    try {
        $id = [Security.Principal.WindowsIdentity]::GetCurrent()
        return (New-Object Security.Principal.WindowsPrincipal($id)).IsInRole(
            [Security.Principal.WindowsBuiltInRole]::Administrator)
    } catch { return $false }
}

# 每次自提权用**独立的** sentinel / transcript 文件名 (带本次调用的时间戳 token):
# 固定文件名会在上一次运行的文件还在时被误读成"本轮已完成", 而且删除动作本身可能
# 被外部守卫拦截 (实测: 受限会话里的批量删除守卫会静默拒绝 Remove-Item 多路径)。
# 用 token 后既无陈旧误判, 也不需要任何删除前置动作。
if (-not $Token) { $Token = Get-Date -Format "yyyyMMdd-HHmmssfff" }
$script:TranscriptPath = Join-Path $WorkRoot "elevated-$Token.log.txt"
$script:DonePath       = Join-Path $WorkRoot "elevated-$Token.done.txt"
if (-not (Test-Path $WorkRoot)) { New-Item -ItemType Directory -Force -Path $WorkRoot | Out-Null }

# 收尾出口: 提权子进程必须先落 sentinel 再退出。
# 实测(本机 Win10/PS5.1): 提权子进程跑完 Stop-Transcript 之后进程仍不退出,
# 于是父进程若用 Start-Process -Wait 会永久卡住。所以父进程不以"进程退出"为准,
# 只认 sentinel 文件; 拿到 sentinel 后主动把残留子进程收掉。
function Exit-With {
    param([int]$Code)
    if ($Elevated) {
        Stop-Transcript -ErrorAction SilentlyContinue | Out-Null
        Set-Content -Path $script:DonePath -Value $Code -Encoding ASCII -ErrorAction SilentlyContinue
    }
    exit $Code
}

if ($Elevated) {
    Start-Transcript -Path $script:TranscriptPath -Force | Out-Null
} elseif (-not $BuildOnly -and -not (Test-Admin)) {
    if ($NoElevate) {
        Write-Host "[WARN] 当前非管理员且 -NoElevate, 注册 DLL 很可能失败" -ForegroundColor Yellow
    } else {
        # 原样回放本次命令行, 末尾补 -Token / -Elevated。数组参数按逗号串传, 避免被
        # 拆成多个位置参数 (PowerShell 会把 "x86,x64" 正确绑到 [string[]])。
        $re = @()
        foreach ($k in $PSBoundParameters.Keys) {
            if ($k -eq "Elevated" -or $k -eq "Token") { continue }
            $v = $PSBoundParameters[$k]
            if ($v -is [switch])   { if ($v.IsPresent) { $re += "-$k" } }
            elseif ($v -is [array]) { $re += @("-$k", ($v -join ",")) }
            else                    { $re += @("-$k", "$v") }
        }
        $re += @("-Token", $Token, "-Elevated")
        Write-Host "注册 COM DLL 需要管理员权限, 正在自提权 (会弹一次 UAC)..." -ForegroundColor Yellow
        $p = $null
        try {
            $p = Start-Process -FilePath "powershell.exe" -Verb RunAs -PassThru `
                -ArgumentList (@("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $PSCommandPath) + $re)
        } catch {
            Write-Host "[ERROR] 提权失败 (UAC 被拒绝?): $($_.Exception.Message)" -ForegroundColor Red
            exit 3
        }
        # 等 sentinel; 子进程若提前退出(无 sentinel)则立即收工, 不必空等
        $deadline = (Get-Date).AddSeconds(2 * $BuildTimeoutSec + 900)
        while ((Get-Date) -lt $deadline) {
            if (Test-Path $script:DonePath) { break }
            if ($p -and $p.HasExited) { Start-Sleep -Milliseconds 500; break }
            Start-Sleep -Milliseconds 500
        }
        $code = 3
        if (Test-Path $script:DonePath) {
            $raw = (Get-Content $script:DonePath -Raw -ErrorAction SilentlyContinue)
            if ($raw) { [void][int]::TryParse($raw.Trim(), [ref]$code) }
        }
        if (Test-Path $script:TranscriptPath) {
            Get-Content $script:TranscriptPath -Encoding UTF8 | Write-Host
        } else {
            Write-Host "[WARN] 提权子进程未产出日志, 可能 UAC 被拒绝" -ForegroundColor Yellow
        }
        if ($p) { try { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } } catch {} }
        exit $code
    }
}

# === 工具函数 ===
# 读 vbp 的键值 (vbp 是 GBK, 与 regress_all.ps1 / regress_vbman.ps1 同一套约定)
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

# 把 vbp 里指向某个类型库的 Reference 路径改到指定 DLL
# 形如 Reference=*\G{GUID}#2.0#0#<路径>#<类型库名>, 按 '#' 切后倒数第 2 段是路径。
# C3 侧 driver_compile.cpp 会先按 GUID 查注册表、失败再回退用这个路径,
# 所以"先注册本脚本编出的 DLL, 再编宿主"能保证两边用的是同一份产物。
function Set-VbpLibReference {
    param([string]$VbpPath, [string]$LibName, [string]$DllPath)
    $enc   = [Text.Encoding]::Default
    $lines = [IO.File]::ReadAllLines($VbpPath, $enc)
    $hit   = 0
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -notmatch '^\s*Reference=') { continue }
        $parts = $lines[$i].Split('#')
        if ($parts.Count -lt 5) { continue }
        if ($parts[$parts.Count - 1].Trim() -ne $LibName) { continue }
        $parts[$parts.Count - 2] = $DllPath
        $lines[$i] = ($parts -join '#')
        $hit++
    }
    if ($hit -gt 0) { [IO.File]::WriteAllLines($VbpPath, $lines, $enc) }
    return $hit
}

# 带超时运行进程, 返回 @{ ExitCode; Stdout; Stderr; TimedOut; Err }
# 注意 1: 参数名不能用 $Args (PowerShell 自动变量)
# 注意 2: C3 的诊断(VB error/warning)走 **stderr**, 程序运行输出才走 stdout, 两个流都要收;
#         读写用 ReadToEndAsync 并行进行, 避免单流缓冲区写满造成死锁。
function Invoke-Proc {
    param([string]$Exe, [string[]]$ArgList, [string]$Cwd, [int]$TimeoutSec, [switch]$Redirect)
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $Exe
    if ($ArgList) {
        # 仅含空格/引号的参数才加引号
        $psi.Arguments = ($ArgList | ForEach-Object {
            if ($_ -match '[\s"]') { '"{0}"' -f $_ } else { $_ }
        }) -join " "
    }
    $psi.WorkingDirectory = $Cwd
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = [bool]$Redirect
    $psi.RedirectStandardError  = [bool]$Redirect
    # C3 的诊断是 UTF-8 字节流; 不显式指定的话 .NET 按系统 ANSI(中文机 = GBK) 解码,
    # 落盘后中文全成乱码。宿主的断言串是 ASCII, 用 UTF-8 解码同样正确。
    if ($Redirect) {
        $psi.StandardOutputEncoding = [Text.Encoding]::UTF8
        $psi.StandardErrorEncoding  = [Text.Encoding]::UTF8
    }
    $psi.CreateNoWindow = $true
    # 显式下发 MSVC 环境 (含 PATH), 不依赖进程环境继承
    if ($script:MsvcEnv -and $script:MsvcEnv.Count -gt 0) {
        foreach ($kv in $script:MsvcEnv.GetEnumerator()) {
            $psi.EnvironmentVariables[$kv.Key] = $kv.Value
        }
    }
    $p = $null; $startErr = ""
    try { $p = [System.Diagnostics.Process]::Start($psi) } catch { $startErr = $_.Exception.Message }
    if (-not $p) { return @{ ExitCode = -99; Stdout = ""; Stderr = ""; TimedOut = $false; Err = $startErr } }

    $outTask = $null; $errTask = $null
    if ($Redirect) {
        $outTask = $p.StandardOutput.ReadToEndAsync()
        $errTask = $p.StandardError.ReadToEndAsync()
    }
    if (-not $p.WaitForExit($TimeoutSec * 1000)) {
        try { $p.Kill() } catch {}
        $p.WaitForExit(5000) | Out-Null
        return @{ ExitCode = -1; Stdout = ""; Stderr = ""; TimedOut = $true; Err = "" }
    }
    $so = ""; $se = ""
    if ($Redirect) {
        try { $so = $outTask.Result } catch { $so = "" }
        try { $se = $errTask.Result } catch { $se = "" }
    }
    $ec = -1
    try { $ec = $p.ExitCode } catch { $ec = -1 }
    return @{ ExitCode = $ec; Stdout = $so; Stderr = $se; TimedOut = $false; Err = "" }
}

# 在目录下找产物: 优先精确名, 其次任意 dll/exe (取最新)
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

# 读 PE 头里的 machine 字段, 校验产物架构与 -Arch 一致
# 0x014C = i386, 0x8664 = x64。C3 若静默降级成 32 位, 这一步能挡住,
# 否则会拿 x86 产物去配 x64 注册器, 得到"注册失败"这种误导性结论。
function Get-PeMachine {
    param([string]$Path)
    try {
        $fs = [IO.File]::OpenRead($Path)
        try {
            $br = New-Object IO.BinaryReader($fs)
            $fs.Position = 0x3C
            $peOff = $br.ReadInt32()
            if ($peOff -le 0 -or $peOff -gt ($fs.Length - 6)) { return 0 }
            $fs.Position = $peOff + 4
            return $br.ReadUInt16()
        } finally { $fs.Close() }
    } catch { return 0 }
}

# C3 编译一个 vbp, 产物与日志落隔离输出目录 (不污染源树)
function Invoke-C3Build {
    param([string]$VbpAbs, [string]$OutDir, [string]$Want, [string]$TargetArch, [switch]$AsDll)
    New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
    $outLog = Join-Path $OutDir "c3_stdout.log"
    $errLog = Join-Path $OutDir "c3_stderr.log"
    $argList = @($VbpAbs)
    if ($AsDll) { $argList += "--dll" }
    $argList += @("--arch", $TargetArch, "--output-dir", $OutDir)

    $r = Invoke-Proc -Exe $C3 -ArgList $argList -Cwd $OutDir -TimeoutSec $BuildTimeoutSec -Redirect
    $enc = New-Object Text.UTF8Encoding $true
    [IO.File]::WriteAllText($outLog, [string]$r.Stdout, $enc)
    [IO.File]::WriteAllText($errLog, [string]$r.Stderr, $enc)

    $warn = 0
    foreach ($f in @($errLog, $outLog)) {
        if (Test-Path $f) { $warn += (Select-String -Path $f -Pattern "warning VB" -AllMatches | Measure-Object).Count }
    }
    # MSVC 报错形如 "error C2065: ..." / "error LNK2019: ..." (中文版 MSVC 也只把
    # 消息正文汉化, 关键字仍是英文), 两种都要数
    $clErrLog = Join-Path $OutDir "c3-error.log"
    $clErr = 0
    $firstClErr = ""
    if (Test-Path $clErrLog) {
        $m = Select-String -Path $clErrLog -Pattern "error (C|LNK)\d+" -AllMatches
        $clErr = ($m | Measure-Object).Count
        if ($m) { $firstClErr = $m[0].Line.Trim() }
    }

    $res = @{ Ok = $false; Product = ""; Warn = $warn; ClErr = $clErr; Note = "" }
    if ($r.Err)      { $res.Note = "无法启动 C3: $($r.Err)";         return $res }
    if ($r.TimedOut) { $res.Note = "编译超时 (${BuildTimeoutSec}s)"; return $res }
    if ($r.ExitCode -ne 0) {
        # 优先摘 cl/link 的具体错误 (c3-error.log), 没有再退回 C3 前端的 VB 错误
        $first = $firstClErr
        if (-not $first -and (Test-Path $errLog)) {
            $m = Select-String -Path $errLog -Pattern "error VB\d+" -AllMatches | Select-Object -First 1
            if ($m) { $first = $m.Line.Trim() }
        }
        $res.Note = "exit=$($r.ExitCode)"
        if ($first) { $res.Note += " | $first" }
        return $res
    }
    $p = Find-Product -Dir $OutDir -Want $Want
    if (-not $p) { $res.Note = "无产物"; return $res }
    $res.Ok = $true; $res.Product = $p
    return $res
}

# 按架构取注册器路径。要点: 64 位系统上 32 位进程里的 "System32" 会被重定向到
# SysWOW64, 所以 32 位 PowerShell 要拿 64 位注册器必须走 Sysnative。
function Get-RegSvr32 {
    param([string]$TargetArch)
    if ($TargetArch -eq "x64") {
        if ([Environment]::Is64BitProcess) { return (Join-Path (Join-Path $env:windir "System32") "regsvr32.exe") }
        return (Join-Path (Join-Path $env:windir "Sysnative") "regsvr32.exe")
    }
    return (Join-Path (Join-Path $env:windir "SysWOW64") "regsvr32.exe")
}

# 把 .NET 给的 Int32 退出码格式化成无符号十六进制。
# 注意不能写 [uint32]($ec -band 0xFFFFFFFF): PowerShell 5.1 把 0xFFFFFFFF 解析成
# Int32 的 -1, 负数 -band -1 之后还是负数, 再转 [uint32] 会抛"值太大或太小"
# (实测把 CRASH 判定打成了 SCRIPT-ERROR)。
function Format-ExitCode {
    param([int]$Code)
    if ($Code -lt 0) { return ("0x{0:X8}" -f [int64]($Code + 4294967296)) }
    return ("0x{0:X8}" -f [int64]$Code)
}

# 常见 NTSTATUS 异常码 -> 人话, 让 CRASH 判定直接可读
function Get-ExitMeaning {
    param([string]$Hex)
    switch ($Hex.ToUpper()) {
        "0XC0000005" { return "ACCESS_VIOLATION 访问违规" }
        "0XC0000374" { return "HEAP_CORRUPTION 堆损坏" }
        "0XC00000FD" { return "STACK_OVERFLOW 栈溢出(疑无限递归)" }
        "0XC0000409" { return "STACK_BUFFER_OVERRUN" }
        "0XC000013A" { return "CONTROL_C_EXIT 被结束" }
        default      { return "" }
    }
}

# 注册/注销 COM DLL, 返回 @{ Ok; Code; Note }
function Invoke-RegSvr {
    param([string]$Dll, [string]$TargetArch, [switch]$Unregister)
    $exe = Get-RegSvr32 -TargetArch $TargetArch
    if (-not (Test-Path $exe)) { return @{ Ok = $false; Code = -1; Note = "未找到注册器: $exe" } }
    $argList = @()
    if ($Unregister) { $argList += "/u" }
    $argList += @("/s", $Dll)
    $r = Invoke-Proc -Exe $exe -ArgList $argList -Cwd (Split-Path -Parent $Dll) -TimeoutSec 120
    $ok = ($r.ExitCode -eq 0)
    $note = ""
    if (-not $ok) {
        if ($r.TimedOut) { $note = "注册器超时" }
        else { $note = ("注册器退出码 " + (Format-ExitCode $r.ExitCode)) }
    }
    return @{ Ok = $ok; Code = $r.ExitCode; Note = $note }
}

# 注册残留复核: 取 HKCR 下所有 VBMAN 系 ProgID。注销后必须为空。
# 直接读 ClassesRoot.GetSubKeyNames() 是内存态枚举, 比 Get-ChildItem 逐键快得多。
function Get-VbmanProgIdKeys {
    $out = @()
    try {
        $names = [Microsoft.Win32.Registry]::ClassesRoot.GetSubKeyNames()
        foreach ($n in $names) {
            if ($n -like "VBMANLIB.*" -or $n -like "VBMAN.*") { $out += $n }
        }
    } catch {}
    return ,$out
}

# 抑制子进程崩溃时的 WER 弹窗: 子进程默认继承本进程的 error mode, 设上
# SEM_NOGPFAULTERRORBOX 后崩溃进程会直接退出而不是弹窗等待 —— 否则"崩溃"会被
# 误判成"挂起"(超时)。SetErrorMode 不可用时退化为靠运行超时兜底。
try {
    Add-Type -Namespace C3T -Name ErrMode -MemberDefinition @'
[DllImport("kernel32.dll")] public static extern uint SetErrorMode(uint uMode);
'@
    [C3T.ErrMode]::SetErrorMode(0x0001 -bor 0x0002 -bor 0x8000) | Out-Null
} catch {}

# === 单架构测试 (函数化: 内部可以放心用 return, 不会终止整个脚本) ===
function Invoke-ArchTest {
    param([string]$TargetArch, [string]$ArchDir)

    $r = @{ Arch = $TargetArch; Verdict = ""; Note = ""; LibSize = ""; HostSize = ""
            ExitCode = ""; Stdout = "" }
    New-Item -ItemType Directory -Force -Path $ArchDir | Out-Null

    # --- 1. 取库产物: 默认用 C3 现编; 给 -LibDll 则直接用现成 DLL (跑对照基线用) ---
    $libPath = ""
    if ($LibDll) {
        if (-not (Test-Path $LibDll)) {
            $r.Verdict = "BUILD-FAIL"; $r.Note = "-LibDll 不存在: $LibDll"
            Write-Host ("[$TargetArch] BUILD-FAIL  {0}" -f $r.Note) -ForegroundColor Red
            return $r
        }
        $libPath = (Resolve-Path $LibDll).Path
        Write-Host "[$TargetArch] 跳过编译, 使用现成 DLL: $libPath" -ForegroundColor Cyan
    } else {
        Write-Host "[$TargetArch] 编译库工程..." -ForegroundColor Cyan
        $libB = Invoke-C3Build -VbpAbs $LibVbp -OutDir (Join-Path $ArchDir "lib.c3") `
            -Want $script:LibWant -TargetArch $TargetArch -AsDll:$script:LibAsDll
        if (-not $libB.Ok) {
            $r.Verdict = "BUILD-FAIL"; $r.Note = "库: $($libB.Note)"
            Write-Host ("[$TargetArch] BUILD-FAIL  库编译失败: {0}" -f $libB.Note) -ForegroundColor Red
            return $r
        }
        $libPath = $libB.Product
    }
    $r.LibSize = (Get-Item $libPath).Length
    $machine  = Get-PeMachine $libPath
    $wantMach = 0x014C; if ($TargetArch -eq "x64") { $wantMach = 0x8664 }
    if ($machine -ne $wantMach) {
        $r.Verdict = "ARCH-DIFF"
        $r.Note = ("产物 machine=0x{0:X4}, 期望 0x{1:X4}" -f $machine, $wantMach)
        Write-Host ("[$TargetArch] ARCH-DIFF  {0}" -f $r.Note) -ForegroundColor Red
        return $r
    }
    Write-Host ("[$TargetArch] 库产物 OK: {0} ({1:N0} 字节, machine=0x{2:X4})" -f `
        (Split-Path -Leaf $libPath), $r.LibSize, $machine) -ForegroundColor Green

    if ($BuildOnly) {
        $r.Verdict = "BUILD-OK"; $r.Note = "-BuildOnly (未注册未运行)"
        Write-Host ("[$TargetArch] BUILD-OK  {0}" -f $r.Note) -ForegroundColor Green
        return $r
    }

    # --- 2. 注册 -> 编宿主 -> 运行 -> 注销; 注销放 finally, 无论中途怎么失败都执行 ---
    $reg = $null; $ran = $null
    try {
        Write-Host "[$TargetArch] 注册 DLL..." -ForegroundColor Cyan
        $reg = Invoke-RegSvr -Dll $libPath -TargetArch $TargetArch
        if (-not $reg.Ok) {
            $r.Verdict = "REG-FAIL"; $r.Note = "注册失败: $($reg.Note)"
            Write-Host ("[$TargetArch] REG-FAIL  {0}" -f $r.Note) -ForegroundColor Red
            return $r
        }

        # 宿主在副本里编译, 并把 VBMANLIB 的类型库引用改指到刚编出的 DLL
        # 变量名用 $stageDir: PowerShell 变量名大小写不敏感, 若写成 $hostDir 会与
        # 脚本级的 $HostSrcDir 撞名(改名前是 $HostDir), 函数内赋值会遮蔽脚本变量,
        # 导致源目录和目标目录变成同一个值, 复制静默失败。
        $stageDir = Join-Path $ArchDir "host"
        if (Test-Path $stageDir) { Remove-Item $stageDir -Recurse -Force -ErrorAction SilentlyContinue }
        New-Item -ItemType Directory -Force -Path $stageDir | Out-Null
        Copy-Item (Join-Path $HostSrcDir "*") $stageDir -Recurse -Force
        $hostVbpCopy = Join-Path $stageDir (Split-Path -Leaf $HostVbp)
        if (-not (Test-Path $hostVbpCopy)) {
            $r.Verdict = "BUILD-FAIL"; $r.Note = "宿主副本未生成: $hostVbpCopy"
            Write-Host ("[$TargetArch] BUILD-FAIL  {0}" -f $r.Note) -ForegroundColor Red
            return $r
        }
        $hit = Set-VbpLibReference -VbpPath $hostVbpCopy -LibName "VBMANLIB" -DllPath $libPath
        if ($hit -eq 0) {
            # 宿主不声明 VBMANLIB 类型库引用是**合法**的: 纯晚绑定宿主只用
            # CreateObject, 运行期靠注册表解析 ProgID, 编译期不需要类型库。
            # 默认宿主 tests\vbman_host 就是这种; 早绑定宿主(如 tests\test_vbman)
            # 才会命中 Reference, 此时必须改写到 C3 产物, 否则会编到旧 DLL 上。
            Write-Host "[$TargetArch] 宿主未声明 VBMANLIB 类型库引用 (晚绑定), 跳过路径改写" -ForegroundColor DarkGray
        }

        Write-Host "[$TargetArch] 编译宿主..." -ForegroundColor Cyan
        $hostB = Invoke-C3Build -VbpAbs $hostVbpCopy -OutDir (Join-Path $ArchDir "host.c3") `
            -Want $script:HostWant -TargetArch $TargetArch
        if (-not $hostB.Ok) {
            $r.Verdict = "BUILD-FAIL"; $r.Note = "宿主: $($hostB.Note)"
            Write-Host ("[$TargetArch] BUILD-FAIL  宿主编译失败: {0}" -f $hostB.Note) -ForegroundColor Red
            return $r
        }
        $r.HostSize = (Get-Item $hostB.Product).Length

        Write-Host "[$TargetArch] 运行宿主..." -ForegroundColor Cyan
        $ran = Invoke-Proc -Exe $hostB.Product -ArgList @() -Cwd (Split-Path -Parent $hostB.Product) `
            -TimeoutSec $RunTimeoutSec -Redirect
        $r.Stdout = [string]$ran.Stdout
        $enc = New-Object Text.UTF8Encoding $true
        [IO.File]::WriteAllText((Join-Path $ArchDir "host.stdout.txt"), $r.Stdout, $enc)
        [IO.File]::WriteAllText((Join-Path $ArchDir "host.stderr.txt"), [string]$ran.Stderr, $enc)
        $r.ExitCode = Format-ExitCode $ran.ExitCode
        # 崩溃/挂起点定位: 宿主每步都 Debug.Print 一行, 最后一行就是"活着走到的最后一步"
        $lastLine = ""
        foreach ($ln in ([string]$r.Stdout -split "`r?`n")) {
            if ($ln.Trim() -ne "") { $lastLine = $ln.Trim() }
        }

        if ($ran.TimedOut) {
            $r.Verdict = "HANG"; $r.Note = "运行 ${RunTimeoutSec}s 未退出 (疑似挂起/死锁)"
            if ($lastLine) { $r.Note += " | 卡前最后输出: $lastLine" }
        } elseif ($ran.ExitCode -lt 0) {
            $meaning = Get-ExitMeaning $r.ExitCode
            $r.Verdict = "CRASH"; $r.Note = "异常退出 $($r.ExitCode)"
            if ($meaning)  { $r.Note += " ($meaning)" }
            if ($lastLine) { $r.Note += " | 崩前最后输出: $lastLine" }
        } elseif ([string]::IsNullOrWhiteSpace($r.Stdout)) {
            $r.Verdict = "NO-OUTPUT"; $r.Note = "退出码 0 但无 stdout (入口未执行?)"
        } else {
            $miss = @()
            foreach ($pat in $Expect) {
                if ($pat -and ($r.Stdout -notmatch $pat)) { $miss += $pat }
            }
            if ($miss.Count -gt 0) {
                $r.Verdict = "ASSERT-FAIL"; $r.Note = "stdout 未命中: $($miss -join ' | ')"
            } else {
                $r.Verdict = "PASS"; $r.Note = "退出码 0"
                if ($Expect.Count -gt 0) { $r.Note += ", 命中 $($Expect.Count) 条断言" }
            }
        }
    } finally {
        if ($reg -and $reg.Ok) {
            Write-Host "[$TargetArch] 注销 DLL (系统洁净)..." -ForegroundColor Cyan
            $unreg = Invoke-RegSvr -Dll $libPath -TargetArch $TargetArch -Unregister
            if (-not $unreg.Ok) {
                if ($r.Verdict -eq "PASS") { $r.Verdict = "UNREG-FAIL" }
                $r.Note += " | 注销失败: $($unreg.Note)"
                Write-Host ("[$TargetArch] 注销失败: {0}" -f $unreg.Note) -ForegroundColor Red
            } else {
                $left = Get-VbmanProgIdKeys
                if ($left.Count -gt 0) {
                    if ($r.Verdict -eq "PASS") { $r.Verdict = "UNREG-FAIL" }
                    $r.Note += " | 注销后仍有残留 ProgID: $($left -join ',')"
                    Write-Host ("[$TargetArch] 注销后仍有残留: {0}" -f ($left -join ',')) -ForegroundColor Yellow
                }
            }
        }
    }
    return $r
}

# === MSVC 环境 (与 regress_all.ps1 / regress_vbman.ps1 同一套约定) ===
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
            if (-not $script:MsvcEnv.ContainsKey($name) -or $script:MsvcEnv[$name].Length -lt $matches[2].Length) {
                $script:MsvcEnv[$name] = $matches[2]
            }
            [Environment]::SetEnvironmentVariable($name, $script:MsvcEnv[$name], "Process")
        }
    }
} elseif (-not $env:INCLUDE) {
    Write-Host "[WARN] 无 C3_VCVARSALL 且未探测到 VS, 若 C3 编译失败请先设置 (同 regress_all.ps1)" -ForegroundColor Yellow
}

# === 主流程 ===
$script:LibType  = Get-VbpSetting -VbpPath $LibVbp -Key "Type"
$script:LibWant  = Get-VbpSetting -VbpPath $LibVbp -Key "ExeName32"
$script:HostWant = Get-VbpSetting -VbpPath $HostVbp -Key "ExeName32"
$script:LibAsDll = ($script:LibType -match '(?i)dll')
# vbp 缺 ExeName32 时按 VB6 的默认规则补: 产物名 = 工程文件名 (如 test_vbman.vbp -> test_vbman.exe)
if (-not $script:LibWant) {
    $script:LibWant = [IO.Path]::GetFileNameWithoutExtension($LibVbp) + $(if ($script:LibAsDll) { ".dll" } else { ".exe" })
}
if (-not $script:HostWant) { $script:HostWant = [IO.Path]::GetFileNameWithoutExtension($HostVbp) + ".exe" }

$stamp  = Get-Date -Format "yyyyMMdd-HHmmss"
$RunDir = Join-Path $WorkRoot "run-$stamp"

Write-Host ""
Write-Host "=== vbman 运行期冒烟 (C3 产物) ===" -ForegroundColor Cyan
Write-Host "  C3  : $C3"
Write-Host ("  库  : {0}  (Type={1}, 产物={2})" -f $LibVbp, $script:LibType, $script:LibWant)
if ($LibDll) { Write-Host ("  现成: {0}  (跳过编译)" -f $LibDll) -ForegroundColor DarkGray }
Write-Host ("  宿主: {0}  (产物={1})" -f $HostVbp, $script:HostWant)
Write-Host "  架构: $($Arch -join ', ')   运行超时: ${RunTimeoutSec}s"
if ($Expect -and $Expect.Count -gt 0) {
    Write-Host ("  断言: {0}" -f ($Expect -join ' | '))
} else {
    Write-Host "  断言: (空, 只判崩溃/挂起)" -ForegroundColor DarkGray
}
Write-Host ("  MSVC: INCLUDE={0} cl-in-PATH={1}" -f [bool]$env:INCLUDE, ($script:MsvcEnv["Path"] -match "Hostx64"))
if ($BuildOnly) { Write-Host "  模式: -BuildOnly (只编译, 不注册不运行)" -ForegroundColor DarkGray }
Write-Host ""

$rows = @()
$bad  = 0
foreach ($a in $Arch) {
    # 函数内部的 finally 已保证注销先执行; 这里只兜住异常, 免得汇总表出现空行
    $res = $null
    try {
        $res = Invoke-ArchTest -TargetArch $a -ArchDir (Join-Path $RunDir $a)
    } catch {
        # 带上出错行号: 脚本内部错误如果不指明位置, 只能靠猜
        $where = ""
        if ($_.InvocationInfo) {
            $where = (" (L{0}: {1})" -f $_.InvocationInfo.ScriptLineNumber,
                      ([string]$_.InvocationInfo.Line).Trim())
        }
        $res = @{ Arch = $a; Verdict = "SCRIPT-ERROR"; LibSize = ""; HostSize = ""; ExitCode = ""
                  Stdout = ""; Note = "脚本内部错误: $($_.Exception.Message)$where" }
        Write-Host ("[$a] SCRIPT-ERROR  {0}" -f $res.Note) -ForegroundColor Red
    }
    if (-not $res) {
        $res = @{ Arch = $a; Verdict = "SCRIPT-ERROR"; LibSize = ""; HostSize = ""; ExitCode = ""
                  Stdout = ""; Note = "未返回结果" }
    }
    $rows += [pscustomobject]@{
        Arch = $res.Arch; Verdict = $res.Verdict; ExitCode = $res.ExitCode
        LibSize = $res.LibSize; HostSize = $res.HostSize; Note = $res.Note
    }
    if ($res.Verdict -ne "PASS" -and $res.Verdict -ne "BUILD-OK") { $bad++ }
    if ($res.Stdout) {
        foreach ($ln in ($res.Stdout -split "`r?`n" | Where-Object { $_.Trim() -ne "" })) {
            Write-Host ("       | {0}" -f $ln) -ForegroundColor DarkGray
        }
    }
    Write-Host ""
}

# === 系统洁净终检: 任何残留 ProgID 都说明注销没做干净 ===
$finalLeft = Get-VbmanProgIdKeys
if ($finalLeft.Count -gt 0) {
    Write-Host ("[WARN] 系统里仍存在 VBMAN 注册残留: {0}" -f ($finalLeft -join ',')) -ForegroundColor Yellow
    Write-Host "       手动注销: regsvr32 /u /s <库产物路径>  (x86 用 %windir%\SysWOW64 下的那个)" -ForegroundColor Yellow
    $bad++
}

# === 汇总 ===
Write-Host "=== 汇总 ===" -ForegroundColor Cyan
$rows | Format-Table -AutoSize Arch, Verdict, ExitCode, LibSize, HostSize, Note |
    Out-String -Width 200 | Write-Host
Write-Host "  工作目录: $RunDir"

if ($bad -eq 0) {
    if (-not $KeepWork) { Remove-Item $RunDir -Recurse -Force -ErrorAction SilentlyContinue }
    Exit-With 0
}

Write-Host ""
Write-Host "!! $bad 个架构未达 PASS, 工作目录已保留:" -ForegroundColor Red
Write-Host "   $RunDir" -ForegroundColor Red
Write-Host "   库诊断  : <架构>\lib.c3\c3_stderr.log (VB error/warning)" -ForegroundColor DarkGray
Write-Host "   cl 明细 : <架构>\lib.c3\c3-error.log  (进入 cl 阶段才有)" -ForegroundColor DarkGray
Write-Host "   运行输出: <架构>\host.stdout.txt / host.stderr.txt" -ForegroundColor DarkGray
Exit-With 2
