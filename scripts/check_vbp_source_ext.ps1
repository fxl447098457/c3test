# check_vbp_source_ext.ps1 - VB6 原生 .vbp 的源文件条目不带扩展名, C3 必须自己补 (账 #293)
#
# 起因: VB6 IDE 存盘写的是
#       Form=MainForm / Module=Module1 / Class=Cls1 / UserControl=Uc1 / PropertyPage=Pp1
# —— **一律不带扩展名**, 且源文件常常在子目录里 (`Module=Common\Common`)。
# ComCtlsDemo.vbp 能编过只是因为它手写了扩展名 (`Module=Common; Common\Common.bas`,
# 那是外部工具改出来的两段格式), 所以这个洞在仓里的夹具上完全测不到。
#
# C3 之前把 .vbp 的值原样当路径用, 于是 open("<proj>\Module1") 失败 => VB1006
# 「无法打开文件」。**任何 VB6 IDE 存出来的工程都编不出 exe。**
#
# 本哨兵钉两头, 缺一头都会红:
#   V1 正例: 一份完全照 VB6 原生写法 (条目不带扩展名) 的工程, 七种条目
#      (Form / Module / Class / UserControl / PropertyPage / 子目录 Module)
#      必须全部解析成功并各自产出对应的 .c。
#   V2 反面证人: 已经带扩展名的条目不许被"顺手"改写 —— 放一个真叫 `Foo.Bar` 的源文件,
#      产物必须仍然是它自己 (补扩展名只允许发生在**确实没有扩展名**时)。
#
# 用法: powershell -File scripts\check_vbp_source_ext.ps1   (PASS = exit 0)

param(
    [string]$Root = (Split-Path -Parent $PSScriptRoot),
    [string]$Exe  = ""      # 负控用: 拿另一枚 C3.exe 跑同一份夹具
)
$ErrorActionPreference = "Stop"
$root = $Root
$exe = if ($Exe) { $Exe } else { Join-Path $root ".build\C3.exe" }
if (-not (Test-Path -LiteralPath $exe)) {
    Write-Host ("FAIL missing " + $exe + "  (run scripts\build.bat first)")
    exit 1
}

$bad = @()

# 会话目录跟着 TEMP 走; C3 建不出来就只剩一句 access denied, 读不成被测缺陷。
$work = Join-Path $env:TEMP ("c3_vbpext_" + [Guid]::NewGuid().ToString("N").Substring(0, 8))
$outDir = Join-Path $work "out"
New-Item -ItemType Directory -Force -Path "$work\Sub" | Out-Null

function Put([string]$rel, [string[]]$lines) {
    $p = Join-Path $work $rel
    Set-Content -LiteralPath $p -Value $lines -Encoding ASCII
}

# ---- 夹具: VB6 原生写法, 条目一律不带扩展名 -------------------------------
Put "p.vbp" @(
    'Type=Exe',
    'Form=MainForm',
    'Module=Mod1',
    'Module=Sub\Deep',
    'Class=C1',
    'UserControl=U1',
    'PropertyPage=PP1'
)
Put "MainForm.frm" @(
    'VERSION 5.00',
    'Begin VB.Form MainForm ',
    '  Caption         =   "P"',
    'End',
    'Attribute VB_Name = "MainForm"',
    'Attribute VB_GlobalNameSpace = False',
    'Attribute VB_Creatable = False',
    'Attribute VB_PredeclaredId = True',
    'Attribute VB_Exposed = True'
)
Put "Mod1.bas" @('Attribute VB_Name = "Mod1"', 'Public Const M = 7')
Put "Sub\Deep.bas" @('Attribute VB_Name = "Deep"', 'Public Const D = 9')
Put "C1.cls" @('Attribute VB_Name = "C1"', 'Public Sub Go()', 'End Sub')
Put "U1.ctl" @(
    'VERSION 5.00', 'Begin VB.UserControl U1 ', 'End',
    'Attribute VB_Name = "U1"', 'Attribute VB_GlobalNameSpace = False',
    'Attribute VB_Creatable = False'
)
Put "PP1.pag" @(
    'VERSION 5.00', 'Begin VB.PropertyPage PP1 ', 'End',
    'Attribute VB_Name = "PP1"', 'Attribute VB_GlobalNameSpace = False'
)

# ---- V2 反面证人: 条目已经带扩展名, 不许被再补一层 --------------------------
# ComCtlsDemo.vbp 就是这种写法 (`Module=Common; Common\Common.bas`)。补扩展名的
# 逻辑只看"是否已有扩展名", 所以这一份必须照样解析成功 —— 若无条件往后接,
# 会去找 Weird.bas.bas / Weird.bas.frm ... 全部落空。
$vbp2 = Join-Path $work "q.vbp"
Set-Content -LiteralPath $vbp2 -Value @('Type=Exe', 'Module=Weird.bas') -Encoding ASCII
Set-Content -LiteralPath (Join-Path $work "Weird.bas") -Encoding ASCII -Value @(
    'Attribute VB_Name = "Weird"', 'Public Const Z = 1'
)

$emit = Join-Path $work "emit.c"
$err  = Join-Path $work "emit.err"
$p = Start-Process -FilePath $exe `
        -ArgumentList @("`"$(Join-Path $work 'p.vbp')`"", "--emit-c") `
        -NoNewWindow -Wait -PassThru `
        -RedirectStandardOutput $emit -RedirectStandardError $err
$v1exit = $p.ExitCode
$v1err = ""
if (Test-Path -LiteralPath $err) { $v1err = Get-Content -LiteralPath $err -Raw }

# ---- V1 正例: 七种条目各自都要解析出来 --------------------------------------
if ($v1exit -ne 0) {
    $bad += ("V1 compiler exited " + $v1exit + " on an extensionless (VB6-native) vbp")
}
if ($v1err -match "VB1006") {
    $bad += ("V1 still reports VB1006 (cannot open file) on an extensionless vbp: " +
             (($v1err -split "`r?`n" | Where-Object { $_ -match "VB1006" } | Select-Object -First 1)))
}

# 产物按模块名出 .c; 子目录的 Module=Sub\Deep 也要出 Deep.c
$want = @('MainForm.c', 'Mod1.c', 'Deep.c', 'C1.c', 'U1.c', 'PP1.c')
if ($v1exit -eq 0) {
    $session = Get-ChildItem -Path (Join-Path $env:TEMP "C3C") -Directory -ErrorAction SilentlyContinue |
               Sort-Object LastWriteTime -Descending | Select-Object -First 1
    foreach ($w in $want) {
        if (-not $session -or -not (Test-Path -LiteralPath (Join-Path $session.FullName $w))) {
            $bad += ("V1 no emit for " + $w + " (extensionless entry not resolved)")
        }
    }
}

# ---- V2 反面证人 ----------------------------------------------------------
$err2 = Join-Path $work "emit2.err"
$p2 = Start-Process -FilePath $exe `
        -ArgumentList @("`"$vbp2`"", "--emit-c") `
        -NoNewWindow -Wait -PassThru `
        -RedirectStandardOutput (Join-Path $work "emit2.c") -RedirectStandardError $err2
if ($p2.ExitCode -ne 0) {
    $e2 = ""
    if (Test-Path -LiteralPath $err2) { $e2 = Get-Content -LiteralPath $err2 -Raw }
    $bad += ("V2 compiler exited " + $p2.ExitCode + " on an entry that already has an extension (Module=Weird.bas)")
    if ($e2 -match "VB1006") {
        $bad += "V2 reports VB1006 for Weird.bas - an extra extension was appended blindly"
    }
}

Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue

if ($bad.Count -eq 0) {
    Write-Host "PASS"
    exit 0
}
foreach ($b in $bad) { Write-Host ("FAIL " + $b) }
exit 1