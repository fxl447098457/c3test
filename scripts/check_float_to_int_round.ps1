# check_float_to_int_round.ps1 - 账 #248 的结构性哨兵 (只扫源码, 不起 cl)
#
# 为什么单开这一道: 浮点交给整数目标这一件事, 以前**有两条路两个答案** ——
# 发码把 double 直接递进 vb6_ChkLong(int64_t)，C 在调用边界上截断 (`l = 7 / 2` ⇒ 3)；
# 同一句写成 `l = CLng(7 / 2)` 走 vb6_CLng 内部的 round() ⇒ 4。这类"同一个决定抄了两遍"
# 是本仓反复量出来的缺陷族 (#234 拿 DC / #235 画笔色 / #239 笔位 / #247 宿主层几何)，
# 而它的形状在编译期与链接期**一声不响** —— 只有拿 exact .75 / .5 这类数去跑才看得见。
#
# 规则 (改坏了会红, 不是装饰):
#   S1  取整出口只有一枚: vb6_FltToLng 在 src/rtl 里恰好定义一次，在 vb6rtl_builtin.h 里声明一次
#   S2  那份 round() 也只许有一处: `(int64_t)round(` 在 src/rtl 里恰好 1 次，且住在 vb6_FltToLng 体内
#       (改前 vb6_CInt / vb6_CLng 各抄了一遍 —— 那正是"两个答案"的另一半)
#   S3  显式转换与隐式赋值同源: vb6_CInt / vb6_CLng 的体内必须调 vb6_FltToLng (两处)
#   S4  发码侧浮点档齐: narrowCheckAssign 里那一道浮点分支必须认 Single / Double / Currency 三档
#   S5  旧形不回潮: 套检查的两条发射路径写死 —— 浮点那条必须裹 helper，整数那条必须不裹；
#       出现第三种 `std::string(fn) + ...` 就是又开了一条不经 helper 的路
#   S6  helper 只住一处: vb6_FltToLng( 在 src/backend 里只许出现在 narrowCheckAssign 那一处，
#       且 vb6_ChkByte/Int/Long 这三个名字在 src/backend 里恰好 3 次 (就是那张 switch) ——
#       别处再拼一份 Chk 调用，等于把这条链挪出判据面
#
# 用法:  pwsh -File scripts\check_float_to_int_round.ps1
# 退出码: 0 = 全绿; 1 = 红

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$bad = @()

$rtlDir = Join-Path $root "src\rtl"
$beDir = Join-Path $root "src\backend"
$convRel = "src\rtl\core\vb6rtl\vb6rtl_conv.c"
$hdrRel = "src\rtl\core\vb6rtl\vb6rtl_builtin.h"
$typeRel = "src\backend\cgen_util_type.cpp"
$conv = Join-Path $root $convRel
$hdr = Join-Path $root $hdrRel
$typeFile = Join-Path $root $typeRel
foreach ($need in @($conv, $hdr, $typeFile)) {
    if (-not (Test-Path -LiteralPath $need)) {
        Write-Host ("FAIL S0 missing " + $need) -ForegroundColor Red
        exit 1
    }
}

$convText = [System.IO.File]::ReadAllText($conv)
$hdrText = [System.IO.File]::ReadAllText($hdr)
$typeText = [System.IO.File]::ReadAllText($typeFile)

function Get-SrcFiles($dir) {
    Get-ChildItem -LiteralPath $dir -Recurse -File |
        Where-Object { $_.Extension -in ".c", ".h", ".cpp", ".inc", ".hpp" }
}

# ---- S1: 取整出口只有一枚 ----
$defSites = @()
$defCount = 0
foreach ($f in Get-SrcFiles $rtlDir) {
    $n = @([regex]::Matches([System.IO.File]::ReadAllText($f.FullName), 'int64_t\s+vb6_FltToLng\s*\(\s*double[^)]*\)\s*\{')).Count
    if ($n -gt 0) { $defSites += ($f.Name + "=" + $n); $defCount += $n }
}
if ($defCount -ne 1) {
    $bad += ("S1 vb6_FltToLng definitions = " + $defCount + " (" + ($defSites -join ",") + ", expected 1)")
}
if (@([regex]::Matches($hdrText, 'int64_t vb6_FltToLng\(double x\);')).Count -ne 1) {
    $bad += "S1 vb6_FltToLng is not declared exactly once in vb6rtl_builtin.h"
}

# ---- S2: round() 那一处只住在 helper 体内 ----
$roundSites = @()
$roundCount = 0
foreach ($f in Get-SrcFiles $rtlDir) {
    $n = @([regex]::Matches([System.IO.File]::ReadAllText($f.FullName), '\(int64_t\)\s*round\s*\(')).Count
    if ($n -gt 0) { $roundSites += ($f.Name + "=" + $n); $roundCount += $n }
}
if ($roundCount -ne 1) {
    $bad += ("S2 (int64_t)round( sites in src/rtl = " + $roundCount + " (" + ($roundSites -join ",") +
             ", expected 1 -- the helper body; a second copy is the two-answers bug)")
}
$mHelp = [regex]::Match($convText, 'int64_t\s+vb6_FltToLng\s*\([^)]*\)\s*\{[\s\S]*?\}')
if (-not $mHelp.Success) {
    $bad += "S2 helper body not found in vb6rtl_conv.c"
} elseif (@([regex]::Matches($mHelp.Value, '\(int64_t\)\s*round\s*\(')).Count -ne 1) {
    $bad += "S2 the one remaining (int64_t)round( is not inside vb6_FltToLng's body"
}

# ---- S3: CLng / CInt 同源 ----
foreach ($fn in @("vb6_CInt", "vb6_CLng")) {
    $mFn = [regex]::Match($convText, '\b' + $fn + '\s*\(\s*double[^)]*\)\s*\{[^}]*\}')
    if (-not $mFn.Success) { $bad += ("S3 " + $fn + " body not found"); continue }
    if (@([regex]::Matches($mFn.Value, 'vb6_FltToLng\(')).Count -ne 1) {
        $bad += ($fn + " does not route through vb6_FltToLng exactly once -- its body is: " +
                 $mFn.Value.Replace("`r", " ").Replace("`n", " ").Substring(0, [Math]::Min(90, $mFn.Value.Length)))
    }
}

# ---- S4: 发码侧浮点档齐 ----
# 三道档必须一起在册: cgenIntBits(Single) 答 32，Long 目标也 32 —— 少了 Single 那一档，
# `l = 某Single` 会被下面那句"装得下, 不套"放过，连截断都省了 (实测 F2L05 那一形)。
$mGate = [regex]::Match($typeText, 'if\s*\(\s*vt == Vb6Type::Single\s*\|\|\s*vt == Vb6Type::Double\s*\|\|\s*vt == Vb6Type::Currency\s*\)')
if (-not $mGate.Success) {
    $bad += "S4 narrowCheckAssign's float gate no longer names all three of Single / Double / Currency"
}

# ---- S5: 两条发射路径写死 ----
$nHelper = @([regex]::Matches($typeText, 'return std::string\(fn\) \+ "\(vb6_FltToLng\(" \+ cValue \+ "\)\)";')).Count
$nPlain = @([regex]::Matches($typeText, 'return std::string\(fn\) \+ "\(" \+ cValue \+ "\)";')).Count
if ($nHelper -ne 1) { $bad += ("S5 float-source wrap through the helper = " + $nHelper + " (expected 1)") }
if ($nPlain -ne 1) { $bad += ("S5 integer-source wrap = " + $nPlain + " (expected 1)") }
$allWrap = @([regex]::Matches($typeText, 'std::string\(fn\)\s*\+')).Count
if ($allWrap -ne 2) { $bad += ("S5 emission paths for the narrow check = " + $allWrap + " (expected exactly 2)") }

# ---- S6: helper / Chk* 名字的站点普查 ----
$bHelp = @([regex]::Matches($typeText, 'vb6_FltToLng\(')).Count
if ($bHelp -ne 1) { $bad += ("S6 vb6_FltToLng( occurrences in cgen_util_type.cpp = " + $bHelp + " (expected 1)") }
$others = @()
foreach ($f in Get-SrcFiles $beDir) {
    if ($f.FullName -eq $typeFile) { continue }
    $n = @([regex]::Matches([System.IO.File]::ReadAllText($f.FullName), '"vb6_Chk(Byte|Int|Long)"')).Count
    if ($n -gt 0) { $others += ($f.Name + "=" + $n) }
}
if ($others.Count -ne 0) {
    $bad += ("S6 the narrow-check names leaked into another emitter: " + ($others -join ","))
}
$swNames = @([regex]::Matches($typeText, '"vb6_Chk(Byte|Int|Long)"')).Count
if ($swNames -ne 3) { $bad += ("S6 narrow-type table names = " + $swNames + " (expected 3: Byte / Integer / Long)") }

if ($bad.Count -ne 0) {
    foreach ($b in $bad) { Write-Host ("FAIL " + $b) -ForegroundColor Red }
    exit 1
}
Write-Host "PASS float_to_int_round (S1 one helper, S2 one round(), S3 CLng/CInt same source, S4 three float types gated, S5 two emission paths, S6 census clean)"
