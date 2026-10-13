# check_variant_slot_store.ps1 - 账 #305 §B164 (第五十五刀): 「把一枚值收进 Variant 槽」只许住一枚出口
#
# 为什么要这一道: 这个决定在 src\backend 里原本有两份答案 —— 普通赋值那一站发的是
# 「先清目标旧的持有, 再把右值搬进去」两行, 而 For Each 的循环变量只发了第二行(裸结构赋值)。
# 后果 = 每遍历一个元素就把上一轮那只 BSTR 忘了(实测 BASE 每元素漏 82/71 字节, 台账 §B164 形④),
# 与 §B163「一站两成因」、§B157「按名字当键必须带全身份」是同一族第三次撞。
#
# 第五十五刀把它收成 CCodeGen::emitVariantSlotStore 一枚, 并让它答**两半句**:
#   ① 放掉槽里原来那份       —— 不放就没人回收(本刀那 82/71 B/元素)
#   ② 槽对新值必须自己持有    —— 照抄借来的指针, 别人一释放这槽就悬垂; 而对借来的值补 ①,
#                              就等于 free 掉别人的内存(实测 arr-sum 820 变 813/815)
# rhsOwnsValue 答 ②: 集合/ParamArray 那三站的右值是刚 mint 的带权值(清+搬), 数组那一站交的是
# 数组自己那只 BSTR(清+深拷贝 = vb6_VariantAssign)。
#
# 五条规则 (R1-R4 = 第五十五刀; R5 = 第五十九刀, 账 #305 §B168):
#   R1  出口存在、本体就是那三形(借用->一次调用 / 自指->临时量+清+搬 / 直发->清+搬):
#       恰好 6 枚 emitLine、含「右值是借来的」那一路、体里不许 switch
#   R2  声明 + 定义各恰好一处, 且调用点 >= $CallFloor (少了 = 有人又开始自己抄)
#   R3  「清+搬」那一对 emitLine 的手抄形状只许出现在**定义出口的这份文件**里, 且恰好两形
#       (两行谁先谁后也由这条钉住: 先清后搬)
#   R4  还剩几处在裸发槽位赋值 —— 名单钉死, 新增即红; 真修掉一处要把名单同步改短(§B164 形⑤起)
param(
    [string]$Root = "",
    [int]$CallFloor = 7
)
$ErrorActionPreference = 'Continue'
if (-not $Root) { $Root = (Split-Path -Parent $PSScriptRoot) }
$backend = Join-Path $Root 'src\backend'
if (-not (Test-Path $backend)) { Write-Host ('FAIL 没有 ' + $backend); exit 2 }

# 本刀刻意没接线的三站(每一站都得先问「右值是不是借来的」, 不能盲改) + 已接线那一站的归零记录
$OPEN = @(
    # 事件实参: pName = vb6_VariantFromStackVARIANT(&args[i])
    @{ File = 'cgen_base_generate_evt_impl.inc'; N = 1 },
    # FnRetUdt 的 Variant 成员那一站本刀已接线(站⑥, 发码仍是「清+搬」) —— 裸发归零;
    # 但「这里的右值是不是借来的」按 Fix 232 那一问还没拍, 记在台账 §B164 形⑥
    @{ File = 'cgen_assign_stmt_special.inc'; N = 0 },
    # 这一份里的三处是 vb6_VariantTo*(从槽里读出, 不是写入) —— R4 的 (?!To) 已排除, 故 N = 0
    @{ File = 'cgen_assign_value_sem.inc'; N = 0 },
    # For 循环起点: varAcc = vb6_VariantFromValue(start)
    @{ File = 'cgen_control.cpp'; N = 1 },
    # Property Let 进槽: target = vb6_VariantFromValue(value)
    @{ File = 'cgen_setlet.cpp'; N = 1 }
)

$files = @(Get-ChildItem -Path $backend -Recurse -File -Include *.cpp, *.hpp, *.inc)
$bad = @()
$defFile = ''
$defBody = ''
$declCount = 0
$totalRefs = 0
$handPairs = @()
$bareByFile = @{}

$DEF_RE   = '(?m)^\s*void\s+CCodeGen::emitVariantSlotStore\s*\('
$DECL_RE  = '^\s*void\s+emitVariantSlotStore\s*\('
$PAIR_RE  = 'c_\.emitLine\("\s*vb6_VariantClear\(&" \+ (\w+) \+ "\);"\)[\s;]+c_\.emitLine\((?:"\s*" \+ )?\1 \+ "'
$BARE_RE  = 'c_\.emitLine\(\w+ \+ " = vb6_Variant(?!To)'

foreach ($f in $files) {
    $t = [System.IO.File]::ReadAllText($f.FullName)
    $refs = [regex]::Matches($t, 'emitVariantSlotStore')
    foreach ($p in [regex]::Matches($t, $PAIR_RE)) { $handPairs += $f.Name }
    foreach ($b in [regex]::Matches($t, $BARE_RE)) {
        $k = Split-Path $f.FullName -Leaf
        if (-not $bareByFile.ContainsKey($k)) { $bareByFile[$k] = 0 }
        $bareByFile[$k] = $bareByFile[$k] + 1
    }
    if ($refs.Count -eq 0) { continue }
    $totalRefs += $refs.Count
    if ([regex]::IsMatch($t, $DECL_RE, 'Multiline')) { $declCount++ }
    $dm = [regex]::Matches($t, $DEF_RE)
    if ($dm.Count -gt 1) { $bad += ('R1 ' + $f.Name + ': 出口定义了 ' + $dm.Count + ' 遍') }
    if ($dm.Count -eq 1) {
        if ($defFile -ne '') { $bad += ('R1 出口定义落在两份文件: ' + (Split-Path $defFile -Leaf) + ' + ' + $f.Name) }
        $defFile = $f.FullName
        $i = $t.IndexOf($dm[0].Value)
        $j = $t.IndexOf([char]10 + '}', $i)
        $defBody = $t.Substring($i, [Math]::Max(0, $j - $i))
    }
}

if ($defFile -eq '') { $bad += 'R1 src\backend 里找不到 emitVariantSlotStore 的定义 —— 那一问又没人管了' }
if ($declCount -ne 1) { $bad += ('R2 声明 found ' + $declCount + ' 处 (要恰好 1, 在 detail\util\cgen_helpers.inc)') }

$callSites = $totalRefs - $declCount - 1          # 减去声明与定义那两处
if ($callSites -lt $CallFloor) {
    $bad += ('R2 调用点 ' + $callSites + ' < ' + $CallFloor + ' —— 站点该问这一枚出口, 不该自己抄')
}
if ($defBody -ne '') {
    $n = ([regex]::Matches($defBody, 'c_\.emitLine')).Count
    if ($n -ne 6) { $bad += ('R1 出口本体有 ' + $n + ' 枚 emitLine (要恰好 6: 借用一路 1 + 自指一路 3 + 直发一路 2)') }
    if ($defBody.IndexOf('vb6_VariantAssign(&') -lt 0) { $bad += 'R1 出口缺了「右值是借来的」那一路 (vb6_VariantAssign)' }
    if ($defBody -match 'switch\s*\(') { $bad += 'R1 出口体里出现 switch —— 槽位写入不该再分档' }
}
$wantFile = Split-Path $defFile -Leaf
foreach ($g in ($handPairs | Group-Object)) {
    if ($g.Name -ne $wantFile -or $g.Count -ne 2) {
        $bad += ('R3 手抄「清+搬」出现在 ' + $g.Name + ' x' + $g.Count + ' (只许住 ' + $wantFile + ' x2: 直发 + 自指)')
    }
}
$bareTotal = 0
foreach ($v in $bareByFile.Values) { $bareTotal += $v }
foreach ($o in $OPEN) {
    $got = 0
    if ($bareByFile.ContainsKey($o.File)) { $got = $bareByFile[$o.File] }
    if ($got -ne $o.N) {
        $bad += ('R4 裸发槽位赋值 ' + $o.File + ' = ' + $got + ' 处, 名单钉的是 ' + $o.N + ' (新增即红; 修掉了就把名单改短)')
    }
}
foreach ($k in $bareByFile.Keys) {
    if (@($OPEN | Where-Object { $_.File -eq $k }).Count -eq 0) {
        $bad += ('R4 名单外的新站点 ' + $k + ' x' + $bareByFile[$k] + ' —— 那一问有人又开始自己抄了')
    }
}

# ---- R5 (账 #305 §B168, 第五十九刀): 「这条左值到底是不是 Variant 槽」的**左值侧**那一问 ----
# 赋值站那四条既有判据 (裸名 knownVariantVars_ / 本模块 classVariantMembers_ / 发出来的文本
# 像 VB6_SA_* / UDT 字段类型表) 问的全是「本模块认不认识这个名字」, 全都答不了**别的类**的
# 字段 —— 于是 `h.PV = 7` 发的是一句完全不装箱的裸赋值, cl C2440 = 整棵工程编不过。
# 第五十九刀把这一问收进 driver 注入的工程级「类 -> vb6_VARIANT 字段」表 + 一处查询出口。
#   R5a 查询出口恰好定义 1 遍, 且住在 cgen_api.inc (与 setProjectClassNames 同格)
#   R5b 调用点 >= 1 (赋值站④) —— 归零 = 那一问又没人管了
#   R5c 那张表只在 driver 的一趟预扫描里建 (恰好 1 份文件), 注入站点恰好 1 处
#   R5d 「认文本尾注」这条歧路不许复活: `class var .` 这份拼写逐文件钉死处数, 新增即红
#       (今天 11 处 / 6 份文件: 8 处是**生产**那句尾注的发码点, 3 处是讲这件事的注释 ——
#        注释也一起钉, 因为「谁在提这句文本」正是这一族重新长出来的地方)
#   R5e 本模块那张老表 classVariantMembers_ 的写者仍只住一份文件, 恰好 4 枚 insert
$QDEF_RE_168    = 'bool\s+classFieldIsVariantSlot\s*\('
$QCALL_RE_168   = 'classFieldIsVariantSlot\s*\('
$MAPDECL_RE_168 = 'std::unordered_map<std::string,\s*std::set<std::string>>\s+classVariantFieldMap\s*;'
$INJ_RE_168     = 'setClassVariantFieldMap\(&classVariantFieldMap\)'
$INS_RE_168     = 'classVariantMembers_\.insert'
$MARK_RE_168    = 'class var \.'
$srcFiles168 = @(Get-ChildItem -Path (Join-Path $Root 'src') -Recurse -File -Include *.cpp, *.hpp, *.inc)
$qDefs168 = 0; $qDefFiles168 = @(); $qAll168 = 0; $mapFiles168 = @(); $inj168 = 0
$markByFile168 = @{}; $insByFile168 = @{}
foreach ($f in $srcFiles168) {
    $t = [System.IO.File]::ReadAllText($f.FullName)
    $dm168 = [regex]::Matches($t, $QDEF_RE_168).Count
    if ($dm168 -gt 0) { $qDefs168 += $dm168; $qDefFiles168 += $f.Name }
    # 注释行里提一句名字不算站点 —— 逐行扫, 只数**代码行**上的调用
    foreach ($ln168 in ($t -split "`r?`n")) {
        $tr168 = $ln168.Trim()
        if ($tr168.StartsWith('//') -or $tr168.StartsWith('*')) { continue }
        $qAll168 += [regex]::Matches($ln168, $QCALL_RE_168).Count
    }
    if ([regex]::IsMatch($t, $MAPDECL_RE_168)) { $mapFiles168 += $f.Name }
    $inj168 += [regex]::Matches($t, $INJ_RE_168).Count
    $leaf168 = Split-Path $f.FullName -Leaf
    $mc168 = [regex]::Matches($t, $MARK_RE_168).Count
    if ($mc168 -gt 0) { $markByFile168[$leaf168] = $mc168 }
    $ic168 = [regex]::Matches($t, $INS_RE_168).Count
    if ($ic168 -gt 0) { $insByFile168[$leaf168] = $ic168 }
}
if ($qDefs168 -ne 1) { $bad += ('R5a classFieldIsVariantSlot 定义了 ' + $qDefs168 + ' 遍 (要恰好 1)') }
if ($qDefFiles168.Count -ne 1 -or $qDefFiles168[0] -ne 'cgen_api.inc') {
    $bad += ('R5a 查询出口住在 [' + ($qDefFiles168 -join ',') + '] (要住 cgen_api.inc)')
}
$qSites168 = $qAll168 - $qDefs168
if ($qSites168 -lt 1) { $bad += 'R5b 「对象.字段是不是 Variant 槽」这一问的调用点归零了' }
if ($mapFiles168.Count -ne 1 -or $mapFiles168[0] -ne 'driver_codegen_variantfield_scan.inc') {
    $bad += ('R5c 工程级表建在 [' + ($mapFiles168 -join ',') + '] (只许 driver_codegen_variantfield_scan.inc)')
}
if ($inj168 -ne 1) { $bad += ('R5c 表的注入站点 ' + $inj168 + ' 处 (要恰好 1)') }
$WantMark168 = [ordered]@{
    'cgen_expr_call_callee_member.inc'    = 1
    'cgen_expr_member_class_fallback.inc' = 3
    'cgen_expr_member_voidptr_com.inc'    = 4
    'cgen_state.inc'                      = 1
    'cgen_call.cpp'                       = 1
    'cgen_assign_value_sem.inc'           = 1
}
foreach ($k in $WantMark168.Keys) {
    $g168 = 0
    if ($markByFile168.ContainsKey($k)) { $g168 = $markByFile168[$k] }
    if ($g168 -ne $WantMark168[$k]) {
        $bad += ('R5d ' + $k + ' 里 class var . 出现 ' + $g168 + ' 处, 名单钉的是 ' + $WantMark168[$k])
    }
}
foreach ($k in $markByFile168.Keys) {
    if (-not $WantMark168.Contains($k)) {
        $bad += ('R5d 名单外的新文件 ' + $k + ' x' + $markByFile168[$k] + ' —— 又有人把那句尾注当判据了')
    }
}
$insFiles168 = @($insByFile168.Keys)
if ($insFiles168.Count -ne 1 -or $insFiles168[0] -ne 'cgen_base_generate_state_scan.inc' -or
        $insByFile168['cgen_base_generate_state_scan.inc'] -ne 4) {
    $bad += ('R5e classVariantMembers_ 的写者散在 [' + ($insFiles168 -join ',') + '] (只许 state_scan x4)')
}
$markTotal168 = 0
foreach ($v in $markByFile168.Values) { $markTotal168 += $v }

$nHand = $handPairs.Count
if ($bad.Count -eq 0) {
    Write-Host ('PASS variant_slot_store: def 1 | calls ' + $callSites + ' | shapes ' + $nHand + ' | open stations ' + $bareTotal + ' | slotask168 ' + $qSites168 + ' | mark168 ' + $markTotal168 + ' | violations 0') -ForegroundColor Green
    exit 0
}
foreach ($b in $bad) { Write-Host ('FAIL ' + $b) -ForegroundColor Red }
exit 1
