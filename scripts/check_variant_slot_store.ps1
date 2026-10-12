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
# 四条规则:
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

$nHand = $handPairs.Count
if ($bad.Count -eq 0) {
    Write-Host ('PASS variant_slot_store: def 1 | calls ' + $callSites + ' | shapes ' + $nHand + ' | open stations ' + $bareTotal + ' | violations 0') -ForegroundColor Green
    exit 0
}
foreach ($b in $bad) { Write-Host ('FAIL ' + $b) -ForegroundColor Red }
exit 1
