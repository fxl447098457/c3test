# check_form_draw_state.ps1 - 账 #233 的结构性哨兵 (只扫源码, 不起 cl)
#
# 为什么单开这一道: Form 的绘图状态属性 (CurrentX / CurrentY / DrawWidth) 这一族, 缺陷形状是
# "只接了一半" —— 读侧在 cgen 的一份侧表里硬编码四条, 写侧压根没登记进 cgen 的写表, 于是
# `Me.DrawWidth = 3` 发成 `vb6_Form_DrawGetWidth(hwnd) = 3;` (C2106, 两架构都编不出来);
# 同时 RTL 在**同一个窗口属性名** `VB6_CurrentX` 上挂了两套编码 (绘图家族 int32、Form Print
# float 位图案), 互读必错。这些都是"看着像做完了"的形状, 编译与运行各只响一半, 所以要结构钉。
#
# 规则 (改坏了会红, 不是装饰):
#   S1  笔位那份存储唯一: `VB6_CurrentX` / `VB6_CurrentY` 这两个窗口属性名的 GetPropW/SetPropW
#       在 src/rtl 里只许出现在 vb6forms_widget_prop.c (float 那一户) —— 再开第二处 = 又一份编码
#   S2  cgen 侧表不回潮: 四个笔位导出名 (vb6_Form_DrawGet/SetCurrentX/Y) 在 src/backend 里 0 次
#   S3  读写成对: currentx / currenty / drawwidth 三个名在 getControlPropReadFn 的 Form 档与
#       getControlPropWriteFn 的 Form 档**都要**有 (少一边就是这一刀回归)
#   S4  编码对称: vb6_DrawSetI 那条 SetPropW 必须写裸值 (只看写行 —— 注释里提到 "v+1" 是在讲历史)
#
# 用法:  pwsh -File scripts\check_form_draw_state.ps1
# 退出码: 0 = 全绿; 1 = 红

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$bad = @()

$rtlDir = Join-Path $root "src\rtl"
$beDir = Join-Path $root "src\backend"
if (-not (Test-Path -LiteralPath $rtlDir)) { Write-Host "FAIL S0 missing src\rtl" -ForegroundColor Red; exit 1 }
if (-not (Test-Path -LiteralPath $beDir)) { Write-Host "FAIL S0 missing src\backend" -ForegroundColor Red; exit 1 }

function Get-SrcFiles($dir) {
    Get-ChildItem -LiteralPath $dir -Recurse -File |
        Where-Object { $_.Extension -in ".c", ".h", ".cpp", ".inc", ".hpp" }
}

# ---- S1: 笔位存储唯一 ----
$keepRel = "src\rtl\core\vb6forms\vb6forms_widget_prop.c"
$keep = Join-Path $root $keepRel
$patProp = '(GetPropW|SetPropW)\s*\([^;]*L"VB6_Current[XY]"'
$leak = @()
foreach ($f in Get-SrcFiles $rtlDir) {
    if ($f.FullName -eq $keep) { continue }
    foreach ($m in [regex]::Matches([System.IO.File]::ReadAllText($f.FullName), $patProp)) {
        $leak += ($f.Name + ":" + $m.Value.Substring(0, [Math]::Min(40, $m.Value.Length)))
    }
}
if ($leak.Count -ne 0) {
    $bad += ("S1 pen-position store opened a second home = " + $leak.Count + " -> " +
             (($leak | Select-Object -First 5) -join " | "))
}
$keepText = [System.IO.File]::ReadAllText($keep)
foreach ($n in @("VB6_CurrentX", "VB6_CurrentY")) {
    if (@([regex]::Matches($keepText, '"' + $n + '"')).Count -lt 2) {
        $bad += ("S1 " + $keepRel + " no longer owns both ends of " + $n)
    }
}

# ---- S2: cgen 侧表不回潮 ----
$patSide = 'vb6_Form_Draw(Set)?Current[XY]'
$side = @()
foreach ($f in Get-SrcFiles $beDir) {
    foreach ($m in [regex]::Matches([System.IO.File]::ReadAllText($f.FullName), $patSide)) {
        $side += ($f.Name + ":" + $m.Value)
    }
}
if ($side.Count -ne 0) {
    $bad += ("S2 the deleted side-list is back in cgen = " + (($side | Select-Object -First 5) -join " | "))
}

# ---- S3: 读写成对 (Form 档) ----
$util = Join-Path $root "src\backend\cgen_util_ctrl.cpp"
$u = [System.IO.File]::ReadAllText($util)
function Get-FormCase([string]$text, [string]$fnPat, [string]$tag) {
    $mFn = [regex]::Match($text, $fnPat + '[\s\S]*?\r?\n\}')
    if (-not $mFn.Success) { return $null }
    $mCase = [regex]::Match($mFn.Value, 'case\s+FrmControlType::Form\s*:[\s\S]*?break\s*;')
    if (-not $mCase.Success) { return $null }
    return $mCase.Value
}
$rd = Get-FormCase $u 'std::string\s+CCodeGen::getControlPropReadFn' 'read'
$wr = Get-FormCase $u 'std::string\s+CCodeGen::getControlPropWriteFn' 'write'
if ($null -eq $rd) { $bad += "S3 getControlPropReadFn Form case not found" }
if ($null -eq $wr) { $bad += "S3 getControlPropWriteFn Form case not found" }
if ($null -ne $rd -and $null -ne $wr) {
    foreach ($p in @("currentx", "currenty", "drawwidth")) {
        $nr = @([regex]::Matches($rd, '"' + $p + '"')).Count
        $nw = @([regex]::Matches($wr, '"' + $p + '"')).Count
        if ($nr -lt 1 -or $nw -lt 1) {
            $bad += ("S3 property '" + $p + "' is registered read=" + $nr + " write=" + $nw +
                     " (both ends are required; the missing half is what made 233 compile-broken)")
        }
    }
}

# ---- S4: 编码对称 ----
$draw = Join-Path $root "src\rtl\core\vb6forms\vb6forms_draw.c"
$d = [System.IO.File]::ReadAllText($draw)
$mSet = [regex]::Match($d, 'static void vb6_DrawSetI\([\s\S]*?\r?\n\}')
if (-not $mSet.Success) {
    $bad += "S4 vb6_DrawSetI not found in vb6forms_draw.c"
} elseif (@([regex]::Matches($mSet.Value, 'SetPropW\s*\(\s*hw\s*,\s*name\s*,[^;]*')).Count -ne 1) {
    $bad += "S4 vb6_DrawSetI no longer has exactly one SetPropW(hw, name, ...) write"
} else {
    # 只看那一行写进去的表达式 —— 注释里出现 "v+1" 是在讲历史, 不算数
    $w = [regex]::Match($mSet.Value, 'SetPropW\s*\(\s*hw\s*,\s*name\s*,[^;]*').Value
    if ($w -match '\+\s*1') {
        $bad += ("S4 vb6_DrawSetI stores v+1 again while vb6_DrawGetI does not subtract 1 -> " + $w.Trim())
    }
}

if ($bad.Count -eq 0) {
    Write-Host ("PASS form draw state: pen store unique, side-list gone, " +
                "read/write paired for 3 props, encoding symmetric") -ForegroundColor Green
    exit 0
}
foreach ($b in $bad) { Write-Host ("FAIL " + $b) -ForegroundColor Red }
exit 1
