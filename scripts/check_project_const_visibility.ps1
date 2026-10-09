# check_project_const_visibility.ps1 - census: 工程级"这名字工程里有"只有一处问、三份名单都从 AST 来（账 #278 §B106）
#
# 起因（2026-10-09，探针 `.build/b909_probe` 实测）：标准模块的模块级 `Public Const` 从来没进
# 工程级名字表 —— 那张表逐条 `switch (d->kind)` 只收 `SubDecl / FunctionDecl / PropertyDecl`。
# 后果不是"产物坏了"而是相反：发码把跨模块 Public Const **折成了字面量**（`v = (1 + 42);`），
# 而语义层因为认不得这个名字，Option Explicit 下配一条 VB3001、宽松模式下更实 —— 会把它
# 登记成一枚**隐式 Variant 局部**，把常量本身吃掉。VB6 里 Public Const 与 Public 过程同格：工程级裸名可见。
#
# 本哨兵钉两头，缺一头都会红：
#   V1 正例放行：一枚 `Public Const` + 一枚 `Public Enum` 的两个成员，被另一个 `.bas` 与一个 `.cls`
#      按裸名用 ⇒ 产物里必须是**折好的字面量**，且 diagnostics 里不许出现这几个名字的 VB3001。
#   V2 反面证人：同一个工程里再放一枚**真不存在**的名字 ⇒ 必须**仍然**报 VB3001。
#      （只钉 V1 的哨兵会替"干脆什么都不报"背书 —— 那是同一个洞的另一种坏法。）
#   S1 单一建造点：三份工程级名单（模块名 / Public 过程 / Public Const+Enum 成员）各自只在
#      `driver_semantics.cpp` 里被 insert；语义层只有一个消费点 `namesProjectLevel`。
#   S2 建造点与消费点不许换家：`setProject*Names` 三个 setter 各恰好 1 处声明 + 1 处调用。
#
# 输出 ASCII（控制台是 GBK，中文读数重定向后不可 grep）。文件必须 UTF-8 BOM + CRLF：
# PS 5.1 读无 BOM 的 .ps1 按 ANSI，行尾中文字节会吃掉换行 ⇒ ParserError 而退出码仍 0。
#
# 用法: powershell -File scripts\check_project_const_visibility.ps1   (PASS = exit 0)

param(
    [string]$Root = (Split-Path -Parent $PSScriptRoot),
    [string]$Exe = ""      # 负控用: 拿另一枚 C3.exe 跑同一份夹具
)
$ErrorActionPreference = "Stop"
$root = $Root
$exe = if ($Exe) { $Exe } else { Join-Path $root ".build\C3.exe" }
if (-not (Test-Path -LiteralPath $exe)) {
    Write-Host ("FAIL missing " + $exe + "  (先跑 scripts/build.bat 产出 C3.exe)")
    exit 1
}

$bad = @()
$work = Join-Path $env:TEMP ("c3_projconst_" + [Guid]::NewGuid().ToString("N").Substring(0, 8))
New-Item -ItemType Directory -Path $work -Force | Out-Null

try {
    # ---------- V1/V2 的夹具：一份标准模块放常量与枚举，另一份 .bas 与一份 .cls 按裸名用 ----------
    $modA = @(
        'Attribute VB_Name = "PcvConstants"',
        'Option Explicit',
        '',
        'Public Const PCV_ANSWER As Long = 42',
        'Public Enum PCV_MODE',
        '    PCV_OFF = 0',
        '    PCV_ON = 3',
        'End Enum',
        ''
    )
    $modB = @(
        'Attribute VB_Name = "PcvUser"',
        'Option Explicit',
        '',
        'Public Function SumIt() As Long',
        '    SumIt = PCV_ANSWER + PCV_ON - PCV_OFF',
        'End Function',
        ''
    )
    $clsC = @(
        'VERSION 1.0 CLASS',
        'BEGIN',
        '  MultiUse = -1  ',
        'END',
        'Attribute VB_Name = "PcvCls"',
        'Option Explicit',
        '',
        'Public Function Peek() As Long',
        '    Dim v As Long',
        '    v = PCV_ANSWER',
        '    Peek = v + PCV_OFF',
        '    Dim bogus As Long',
        '    bogus = pcvNoSuchNameAnywhere',   ' ',
        # V2 的反面证人: 下面这枚名字工程里真没有, 必须继续报 VB3001
        'End Function',
        ''
    )
    $vbp = @(
        'Type=Exe',
        'Module=PcvConstants; PcvConstants.bas',
        'Module=PcvUser; PcvUser.bas',
        'Class=PcvCls; PcvCls.cls',
        'ExeName32="pcv.exe"',
        'Startup="PcvUser"',
        ''
    )
    Set-Content -LiteralPath (Join-Path $work "PcvConstants.bas") -Value $modA -Encoding ASCII
    Set-Content -LiteralPath (Join-Path $work "PcvUser.bas")     -Value $modB -Encoding ASCII
    Set-Content -LiteralPath (Join-Path $work "PcvCls.cls")      -Value $clsC -Encoding ASCII
    $proj = Join-Path $work "pcv.vbp"
    Set-Content -LiteralPath $proj -Value $vbp -Encoding ASCII

    $outExe = Join-Path $work "emit.c"
    $errFile = Join-Path $work "emit.err"
    # 用 OS 级重定向取字节（--emit-c 的产物走 stdout, 诊断走 stderr）
    $p = Start-Process -FilePath $exe -ArgumentList @("`"$proj`"", "--emit-c") `
                       -NoNewWindow -Wait -PassThru `
                       -RedirectStandardOutput $outExe -RedirectStandardError $errFile
    if ($p.ExitCode -ne 0) { $bad += ("V0 compiler exited " + $p.ExitCode + " on the probe project") }

    $emit = ""
    if (Test-Path -LiteralPath $outExe) { $emit = Get-Content -LiteralPath $outExe -Raw }
    $errTxt = ""
    if (Test-Path -LiteralPath $errFile) { $errTxt = Get-Content -LiteralPath $errFile -Raw }

    # ---------- V1: 三个真名字都不许报 VB3001 ----------
    foreach ($nm in @("PCV_ANSWER", "PCV_ON", "PCV_OFF")) {
        if ($errTxt -match ("VB3001[^\r\n]*" + $nm)) {
            $bad += ("V1 project-level name " + $nm + " still reports VB3001 (it is a Public Const/Enum member of this project)")
        }
    }
    # 产物必须是折好的字面量 (实测 `vb6_ret_SumIt = ((42 + 3) - 0);`) —— 认折叠, 不认括号形状
    if ($emit -notmatch '42 \+ 3') {
        $bad += "V1 emitted C did not fold the cross-module constants (want the literals 42 and 3 to meet in one expression)"
    }
    if ($emit -match 'PCV_ANSWER \+|\+ PCV_ON|\+ PCV_OFF') {
        $bad += "V1 emitted C still evaluates these constants by name instead of folding them"
    }

    # ---------- V2: 真不存在的那个名字必须继续报 ----------
    if ($errTxt -notmatch "VB3001[^\r\n]*pcvNoSuchNameAnywhere") {
        $bad += "V2 NEGATIVE CONTROL FAILED: pcvNoSuchNameAnywhere no longer reports VB3001 - the gate would be blanket-off"
    }

    # ---------- S1/S2: 单一建造点与单一消费点 ----------
    function Count-Matches([string]$path, [string]$pattern) {
        if (-not (Test-Path -LiteralPath $path)) { $bad += ("S missing file " + $path); return 0 }
        return ([regex]::Matches((Get-Content -LiteralPath $path -Raw), $pattern)).Count
    }
    $drv  = Join-Path $root "src\driver\driver_semantics.cpp"
    $util = Join-Path $root "src\semantics\semantic_analyzer_util.cpp"
    $hdr  = Join-Path $root "src\semantics\semantic_analyzer.hpp"

    $insConst  = Count-Matches $drv 'projPubConsts\.insert'
    $insProc   = Count-Matches $drv 'projPubProcs\.insert'
    $insMod    = Count-Matches $drv 'projModNames\.insert'
    if ($insConst -ne 2) { $bad += ("S1 projPubConsts.insert has " + $insConst + " sites (want exactly 2: ConstDecl + EnumMember)") }
    if ($insProc  -ne 1) { $bad += ("S1 projPubProcs.insert has "  + $insProc  + " sites (want exactly 1)") }
    if ($insMod   -ne 1) { $bad += ("S1 projModuleNames insert has " + $insMod + " sites (want exactly 1)") }

    $setters = @("setProjectModuleNames", "setProjectPublicProcNames", "setProjectPublicConstNames")
    foreach ($s in $setters) {
        $decl = Count-Matches $hdr  ("void " + $s + "\(")
        $call = Count-Matches $drv  ("analyzer->" + $s + "\(")
        if ($decl -ne 1) { $bad += ("S2 " + $s + " declared " + $decl + " times in the header (want 1)") }
        if ($call -ne 1) { $bad += ("S2 " + $s + " called " + $call + " times in the driver (want 1)") }
    }
    $consConst = Count-Matches $util 'projPublicConsts_\.count'
    $consProc  = Count-Matches $util 'projPublicProcs_\.count'
    $consMod   = Count-Matches $util 'projModuleNames_\.count'
    if ($consConst -ne 1 -or $consProc -ne 1 -or $consMod -ne 1) {
        $bad += ("S1 the three project-level tables must be asked in exactly ONE place: consts=" +
                 $consConst + " procs=" + $consProc + " modules=" + $consMod)
    }
} finally {
    Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
}

if ($bad.Count -gt 0) {
    foreach ($b in $bad) { Write-Host ("FAIL " + $b) }
    exit 1
}
Write-Host "PASS project_const_visibility (V1 folded + no VB3001, V2 bogus name still reported, S1 2/1/1 inserts, S2 3 setters 1+1, one consumer)"
exit 0
