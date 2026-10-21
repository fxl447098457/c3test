# check_owned_temp_reclaim.ps1 - 账 #454 §B164 形①②③⑤ (第一百三十四刀):
#   「一条语句里现造的带权临时量」只许住一枚权威、一份名单、一处回收
#
# 为什么要这一道: 五形实测 (x86, 当前 HEAD 复算)
#   ① `n = Len("ab" & "cd")`          75.2 B/次   (拼接结果 + 两枚字面量, 三枚都没人 SysFreeString)
#   ② `n = Len(gArr(1) & "!")`        50.2 B/次
#   ③ `n = VarType(gColl(2))`         25.6 B/次   (Collection 取回那枚带权副本)
#   ⑤ `n = Len(v)`  (v 是数值 Variant) 24.8 B/次  (换算出口现造的临时串)
#   ④ For Each 循环变量 = 0  ← 第五十五刀已出, 本格只是别把它改坏
# 三形的共同点 = **结果没落到任何变量上**, 于是从没进过发码的回收名单。
#
# 口径 = ① 语句级**带档**名单 (先例 M22 `ansiTempsToFree_`): 现造的带权临时量提成具名
# 变量、登记名单、由 emitStmt 在这条语句末按档收回。与 VB6「临时量的寿命就是这一条语句」
# 同档 ⇒ 不会重演 §B165 那次「改问借用权威反而悬空」。
# **带档**是硬要求: ①②⑤ 交 BSTR (vb6_BSTR_Free)、③ 交 vb6_VARIANT (vb6_VariantClear);
# 只发一种就会在另一档上继续漏, 或把 VARIANT 当 BSTR 解 ⇒ 读越界。
#
# 八条规则
#   R1  「这段发码文本是不是现造的带权值、是 BSTR 还是 VARIANT」只许一枚权威: 定义 1 遍
#       (cgen_util_ctrl.cpp) + 声明 1 遍 (cgen_helpers.inc); 体里必须**转问**既有的
#       exprBorrowsBstrStorage —— 自己再抄一份 topIsCall = 两份答案
#   R2  提名落地 hoistOwnedTemp 同样定义 1 / 声明 1, 且调用点 >= $CallFloor
#   R3  名单的 push_back 只许 1 处, 且只许住 hoistOwnedTemp 本体那份文件 ——
#       站点自己登记 = 名单的寿命不再由出口管
#   R4  名单声明只许 1 处 (cgen_state.inc), 两枚档常量各 1 枚
#   R5  语句末那两枚 free 只许住 cgen_stmt.cpp, 两档各 1 处, 且**带层号守卫**
#       (声明若发在一对已合上的花括号里 —— Debug.Print 那族 `{ ... }` —— 宁可不收,
#        不许发成引用越界的 C; 与 boxTempsToClear_ 同一条纪律)
#   R6  具名临时量的声明行只许由出口写 (`BSTR _own_` / `vb6_VARIANT _own_` 字面量只许住
#       出口那份文件) —— 站点自己拼 = 第三条路
#   R7  过程入口 / 出口各清一次名单, 恰好 6 处 (func/proc/prop × 2)
#   R8  拼接站不许回到「文本里出现 vb6_BSTR_Concat 子串」那条旧判据 —— 那是**子串**搜索,
#       左操作数是 `vb6_Len(vb6_BSTR_Concat(...))` 这种顶层不是 BSTR 的形状时会把一枚
#       Long 当 BSTR 放掉。现在只许问权威。
param(
    [string]$Root = "",
    [int]$CallFloor = 2
)
$ErrorActionPreference = 'Continue'
if (-not $Root) { $Root = (Split-Path -Parent $PSScriptRoot) }
$src = Join-Path $Root 'src'
if (-not (Test-Path $src)) { Write-Host ('FAIL 没有 ' + $src); exit 2 }

$files = @(Get-ChildItem -Path $src -Recurse -File -Include *.cpp, *.hpp, *.inc)
$bad = @()

function CodeLines([string]$t) {
    # 注释行不算站点 —— 逐行扫, 只数代码行上的出现
    $out = @()
    foreach ($ln in ($t -split "`r?`n")) {
        $s = $ln.Trim()
        if ($s.StartsWith('//') -or $s.StartsWith('*') -or $s.StartsWith('/*')) { continue }
        $out += $ln
    }
    return $out
}

$kindDef = 0; $kindDecl = 0; $kindCalls = 0; $kindBodyAsks = 0
$hoistDef = 0; $hoistDecl = 0; $hoistCalls = 0
$pushFiles = @{}; $pushTotal = 0
$listDecls = 0; $kindConsts = 0
$drainBstr = @{}; $drainVar = @{}; $guardOk = 0
$declLit = @{}; $clearFiles = @{}
$concatTextJudge = @()

foreach ($f in $files) {
    $t = [System.IO.File]::ReadAllText($f.FullName)
    $leaf = Split-Path $f.FullName -Leaf
    $code = CodeLines $t

    $kindDef  += [regex]::Matches($t, 'int\s+CCodeGen::exprOwnedTempKind\s*\(').Count
    $kindDecl += [regex]::Matches($t, '(?m)^\s*int\s+exprOwnedTempKind\s*\(').Count
    $hoistDef += [regex]::Matches($t, 'std::string\s+CCodeGen::hoistOwnedTemp\s*\(').Count
    $hoistDecl += [regex]::Matches($t, '(?m)^\s*std::string\s+hoistOwnedTemp\s*\(').Count

    foreach ($ln in $code) {
        $kindCalls  += [regex]::Matches($ln, 'exprOwnedTempKind\s*\(').Count
        $hoistCalls += [regex]::Matches($ln, 'hoistOwnedTemp\s*\(').Count
    }

    $pb = [regex]::Matches($t, 'ownedTempsToFree_\.push_back').Count
    if ($pb -gt 0) { $pushFiles[$leaf] = $pb; $pushTotal += $pb }

    $ld = [regex]::Matches($t, 'std::vector<OwnedTemp>\s+ownedTempsToFree_').Count
    $listDecls += $ld
    $kindConsts += [regex]::Matches($t, 'static const int kOwnedTemp(Bstr|Variant)').Count

    $db = [regex]::Matches($t, 'c_\.emitLine\("vb6_BSTR_Free\("').Count
    $dv = [regex]::Matches($t, 'c_\.emitLine\("vb6_VariantClear\(&"').Count
    if ($db -gt 0) { $drainBstr[$leaf] = $db }
    if ($dv -gt 0) { $drainVar[$leaf] = $dv }

    $dl = [regex]::Matches($t, '"BSTR _own_"').Count + [regex]::Matches($t, '"vb6_VARIANT " \+ nm').Count
    if ($dl -gt 0) { $declLit[$leaf] = $dl }

    $cl = [regex]::Matches($t, 'ownedTempsToFree_\.clear\(\);').Count
    if ($cl -gt 0) { $clearFiles[$leaf] = $cl }

    # R8: 拼接站不许再用子串判据
    foreach ($ln in $code) {
        if ($ln -match 'left\.find\("vb6_BSTR_Concat"\)' -or $ln -match 'leftIsConcat') {
            $concatTextJudge += ($leaf + ': ' + $ln.Trim())
        }
    }
    # R5 层号守卫: 语句末那段里必须出现 level 比较
    if ($leaf -eq 'cgen_stmt.cpp') {
        if ($t -match 'ot\.level\s*!=\s*c_\.indentLevel\(\)') { $guardOk = 1 }
    }
}

# ---- R1 唯一权威 ----
if ($kindDef -ne 1) { $bad += ('R1 exprOwnedTempKind 定义了 ' + $kindDef + ' 遍 (要恰好 1, 在 cgen_util_ctrl.cpp)') }
if ($kindDecl -ne 1) { $bad += ('R1 exprOwnedTempKind 声明了 ' + $kindDecl + ' 处 (要恰好 1, 在 cgen_helpers.inc)') }
$kindBody = ''
$ctrl = Join-Path $src 'backend\cgen_util_ctrl.cpp'
if (Test-Path $ctrl) {
    $tc = [System.IO.File]::ReadAllText($ctrl)
    $m = [regex]::Match($tc, '(?s)int\s+CCodeGen::exprOwnedTempKind\s*\(.*?\n\}')
    if ($m.Success) {
        $kindBody = $m.Value
        if ($kindBody -notmatch 'exprBorrowsBstrStorage\(') {
            $bad += 'R1 权威体里没有转问 exprBorrowsBstrStorage —— 自己抄一份"借不借"就是两份答案'
        }
        if ($kindBody -notmatch 'variantRhsOwnsValue\(') {
            $bad += 'R1 权威体里没有转问 variantRhsOwnsValue —— VARIANT 档必须问它(它连"右值是一枚活槽"一起答)'
        }
        # 两枚"不惜"闸: 顶层必须是调用(裸标识符/裸字面量到此还没变成 BSTR), 四兄弟 COM 结果
        # 交的是 void* 不是 BSTR —— 这两条任一失守, 产物里就会出现 `BSTR t = L"ab";`
        # (随后 SysFreeString 字面量 = 堆写坏) 或拿 VARIANT* 当 BSTR 放掉。
        if ($kindBody -notmatch 'b167TopIsCall\(') {
            $bad += 'R1 权威体里没有"顶层必须是调用"那道闸 (b167TopIsCall) —— 裸字面量会被当带权串放掉'
        }
        if ($kindBody -notmatch 'vb6_ComGetProp') {
            $bad += 'R1 权威体里没有排除四兄弟 COM 结果 (vb6_ComCall/ComCallByDispid/ComGetProp/ComGetObjectProp) —— 它们交的是 void*'
        }
    } else { $bad += 'R1 取不到 exprOwnedTempKind 的函数体' }
}
# ---- R2 提名落地 ----
if ($hoistDef -ne 1) { $bad += ('R2 hoistOwnedTemp 定义了 ' + $hoistDef + ' 遍 (要恰好 1)') }
if ($hoistDecl -ne 1) { $bad += ('R2 hoistOwnedTemp 声明了 ' + $hoistDecl + ' 处 (要恰好 1)') }
$hoistSites = $hoistCalls - $hoistDef - $hoistDecl
if ($hoistSites -lt $CallFloor) {
    $bad += ('R2 hoistOwnedTemp 调用点 ' + $hoistSites + ' < ' + $CallFloor + ' (拼接站 + 实参站; 少了 = 有人又不回收了)')
}
# ---- R3 登记处唯一 ----
if ($pushTotal -ne 1) { $bad += ('R3 ownedTempsToFree_.push_back 共 ' + $pushTotal + ' 处 (要恰好 1)') }
if ($pushFiles.Count -gt 0 -and -not $pushFiles.ContainsKey('cgen_util_ctrl.cpp')) {
    $bad += ('R3 登记处住在 [' + ($pushFiles.Keys -join ',') + '] (只许住 hoistOwnedTemp 本体那份文件)')
}
# ---- R4 名单与档 ----
if ($listDecls -ne 1) { $bad += ('R4 ownedTempsToFree_ 声明了 ' + $listDecls + ' 处 (要恰好 1, 在 cgen_state.inc)') }
if ($kindConsts -ne 2) { $bad += ('R4 档常量定义了 ' + $kindConsts + ' 枚 (要恰好 2: kOwnedTempBstr / kOwnedTempVariant)') }
# ---- R5 语句末回收 ----
$wantB = 1
$gb = 0; if ($drainBstr.ContainsKey('cgen_stmt.cpp')) { $gb = $drainBstr['cgen_stmt.cpp'] }
$gv = 0; if ($drainVar.ContainsKey('cgen_stmt.cpp')) { $gv = $drainVar['cgen_stmt.cpp'] }
if ($gb -ne $wantB) { $bad += ('R5 cgen_stmt.cpp 里 vb6_BSTR_Free 的发射 ' + $gb + ' 处 (要恰好 ' + $wantB + ')') }
if ($gv -lt 1)     { $bad += ('R5 cgen_stmt.cpp 里 vb6_VariantClear 的发射 ' + $gv + ' 处 (要 >= 1: §B217 那枚 + 本刀带档那枚)') }
foreach ($k in $drainBstr.Keys) { if ($k -ne 'cgen_stmt.cpp') { $bad += ('R5 名单外的 BSTR_Free 发射点 ' + $k + ' x' + $drainBstr[$k]) } }
if ($guardOk -ne 1) { $bad += 'R5 语句末那段缺少层号守卫 (ot.level != c_.indentLevel()) —— 声明落在已合上的花括号里就会发成越界的 C' }
# ---- R6 声明行只许出口写 ----
foreach ($k in $declLit.Keys) { if ($k -ne 'cgen_util_ctrl.cpp') { $bad += ('R6 名单外的新声明点 ' + $k + ' x' + $declLit[$k] + ' —— 具名临时量又长了第三条路') } }
if (-not $declLit.ContainsKey('cgen_util_ctrl.cpp')) { $bad += 'R6 出口里找不到 _own_ 声明行的写法' }
# ---- R7 过程出入口清名单 ----
$clearTotal = 0
foreach ($v in $clearFiles.Values) { $clearTotal += $v }
if ($clearTotal -ne 7) {
    $bad += ('R7 ownedTempsToFree_.clear() 共 ' + $clearTotal + ' 处 (要恰好 7: func/proc/prop × 入口/出口 6 处 + 语句末回收后那 1 处)')
}
# ---- R8 拼接站不许回到子串判据 ----
if ($concatTextJudge.Count -ne 0) {
    foreach ($c in $concatTextJudge) { $bad += ('R8 拼接站又用文本子串判据: ' + $c) }
}

if ($bad.Count -eq 0) {
    Write-Host ('PASS owned_temp_reclaim: auth ' + $kindDef + '/' + $kindDecl + ' | hoist ' + $hoistSites + ' | push ' + $pushTotal + ' | list ' + $listDecls + ' | kinds ' + $kindConsts + ' | drain B' + $gb + '/V' + $gv + ' | guard ' + $guardOk + ' | clears ' + $clearTotal + ' | violations 0') -ForegroundColor Green
    exit 0
}
foreach ($b in $bad) { Write-Host ('FAIL ' + $b) -ForegroundColor Red }
exit 1
