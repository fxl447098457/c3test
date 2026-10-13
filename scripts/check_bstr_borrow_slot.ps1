# check_bstr_borrow_slot.ps1 - 账 #305 §B167 (第五十七刀): 「要收进 Variant 槽的右值自带所有权吗」只许住一处
#
# 为什么要这一道: 装箱出口 vb6_VariantString 只盖 vtBSTR 的章 —— 既不拷也不 AddRef, 于是
# `Dim v : v = s`(s 是活的 String 变量) 发出来的 `v = vb6_VariantString(s)` 让**槽与源头共用同一只
# BSTR**。源头一次重赋值 = 槽里读的是别人用过的堆块; 槽一次 Clear = 把源头还活着的那只块放掉。
# 两台实测在案(BASE 读数 .build/k57_base_x64.txt): W2 那窗把源串读成 "W2-SRC="、W7 那窗读成 "8"。
# 这一刀把那一问收成 CCodeGen::variantRhsOwnsValue 一枚(第五十五刀的「右值是不是活槽」并进来,
# 新添的是「右值是不是借来的串存储」), 由 §B164 站④ 问它。这一道哨兵钉的就是「别改回各站自己抄」。
#
# 五条规则:
#   Q1  variantRhsOwnsValue: 定义恰好 1 处 / 声明恰好 1 处 / 调用点 >= 1 (定义成死码也算红)
#   Q2  exprBorrowsBstrStorage: 定义恰好 1 处 / 声明恰好 1 处 (借用那一形单独露给后来的站点)
#   Q3  §B164 站④ 必须改问这一枚权威, 且第五十五刀那截内联判据 (rhsIsLiveSlot164) 不许再回来
#   Q4  借用那一问的三条腿都在: 两种串数组元素宏前缀 + 「顶层是调用=现造」 + 类型表答 String
#   Q5  硬填「右值自带」的站点计数钉死 (true 恰好 $TrueFloor) —— 新站点不问这一问就报红
param([string]$Root)
$ErrorActionPreference = 'Continue'
if (-not $Root) { $Root = (Split-Path -Parent $PSScriptRoot) }
$backend = Join-Path $Root 'src\backend'
if (-not (Test-Path $backend)) { Write-Host ('FAIL 没有 ' + $backend); exit 2 }

# Q5: 今天实测硬填「右值自带」的站点 —— 站①(ParamArray 栈副本) 站③(集合栈副本, 两形) 站⑤(自指临时量)
# 站⑥(返回 UDT 的 Variant 成员, §B164 形⑥ 待拍) §B166 形⑦(中间槽)。多出来的每一个都必须先过 Q1 那一问。
$TrueFloor = 6

$files = @(Get-ChildItem -Path $backend -Recurse -File -Include *.cpp, *.hpp, *.inc)
$bad = @()
$defQ1 = 0; $declQ1 = 0; $refsQ1 = 0
$defQ2 = 0; $declQ2 = 0; $defTop = 0
$asked = 0; $lambda = 0; $hardTrue = 0
$bodyQ4 = ''

$DEF1  = '(?m)^\s*bool\s+CCodeGen::variantRhsOwnsValue\s*\('
$DECL1 = '(?m)^\s*bool\s+variantRhsOwnsValue\s*\('
$CALL1 = 'variantRhsOwnsValue\s*\('
$DEF2  = '(?m)^\s*bool\s+CCodeGen::exprBorrowsBstrStorage\s*\('
$DECL2 = '(?m)^\s*bool\s+exprBorrowsBstrStorage\s*\('
$DEFT  = '(?m)^static bool b167TopIsCall\s*\('
$LAM   = 'rhsIsLiveSlot164'
$HARD  = 'emitVariantSlotStore\([^;]*?,\s*true\s*[,)]'

foreach ($f in $files) {
    $t = [System.IO.File]::ReadAllText($f.FullName)
    $lambda += ([regex]::Matches($t, $LAM)).Count
    # 调用点可能跨两行 (§B164 站② 就是), 先把空白压平再数 —— 与 Q3 同一读法
    $hardTrue += ([regex]::Matches([regex]::Replace($t, '\s+', ' '), $HARD)).Count
    $refsQ1 += ([regex]::Matches($t, $CALL1)).Count
    $defQ1 += ([regex]::Matches($t, $DEF1)).Count
    $declQ1 += ([regex]::Matches($t, $DECL1)).Count
    $defQ2 += ([regex]::Matches($t, $DEF2)).Count
    $declQ2 += ([regex]::Matches($t, $DECL2)).Count
    $defTop += ([regex]::Matches($t, $DEFT)).Count
    if ($f.Name -eq 'cgen_util_ctrl.cpp') {
        $dm = [regex]::Matches($t, $DEF2)
        if ($dm.Count -eq 1) {
            $i = $t.IndexOf($dm[0].Value)
            $j = $t.IndexOf([char]10 + '}', $i)
            if ($j -lt 0) { $j = $t.Length }
            $bodyQ4 = $t.Substring($i, [Math]::Max(0, $j - $i))
        }
    }
    if ($f.Name -eq 'cgen_assign_value_sem.inc') {
        # 站④ 那一调用跨两行, 先把空白压平再认 (与 §B166 的 P4 同一读法)
        $flat = [regex]::Replace($t, '\s+', ' ')
        $asked += ([regex]::Matches($flat,
            'emitVariantSlotStore\(\s*target\s*,\s*wrappedValue\s*,\s*variantRhsOwnsValue\s*\(\s*node\.value\.get\(\)\s*,\s*value\s*\)\s*\)')).Count
    }
}

$callQ1 = $refsQ1 - $defQ1 - $declQ1
if ($defQ1 -ne 1) { $bad += ('Q1 variantRhsOwnsValue 定义了 ' + $defQ1 + ' 遍(应 1) —— 那一问又散开了') }
if ($declQ1 -ne 1) { $bad += ('Q1 variantRhsOwnsValue 声明了 ' + $declQ1 + ' 处(应 1)') }
if ($callQ1 -lt 1) { $bad += ('Q1 variantRhsOwnsValue 没人调用(' + $callQ1 + ') —— 定义成了死码, 站点还在自己抄') }
if ($defQ2 -ne 1) { $bad += ('Q2 exprBorrowsBstrStorage 定义了 ' + $defQ2 + ' 遍(应 1)') }
if ($declQ2 -ne 1) { $bad += ('Q2 exprBorrowsBstrStorage 声明了 ' + $declQ2 + ' 处(应 1)') }
if ($defTop -ne 1) { $bad += ('Q4 b167TopIsCall 定义了 ' + $defTop + ' 遍(应 1)') }
if ($asked -ne 1) { $bad += ('Q3 §B164 站④ 没在问 variantRhsOwnsValue(' + $asked + ') —— 借来的串会被静默接管') }
if ($lambda -ne 0) { $bad += ('Q3 第五十五刀的内联判据 rhsIsLiveSlot164 又出现 ' + $lambda + ' 处 —— 同一决定两位主人') }
if ($bodyQ4 -eq '') {
    $bad += 'Q4 读不到 exprBorrowsBstrStorage 的本体'
} else {
    foreach ($n in @('VB6_SA_AT(BSTR,', 'VB6_SA_ND_AT2(BSTR,',
                     'kSaBstr167) == 0', 'kSaNdBstr167) == 0',
                     'b167TopIsCall', 'inferExprType', 'LiteralExpr')) {
        if ($bodyQ4.IndexOf($n) -lt 0) { $bad += ('Q4 本体里缺 ' + $n + ' —— 那一腿没了') }
    }
}
if ($hardTrue -ne $TrueFloor) {
    $bad += ('Q5 硬填「右值自带」的站点应 ' + $TrueFloor + ' 处, 实得 ' + $hardTrue + ' —— 新站点不许不问这一问就写死')
}

Write-Host ('Q1 def=' + $defQ1 + ' decl=' + $declQ1 + ' calls=' + $callQ1 +
    ' | Q2 def=' + $defQ2 + ' decl=' + $declQ2 +
    ' | Q3 asked=' + $asked + ' lambda=' + $lambda +
    ' | Q4 topiscall=' + $defTop +
    ' | Q5 hard-true=' + $hardTrue + '(floor ' + $TrueFloor + ')')
if ($bad.Count -gt 0) {
    Write-Host ('FAIL bstr_borrow_slot: ' + $bad.Count + ' 处违规') -ForegroundColor Red
    $bad | ForEach-Object { Write-Host ('   - ' + $_) }
    exit 1
}
Write-Host ('PASS bstr_borrow_slot: 那一问 1 枚定义 / ' + $callQ1 + ' 处调用 / 借用三腿齐 / 硬填站点 ' + $hardTrue) -ForegroundColor Green
exit 0
