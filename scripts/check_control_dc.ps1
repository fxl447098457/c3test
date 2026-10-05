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
# 4 处: Cls / Print / GetControlHDC / ControlMeasureTextPx（账 #196 第二条把文字量也接到同一处口径上了）
$calls = @($rtl | Where-Object { $_.Text.Contains("= vb6_ControlDrawDC(") })
if ($calls.Count -ne 4) {
    $bad += ("D2 call sites of the drawing-DC authority = " + $calls.Count +
             " (want exactly 4: Cls / Print / GetControlHDC / ControlMeasureTextPx) -> " +
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

# ---- D6~D9: 文字量那一半 (账 #196 的第二条: TextHeight / TextWidth) ----
$oaDef = @($rtl | Where-Object { $_.Text.Contains("float vb6_ControlTextWidth(") -and -not $_.Text.EndsWith(";") })
$oaDefH = @($rtl | Where-Object { $_.Text.Contains("float vb6_ControlTextHeight(") -and -not $_.Text.EndsWith(";") })
if ($oaDef.Count -ne 1) { $bad += ("D6 vb6_ControlTextWidth defined " + $oaDef.Count + " times in src/rtl (want exactly 1; .h 里那条以 ; 结尾, 不算)") }
if ($oaDefH.Count -ne 1) { $bad += ("D6 vb6_ControlTextHeight defined " + $oaDefH.Count + " times in src/rtl (want exactly 1)") }

# D7: 后端那张"一个实参方法"表恰好定义一次, 两个成员名与两个出口名都还在
$beDir2 = Join-Path $root "src\backend"
$oaAuth = 0
$oaAuthBody = ""
$oaSites = @()
foreach ($f in (Get-ChildItem -LiteralPath $beDir2 -Recurse -File | Where-Object { $_.Extension -in ".cpp", ".inc", ".hpp" })) {
    $ln = 0
    foreach ($line in ([System.IO.File]::ReadAllText($f.FullName) -split "`r?`n")) {
        $ln++
        $t = $line.Trim()
        if ($t.StartsWith("//")) { continue }
        if ($t -match 'std::string\s+CCodeGen::controlOneArgMethod\s*\(') { $oaAuth++; $oaAuthBody = $f.Name }
        if ($t.Contains("controlOneArgMethod(") -and $t -notmatch 'std::string\s+CCodeGen::') {
            $oaSites += ($f.Name + ":" + $ln)
        }
    }
}
if ($oaAuth -ne 1) { $bad += ("D7 controlOneArgMethod defined " + $oaAuth + " times (want exactly 1)") }
if ($oaSites.Count -lt 2) {
    $bad += ("D7 call sites of controlOneArgMethod = " + $oaSites.Count +
             " (两条码头都要接: With 形与带括号裸形; 只接一头 = 编得过而另一头静默, 同 #143/#150) -> " +
             ($oaSites -join " | "))
}
# 两条码头要**分属两个文件** —— 光数条数会被 cgen_helpers.inc 里那条声明凑够数
$oaFiles = @($oaSites | ForEach-Object { ($_ -split ":")[0] } | Sort-Object -Unique)
foreach ($need in @("cgen_expr_with.cpp", "cgen_expr_call_com_bind.inc")) {
    if ($oaFiles -notcontains $need) {
        $bad += ("D7 那条码头不再调用出口: " + $need + " (只接一头就是 #143 那一族)")
    }
}
$authTxt = [System.IO.File]::ReadAllText((Join-Path $root "src\backend\cgen_util_ctrl.cpp"))
$oaBlk = [regex]::Match($authTxt, 'CCodeGen::controlOneArgMethod\s*[\s\S]{0,700}?\r?\n\}')
if (-not $oaBlk.Success) {
    $bad += "D7 authority body not found"
} else {
    if ($oaBlk.Value -notmatch '"textheight"' -or $oaBlk.Value -notmatch '"textwidth"') {
        $bad += "D7 the table no longer recognises both member names"
    }
    if ($oaBlk.Value -notmatch 'vb6_ControlTextHeight' -or $oaBlk.Value -notmatch 'vb6_ControlTextWidth') {
        $bad += "D7 the table no longer points at the two RTL exits"
    }
    if ($oaBlk.Value -match 'default:\s*\r?\n\s*return\s+"vb6_Control') {
        $bad += "D7 the table grew a generic row (List1.TextHeight 也答一个数 = 伪造成功)"
    }
}

# D8: 两个出口名在 src/backend 只许以"名字"形式出现(表里 + 参数表), 不许别处手拼调用
$hand2 = 0
foreach ($f in (Get-ChildItem -LiteralPath $beDir2 -Recurse -File | Where-Object { $_.Extension -in ".cpp", ".inc", ".hpp" })) {
    $ln = 0
    foreach ($line in ([System.IO.File]::ReadAllText($f.FullName) -split "`r?`n")) {
        $ln++
        if ($line.Trim().StartsWith("//")) { continue }
        if ($line.Contains('"vb6_ControlTextHeight(') -or $line.Contains('"vb6_ControlTextWidth(')) {
            $hand2++
            $bad += ("D8 hand-composed emit of the text-measure exit at " + $f.Name + ":" + $ln)
        }
    }
}
$paramRows = 0
foreach ($f in (Get-ChildItem -LiteralPath $beDir2 -Recurse -File | Where-Object { $_.Extension -in ".cpp", ".inc", ".hpp" })) {
    foreach ($line in ([System.IO.File]::ReadAllText($f.FullName) -split "`r?`n")) {
        if ($line.Contains('{"vb6_ControlTextWidth"') -or $line.Contains('{"vb6_ControlTextHeight"')) { $paramRows++ }
    }
}
if ($paramRows -ne 2) {
    $bad += ("D8 实参签名表里那两行 = " + $paramRows + " (want 2; 缺了就等于把 Variant 裸喂给 GetTextExtentPoint32W = Fix 113 那一味)")
}

# D9: 文字量的两条语义不许丢 —— 必须过单位表(#177)、必须问这枚控件自己的字体/DC(#129)
# 注意要**一个一个函数各自**取身体再判: 上一版这里把两个函数当成一段来抓, 结果宽度那个出口
# 被改成"直接交像素"以后, 高度那半里的 vb6_ScalePxToUser 还在, 整段照样过 —— 假绿了一条。
foreach ($fnName in @("vb6_ControlTextWidth", "vb6_ControlTextHeight")) {
    if ($oaDef.Count -ne 1) { break }
    $all = [System.IO.File]::ReadAllText($oaDef[0].Full)
    $m = [regex]::Match($all, ('float\s+' + $fnName + '\s*\([\s\S]{0,600}?\r?\n\}'))
    if (-not $m.Success) {
        $bad += ("D9 " + $fnName + " 的函数体读不到")
        continue
    }
    $body = $m.Value
    if ($body -notmatch 'vb6_ScalePxToUser') {
        $bad += ("D9 " + $fnName + " 不再折算单位 (缇型对象上交像素 = 账 #177 那一味)")
    }
    if ($body -notmatch 'vb6_WindowScaleModeSelf') {
        $bad += ("D9 " + $fnName + " 不再读这枚窗口自己的 ScaleMode")
    }
    if ($body -notmatch 'vb6_ControlMeasureTextPx') {
        $bad += ("D9 " + $fnName + " 没走那一个量像素的权威 (口径又分家)")
    }
    if ($body -match 'GetDC\(NULL\)') {
        $bad += ("D9 " + $fnName + " 用屏幕 DC 量 (Fix 129: 量的是这枚控件自己的字体, 不是屏幕默认)")
    }
}

# ---- D10: 字体只从一处问窗口 (账 #200) ----
# 裸 STATIC 不记 WM_SETFONT（探针实测 WM_GETFONT / STM_GETFONT 都回 NULL），所以我们自己存一张
# VB6_CtrlFont，所有"这枚控件现在用什么字体"的读法都必须走 vb6_ControlFont 那一处出口。
# 口径范围刻意只圈 vb6forms_ctrl.c（文字量那一路）：仓里另一枚裸问在 vb6forms.c 的 groupbox
# 标题带里，探针实测 BUTTON 也不答 WM_GETFONT —— 那是另一条账，别把它的红算到这条上。
$accDef = @($rtl | Where-Object { $_.File -eq "vb6forms_ctrl.c" -and $_.Text -match '^\s*static\s+HFONT\s+vb6_ControlFont\s*\(' })
if ($accDef.Count -ne 1) {
    $bad += ("D10 vb6_ControlFont defined " + $accDef.Count + " times in vb6forms_ctrl.c (want exactly 1)")
}
$getFontRaw = @($rtl | Where-Object { $_.File -eq "vb6forms_ctrl.c" -and $_.Text.Contains("WM_GETFONT") })
if ($getFontRaw.Count -ne 1) {
    $bad += ("D10 问窗口字体的那一行 = " + $getFontRaw.Count + " 处 (want exactly 1，且只能在出口函数体内) -> " +
             (($getFontRaw | ForEach-Object { $_.File + ":" + $_.Line }) -join " | "))
} elseif ($accDef.Count -eq 1) {
    # 那一行还必须**在出口函数体里** —— 只在别处留一行也算破窗
    $dl = $accDef[0].Line
    $fl = $getFontRaw[0].Line
    if ($fl -le $dl -or $fl -gt $dl + 8) {
        $bad += ("D10 那一行 WM_GETFONT 不在 vb6_ControlFont 的函数体里 (定义在第 " + $dl +
                 " 行，读在第 " + $fl + " 行)")
    }
}
$fntW = @($rtl | Where-Object { $_.Text.Contains("SetPropW") -and $_.Text.Contains('L"VB6_CtrlFont"') })
$fntR = @($rtl | Where-Object { $_.Text.Contains("GetPropW") -and $_.Text.Contains('L"VB6_CtrlFont"') })
if ($fntW.Count -ne 1) { $bad += ("D10 writers of the VB6_CtrlFont slot = " + $fntW.Count + " (want exactly 1)") }
if ($fntR.Count -lt 1) { $bad += "D10 nobody reads the VB6_CtrlFont slot (证人又哑了)" }

if ($bad.Count -eq 0) {
    Write-Host ("PASS Control drawing DC: 定义 " + $defs.Count + " / 调用 " + $calls.Count +
                " / 出口 " + $exitDef.Count + " / 槽位写 " + $wr.Count + " 归还 " + $rel.Count +
                " 撤名 " + $rm.Count + " / 读表 " + $rows + " 手拼 " + $hand +
                " / 文字量出口 " + ($oaDef.Count + $oaDefH.Count) + " 码头 " + $oaSites.Count +
                " 签名表 " + $paramRows + " 手拼2 " + $hand2 +
                " / 字体出口 " + $accDef.Count + " 裸问 " + $getFontRaw.Count +
                " 槽位写 " + $fntW.Count + " 读 " + $fntR.Count) -ForegroundColor Green
    exit 0
}
foreach ($b in $bad) { Write-Host ("FAIL " + $b) -ForegroundColor Red }
exit 1
