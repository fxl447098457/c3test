# 账 #196 的结构性哨兵 (只扫源码, 不起 cl)
#
# 为什么单开这一道: 「这枚控件的绘图 DC 从哪儿来」在 RTL 里只许有一处口径
# (vb6_ControlDrawDC: _Paint 派发期用外层 BeginPaint 挂上的 VB6_PaintDC, 否则回落 GetDC)。
# Print/Cls 早就走它, 但**句柄交不回 VB 代码** —— `.hDC` 没有出口, 只能撞
# cgen_expr_with.cpp 那条 "hwnd.成员" 兜底 = C2039 (真工程物证 ucTreeMaps PropPagFMR.c:74)。
# 接上以后新增了两样会被人无意弄坏的东西:
#   一是「一个对象一张」那份按 HWND 缓存的窗口属性 VB6_ObjectDC (没有归还点就是每枚控件漏一张 GDI 句柄);
#   二是表里那两行只登记在 PictureBox / Form 两档上 —— 给成通用行的话 `List1.hDC` 也答一个数, 那是伪造成功。
# 这三件事在源码层面看都跟"正常"一模一样, 所以判据必须是结构性的。
#
# 规则 (改坏了会红, 不是装饰):
#   D1  vb6_ControlDrawDC 在 src/rtl 里恰好定义一次, 函数体两档都还在 (VB6_PaintDC 与 GetDC)
#   D2  按 `= vb6_ControlDrawDC(` 形状数出来的调用点 = 恰好 3 (Cls / Print / GetControlHDC) ——
#       这一刀之后所有拿绘图 DC 的路都从这一处走, 少一条就是有人又自己抢了一张
#   D3  vb6_GetControlHDC 恰好定义一次, 体内必须同时有: 调权威 / 缓存槽位写 / 白拿那张的 ReleaseDC;
#       并且**不许**自己 GetDC( —— 那就是第二处口径
#   D4  缓存槽位 VB6_ObjectDC: 写者(SetPropW) = 恰好 1, 归还(ReleaseDC) >= 1, 撤名(RemovePropW) >= 1
#   D5  后端: 表里 `return "vb6_GetControlHDC";` = 恰好 2 (PictureBox 与 Form 各一条, 不给通用行),
#       且 src/backend 里手拼发码 `"vb6_GetControlHDC(` = 0 (表交的是名字, 别处不许再拼一遍)
#
# 用法:  pwsh -File scripts\check_control_dc.ps1
# 退出码: 0 = 全绿; 1 = 红

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$bad = @()

$ctrlRel = "src\rtl\core\vb6forms\vb6forms_ctrl.c"
$ctrl = Join-Path $root $ctrlRel

# 把 src\rtl 下所有 .c/.h 逐行摊开: @{f=文件名; n=行号; t=本行(去注释)}
function Get-RtlLine {
    $dir = Join-Path $root "src\rtl"
    foreach ($f in (Get-ChildItem -LiteralPath $dir -Recurse -File | Where-Object { $_.Extension -in ".c", ".h" })) {
        $ln = 0
        foreach ($line in ([System.IO.File]::ReadAllText($f.FullName) -split "`r?`n")) {
            $ln++
            $t = $line.Trim()
            if ($t.StartsWith("//")) { continue }
            [pscustomobject]@{ File = $f.Name; Line = $ln; Text = $t; Full = $f.FullName }
        }
    }
}

$rtl = @(Get-RtlLine)

# ---- D1: 权威恰好一处, 两档都还在 ----
$defs = @($rtl | Where-Object { $_.Text -match 'HDC\s+vb6_ControlDrawDC\s*\(' -and $_.Text -notmatch '=' })
if ($defs.Count -ne 1) {
    $bad += ("D1 vb6_ControlDrawDC defined " + $defs.Count + " times (want exactly 1) -> " +
             (($defs | ForEach-Object { $_.File + ":" + $_.Line }) -join " | "))
}
if (Test-Path -LiteralPath $ctrl) {
    $body = [regex]::Match([System.IO.File]::ReadAllText($ctrl),
                            'vb6_ControlDrawDC\s*\([^)]*\)\s*\{[\s\S]{0,600}?\r?\n\}')
    if (-not $body.Success) {
        $bad += "D1 authority body not found"
    } else {
        if ($body.Value -notmatch 'L"VB6_PaintDC"') {
            $bad += "D1 the authority no longer honors the _Paint-dispatch DC (BeginPaint 那张会被当成泄漏释放掉)"
        }
        if ($body.Value -notmatch 'GetDC\(') {
            $bad += "D1 the authority no longer falls back to the window DC (派发期之外没人给 DC 了)"
        }
    }
} else {
    $bad += ("D1 missing " + $ctrlRel)
}

# ---- D2: 调用点形状与条数 ----
$calls = @($rtl | Where-Object { $_.Text.Contains("= vb6_ControlDrawDC(") })
if ($calls.Count -ne 3) {
    $bad += ("D2 call sites of the drawing-DC authority = " + $calls.Count +
             " (want exactly 3: Cls / Print / GetControlHDC) -> " +
             (($calls | ForEach-Object { $_.File + ":" + $_.Line }) -join " | "))
}

# ---- D3: 出口自己不许抢 DC ----
$exitDef = @($rtl | Where-Object { $_.Text -match '^intptr_t\s+vb6_GetControlHDC\s*\(' -and -not $_.Text.EndsWith(";") })
if ($exitDef.Count -ne 1) {
    $bad += ("D3 vb6_GetControlHDC defined " + $exitDef.Count + " times in src/rtl (want exactly 1; 头文件那条声明在 .h 里以 ; 结尾, 不算)")
} else {
    $txt = [System.IO.File]::ReadAllText($exitDef[0].Full)
    $blk = [regex]::Match($txt, 'intptr_t\s+vb6_GetControlHDC\s*\([\s\S]{0,900}?\r?\n\}')
    if (-not $blk.Success) {
        $bad += "D3 exit body not found"
    } else {
        if ($blk.Value -notmatch 'vb6_ControlDrawDC\(') {
            $bad += "D3 the exit no longer goes through the authority (口径又分家了)"
        }
        if ($blk.Value -notmatch 'SetPropW\([^;]*L"VB6_ObjectDC"') {
            $bad += "D3 the exit no longer caches one-DC-per-object (反复读会换句柄)"
        }
        if ($blk.Value -notmatch 'ReleaseDC\(') {
            $bad += "D3 the exit no longer returns the free GetDC (每读一次漏一张)"
        }
        if ($blk.Value -match 'GetDC\(') {
            $bad += "D3 the exit grabs a DC itself (绕过权威的第二处口径)"
        }
    }
}

# ---- D4: 缓存槽位只有一个写者, 且有人归还 ----
$wr = @($rtl | Where-Object { $_.Text.Contains("SetPropW") -and $_.Text.Contains('L"VB6_ObjectDC"') })
$rel = @($rtl | Where-Object { $_.Text.Contains("ReleaseDC") -and $_.Text.Contains("VB6_ObjectDC") })
$rm = @($rtl | Where-Object { $_.Text.Contains("RemovePropW") -and $_.Text.Contains('L"VB6_ObjectDC"') })
if ($wr.Count -ne 1) {
    $bad += ("D4 writers of the VB6_ObjectDC slot = " + $wr.Count + " (want exactly 1) -> " +
             (($wr | ForEach-Object { $_.File + ":" + $_.Line }) -join " | "))
}
if ($rel.Count -lt 1) { $bad += "D4 nobody releases the cached DC (归还点丢了)" }
if ($rm.Count -lt 1) { $bad += "D4 nobody removes the cached slot at destroy (销毁后又读会拿到野句柄)" }

# ---- D5: 后端两张表成对、不给通用行、别处不手拼 ----
$be = Join-Path $root "src\backend"
$rows = 0
$hand = 0
foreach ($f in (Get-ChildItem -LiteralPath $be -Recurse -File | Where-Object { $_.Extension -in ".cpp", ".inc", ".hpp" })) {
    $ln = 0
    foreach ($line in ([System.IO.File]::ReadAllText($f.FullName) -split "`r?`n")) {
        $ln++
        $t = $line.Trim()
        if ($t.StartsWith("//")) { continue }
        if ($t.Contains('return "vb6_GetControlHDC";')) { $rows++ }
        if ($line.Contains('"vb6_GetControlHDC(')) {
            $hand++
            $bad += ("D5 hand-composed emit of vb6_GetControlHDC( at " + $f.Name + ":" + $ln +
                     " (表交名字就够, 别处再拼一遍等于第二个出口)")
        }
    }
}
if ($rows -ne 2) {
    $bad += ("D5 read-table rows for hdc = " + $rows + " (want exactly 2: PictureBox 与 Form; 给成通用行 = List1.hDC 也答一个数)")
}

if ($bad.Count -eq 0) {
    Write-Host ("PASS Control drawing DC: 定义 " + $defs.Count + " / 调用 " + $calls.Count +
                " / 出口 " + $exitDef.Count + " / 槽位写 " + $wr.Count + " 归还 " + $rel.Count +
                " 撤名 " + $rm.Count + " / 读表 " + $rows + " 手拼 " + $hand) -ForegroundColor Green
    exit 0
}
foreach ($b in $bad) { Write-Host ("FAIL " + $b) -ForegroundColor Red }
exit 1
