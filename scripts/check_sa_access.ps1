# 账 #209 的结构性哨兵 (只扫源码, 不起 cl)
#
# 为什么单开这一道: 一维动态数组的**元素**访问在 RTL 里只有一个入口 —— VB6_SA_AT。 -- (tail pinned ascii)
# 这一刀之前它是裸指针算术 (`((type*)((arr)->data))[idx - (arr)->lBound]`), 描述符是 NULL
# (动态数组从未 ReDim / 已被 Erase) 或下标越界时它一声不吭地读 NULL+0xc → 原生 0xC0000005。 -- (tail pinned ascii)
# 实测 Charts 2020 ucChartBar demo 点 Random: 三次崩在同一偏移 0x1abf2, 符号化落在 -- (tail pinned ascii)
# 生成代码 ucChartBar.c:591 的 `With m_Serie(Index)`。VB6 在这一条是运行时错误 9, 和 -- (tail pinned ascii)
# vb6_UBound/vb6_LBound 的 rev2 同一族 —— 那两处已经抛 9, 这一处漏了。 -- (tail pinned ascii)
#
# 现在检查收在 vb6_SaElemPtr 这一处 (inline 热路径 + vb6_SaElemFail 冷路径)。 -- (tail pinned ascii)
# 本刀刻意**没**动多维那一支: VB6_SA_ND_AT1/2/3 与 4+ 维的 `_ndoff_` 兜底仍是裸寻址 -- (tail pinned ascii)
# (存量 1468 处 / 4 份工程, 大头是 VBFlexGridDemo), 同族下一刀再收, 所以 A4 是 -- (tail pinned ascii)
# "只许那一条兜底行还在", 不是"一条都不许有"。 -- (tail pinned ascii)
#
# 规则 (改坏了会红, 不是装饰):
#   A1  VB6_SA_AT 的宏定义恰好 1 处, 且宏体走 vb6_SaElemPtr (不许再退回裸算术)
#   A2  vb6_SaElemPtr 的 inline 定义恰好 1 处, 体内两条比较(下界/上界)与 NULL 那条都还在 -- (tail pinned ascii)
#   A3  vb6_SaElemFail 声明 1 + 定义 1, 定义体里 vb6_ErrRaise(9 那条还在 -- (tail pinned ascii)
#   A4  src/backend 里非注释的 `->data` 元素寻址行 = 恰好 1 (多维兜底), 多了就是又开了一处旁路 -- (tail pinned ascii)
#   A5  src/rtl 与 src/backend 里 `->data))[` 这种"宏里手算下标"的形状 = 0
#
# 用法:  pwsh -File scripts\check_sa_access.ps1
# 退出码: 0 = 全绿; 1 = 红 -- (tail pinned ascii)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$bad = @()

function Lines([string]$path) {
    ([System.IO.File]::ReadAllText($path) -split "`r?`n")
}

$rtlDir = Join-Path $root "src\rtl"
$beDir = Join-Path $root "src\backend"
$arrH = Join-Path $root "src\rtl\core\vb6rtl\vb6rtl_array.h"
$arrC = Join-Path $root "src\rtl\core\vb6rtl\vb6rtl_array.c"
foreach ($p in @($arrH, $arrC)) {
    if (-not (Test-Path -LiteralPath $p)) {
        Write-Host ("FAIL A0 file missing: " + $p) -ForegroundColor Red
        exit 1
    }
}

# A1: 宏只有定义点这一处写法, 而且必须经 vb6_SaElemPtr
$macroDefs = @()
$macroBodyCallsHelper = $false
foreach ($f in (Get-ChildItem -LiteralPath $rtlDir -Recurse -File | Where-Object { $_.Extension -in ".c", ".h" })) {
    $ln = 0
    foreach ($line in (Lines $f.FullName)) {
        $ln++
        if ($line.Trim() -match '^#\s*define\s+VB6_SA_AT\(') {
            $macroDefs += ($f.Name + ":" + $ln)
            $body = (Lines $f.FullName)[$ln..([Math]::Min($ln + 2, (Lines $f.FullName).Count - 1))] -join " "
            if ($body -match "vb6_SaElemPtr") { $macroBodyCallsHelper = $true }
        }
    }
}
if ($macroDefs.Count -ne 1) {
    $bad += ("A1 VB6_SA_AT macro definitions = " + $macroDefs.Count + " (want exactly 1) -> " + ($macroDefs -join " | "))
}
if ($macroDefs.Count -eq 1 -and -not $macroBodyCallsHelper) {
    $bad += "A1 VB6_SA_AT body no longer routes through vb6_SaElemPtr (the check moved back to raw math)"
}

# A2: 那个唯一的 inline, 三条检查一条不许哑 -- (tail pinned ascii)
$h = [System.IO.File]::ReadAllText($arrH)
$defs = @([regex]::Matches($h, 'static\s+inline\s+void\*\s+vb6_SaElemPtr\s*\('))
if ($defs.Count -ne 1) {
    $bad += ("A2 vb6_SaElemPtr defined " + $defs.Count + " times (want 1)")
} else {
    $blk = [regex]::Match($h, 'static\s+inline\s+void\*\s+vb6_SaElemPtr[\s\S]{0,800}?\r?\n\}')
    if ($blk.Value -notmatch '!arr') { $bad += "A2 NULL-descriptor check is gone from the helper" }
    if ($blk.Value -notmatch 'idx\s*<\s*arr->lBound') { $bad += "A2 lower-bound compare is gone from the helper" }
    if ($blk.Value -notmatch 'idx\s*>\s*arr->uBound') { $bad += "A2 upper-bound compare is gone from the helper" }
    if ($blk.Value -notmatch 'vb6_SaElemFail') { $bad += "A2 the cold-path call is gone from the helper" }
}

# A3: 冷路径声明 + 定义各 1, 且真的抛 9
$decl = @([regex]::Matches($h, 'void\s+vb6_SaElemFail\s*\(')).Count
$csrc = [System.IO.File]::ReadAllText($arrC)
$defn = @([regex]::Matches($csrc, '\r?\nvoid\s+vb6_SaElemFail\s*\(')).Count
if ($decl -ne 1) { $bad += ("A3 vb6_SaElemFail 声明 = " + $decl + " (want 1)") }
if ($defn -ne 1) { $bad += ("A3 vb6_SaElemFail 定义 = " + $defn + " (want 1)") }
$cblk = [regex]::Match($csrc, 'void\s+vb6_SaElemFail[\s\S]{0,1200}?\r?\n\}')
if (-not $cblk.Success -or $cblk.Value -notmatch 'vb6_ErrRaise\(\s*9\b') {
    $bad += "A3 cold path no longer raises runtime error 9 (silent return would spare sites without On Error)"
}

# A4: 后端只许多维兜底那一行手算元素地址 -- (tail pinned ascii)
$ndLines = @()
foreach ($f in (Get-ChildItem -LiteralPath $beDir -Recurse -File | Where-Object { $_.Extension -in ".cpp", ".inc", ".hpp" })) {
    $ln = 0
    foreach ($line in (Lines $f.FullName)) {
        $ln++
        $t = $line.Trim()
        if ($t.StartsWith("//")) { continue }
        if ($line.Contains('->data')) { $ndLines += ($f.Name + ":" + $ln) }
    }
}
if ($ndLines.Count -ne 1) {
    $bad += ("A4 后端手算元素地址的行 = " + $ndLines.Count +
             " (want exactly 1: 多维 4+ 维的 _ndoff_ 兜底) -> " + ($ndLines -join " | "))
} elseif ($ndLines[0] -notmatch "cgen_expr_call_prelude\.inc") {
    $bad += ("A4 that line is no longer the multi-dim fallback -> " + $ndLines[0])
}

# A5: "宏里手算下标"这个形状在整个仓里不许复活 -- (tail pinned ascii)
$shape = 0
foreach ($dir in @($rtlDir, $beDir)) {
    foreach ($f in (Get-ChildItem -LiteralPath $dir -Recurse -File | Where-Object { $_.Extension -in ".c", ".h", ".cpp", ".inc", ".hpp" })) {
        foreach ($line in (Lines $f.FullName)) {
            if ($line.Trim().StartsWith("//")) { continue }
            if ($line.Contains('->data))[')) { $shape++ }
        }
    }
}
if ($shape -ne 0) {
    $bad += ("A5 裸 `->data))[` 元素寻址 = " + $shape + " 处 (want 0; 一维那支必须只有宏这一个入口)")
}

if ($bad.Count -eq 0) {
    Write-Host ("PASS SA element access: macro " + $macroDefs.Count +
                " / inline " + $defs.Count +
                " / cold-path decl " + $decl + " def " + $defn +
                " / backend fallback lines " + $ndLines.Count +
                " / raw-index shape " + $shape) -ForegroundColor Green
    exit 0
}
foreach ($b in $bad) { Write-Host ("FAIL " + $b) -ForegroundColor Red }
exit 1
