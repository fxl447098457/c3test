# check_extender_push_pop.ps1 - Extender 的每个**全局变量**都必须在 push/pop/snapshot 里存取 (账 #294)
#
# 起因（实测 ComCtlsDemo）: `vb6_uc_push`/`vb6_uc_pop` 过去只存取
# `vb6_Extender_Left/Top/Width/Height` 四个槽位，其余 11 个 `vb6_Extender_*` 全是裸全局,
# 出栈没人还原 ⇒ **控件之间互相污染**。实测 ToolBar 在 ReadProperties 里
# `Extender.Align = vbAlignTop`（ToolBar.ctl:879），之后 StatusBar 读到的就是 1，
# 而不是它 .frm 里的 Align=2 (AlignBottom)，于是走错分支 —— 只因为 StatusBar 的
# 设计期高度恰好等于自然高度，肉眼看不出来。静默、跨控件、GA 抓不到。
#
# 钉住两件事：
#   1. 成员集合 —— 从 RTL 声明处**自动列出**所有 vb6_Extender_* 全局变量（不写死清单），
#      所以以后新增成员同样受管；
#   2. 存取对称 —— 每个成员都要在 vb6_uc_push 的保存段、vb6_uc_pop 的还原段、
#      以及 vb6_uc_snapshot 的"没有当前记录"兜底段里各出现一次。
#
# 注意只捞**变量**不捞函数: vb6_Extender_SetFocus / _DragIcon / _Container / _ZOrder
# 都是 RTL 函数，本来就不该进出栈。
#
# 用法: powershell -File scripts\check_extender_push_pop.ps1   (PASS = exit 0)

param([string]$Root = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = "Stop"
$bad = @()

$names = New-Object System.Collections.Generic.HashSet[string]
$rxVar = '(?m)^\s*(?:extern\s+)?[A-Za-z_]\w*\s*\*?\s*(vb6_Extender_[A-Za-z_]\w*)\s*(?:=[^;]*)?;'
foreach ($df in @((Join-Path $Root "src\rtl\core\vb6rtl\vb6rtl_com.c"),
                 (Join-Path $Root "src\rtl\core\vb6rtl\vb6rtl_userctl.h"))) {
    if (-not (Test-Path -LiteralPath $df)) { $bad += "missing $df"; continue }
    $txt = [System.IO.File]::ReadAllText($df)
    foreach ($m in [regex]::Matches($txt, $rxVar)) { [void]$names.Add($m.Groups[1].Value) }
}
if ($names.Count -lt 10) { $bad += ("census found only " + $names.Count + " Extender globals - census is broken") }

$hostFile = Join-Path $Root "src\rtl\core\vb6forms\uc\uc_host.c"
if (-not (Test-Path -LiteralPath $hostFile)) { $bad += "missing $hostFile" }
else {
    $src = [System.IO.File]::ReadAllText($hostFile)
    function Get-Body([string]$text, [string]$sig) {
        $i = $text.IndexOf($sig); if ($i -lt 0) { return "" }
        $j = $text.IndexOf("`n}", $i); if ($j -lt 0) { return $text.Substring($i) }
        return $text.Substring($i, $j - $i)
    }
    $push = Get-Body $src "void vb6_uc_push("
    $pop  = Get-Body $src "void vb6_uc_pop("
    $snap = Get-Body $src "static void vb6_uc_snapshot("
    if (-not $push) { $bad += "cannot locate vb6_uc_push" }
    if (-not $pop)  { $bad += "cannot locate vb6_uc_pop" }
    if (-not $snap) { $bad += "cannot locate vb6_uc_snapshot" }
    if ($push -and $pop -and $snap) {
        foreach ($n in ($names | Sort-Object)) {
            if ($push -notmatch [regex]::Escape($n)) { $bad += "$n never saved in vb6_uc_push (leaks to next control)" }
            if ($pop  -notmatch [regex]::Escape($n)) { $bad += "$n never restored in vb6_uc_pop (survives into next control)" }
            if ($snap -notmatch [regex]::Escape($n)) { $bad += "$n not captured by vb6_uc_snapshot (no-record fallback)" }
        }
    }
}

if ($bad.Count -eq 0) {
    Write-Host ("PASS (" + $names.Count + " Extender globals saved/restored symmetrically)")
    exit 0
}
foreach ($b in $bad) { Write-Host ("FAIL " + $b) }
exit 1