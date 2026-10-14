# check_implicit_cstr_station.ps1 —— 「数值实参交进 VB 的字符串形参 ⇒ 按 VB6 的隐式 CStr 折」这一条
# 规则的站点 / 名单 / 形状对账（账 #305 §B174，第六十五刀）
#
# 为什么要有这一道：这条规则在发码侧原本住**两家**——
#   Fix 175 (src/backend/detail/expr/cgen_expr_call_arg_emit.inc) 问**形参表** (ByVal String)，
#          形状走 wrapToBSTR；
#   Fix 108c (src/backend/detail/expr/cgen_expr_call_builtin_argtype.inc) 靠一张 13 枚名单，
#          自己写了一遍折算 if/else，而且做在 argList **已拼成一整串之后** —— 找回第 N 个实参
#          只能靠"从上次命中往后数子串"这种对齐。
# 两家的档位清单从不汇合（一家有 Byte / LongPtr，另一家有 SByte / UInteger / ULong / ULongLong /
# Decimal），Boolean 那一档还各自给出两种答案（wrapToBSTR 的 "True" 与名单自己写的 "-1"）。
# 第六十五刀收成一位，并补上这一位以前管不着的那一族（**故意没有形参表**的 InStr：位号答在
# builtinNoParamTableStringSlot 一处）。这一道钉的就是"别再长出第二位、别再把形状就地重写"。
#
# 判据（任何一条不齐就红）：
#   R1 站点唯一：typeTakesImplicitCStr 的**调用点**恰好 1 处，且住在 cgen_expr_call_arg_emit.inc
#      （声明 cgen_helpers.inc + 定义 cgen_util_type.cpp 各 1 处，是同一枚问句的两半，不算调用）。
#   R2 位号表唯一：builtinNoParamTableStringSlot 同一条，也必须恰好一处调用、同一文件。
#   R3 旧名单不许回来：strArgPos108c 在 src/ 里 0 处。
#   R4 那一站不再发形状：cgen_expr_call_builtin_argtype.inc 的**代码**里不许出现 vb6_CStr。
#   R5 名单自己的 cast 形状不许在别处重现：vb6_CStrLong((int32_t)( 在 src/backend 里 0 处。
#   R6 census 不许读空：档位闸的 case >= 14、**位号表认到的内在函数名 >= 11**（第六十六刀
#      从 1 枚扩到 11 枚 = InStr + 注册表四枚 + MsgBox / InputBox / SendKeys / SetAttr /
#      SavePicture / Format），arg_emit 里 wrapToBSTR 的调用 >= 1（形状出口还在被问）。
#   R7 第二位主人不许回来：cgen_expr_call_conv_output.inc 的**代码**里 wrapToBSTR 0 处 ——
#      那一站曾经按 args[0] 的文本前缀连判六条自己折 MsgBox 的 prompt（§B175 量过：它在自己
#      管的位上折得对，但**只管 prompt 一位**，标题位今天仍裸交 double）。同时钉
#      'callee = "vb6_MsgBox1"' 恰好 1 处 —— 那一形是 arity 改道，问的是另一件事，必须还在。
#
#   R11 字节数组的两位主人登记路数相等（各 7 条：模块级 2 + 局部 2 + 形参 3）。
#   R12 字节数组那两站的剥壳闸 = 2 条 `>= 4`，且 builtin_fixup 里不许再留 `> 4`。
# 用法: powershell -File scripts\check_implicit_cstr_station.ps1 [-RootOverride <目录>]  (PASS = exit 0)
#   -RootOverride 是给负控用的: 把 src\backend 抄到别处再判, 真树一字不动。

param([string]$RootOverride = '')
$ErrorActionPreference = 'Stop'
$Root = if ($RootOverride) { $RootOverride } else { Split-Path -Parent $PSScriptRoot }
$backend = Join-Path $Root 'src\backend'
$utilCpp = Join-Path $backend 'cgen_util_type.cpp'
$helpers = Join-Path $backend 'detail\util\cgen_helpers.inc'
$emitInc = Join-Path $backend 'detail\expr\cgen_expr_call_arg_emit.inc'
$argtypeInc = Join-Path $backend 'detail\expr\cgen_expr_call_builtin_argtype.inc'
$convInc    = Join-Path $backend 'detail\expr\cgen_expr_call_conv_output.inc'
$argVariantInc = Join-Path $backend 'detail\expr\cgen_expr_call_arg_variant.inc'
foreach ($p in @($utilCpp, $helpers, $emitInc, $argtypeInc, $convInc, $argVariantInc)) {
    if (-not (Test-Path -LiteralPath $p)) { Write-Host ('FAIL: 读不到 ' + $p); exit 1 }
}

function Strip-CComments([string]$t) {
    $t = [regex]::Replace($t, '/\*[\s\S]*?\*/', ' ')
    $t = [regex]::Replace($t, '(?m)//.*$', ' ')
    return $t
}

# 取一段花括号平衡的块体 (start 指向块首 '{' 的位置)
function Get-BlockBody([string]$t, [int]$start) {
    $depth = 0
    for ($j = $start; $j -lt $t.Length; $j++) {
        $ch = $t[$j]
        if ($ch -eq '{') { $depth++ }
        elseif ($ch -eq '}') {
            $depth--
            if ($depth -eq 0) { return $t.Substring($start + 1, $j - $start - 1) }
        }
    }
    return $null
}

$allFiles = @(Get-ChildItem -Path $backend -Recurse -File |
              Where-Object { $_.Extension -eq '.cpp' -or $_.Extension -eq '.inc' })
$codeByFile = @{}
foreach ($f in $allFiles) { $codeByFile[$f.FullName] = Strip-CComments ([IO.File]::ReadAllText($f.FullName)) }
$util = $codeByFile[$utilCpp]
$emit = $codeByFile[$emitInc]
$argtype = $codeByFile[$argtypeInc]
$argVariant = $codeByFile[$argVariantInc]
$allCode = ($codeByFile.Values -join [Environment]::NewLine)

$srcRaw = ''
foreach ($f in @(Get-ChildItem -Path (Join-Path $Root 'src') -Recurse -File |
                 Where-Object { $_.Extension -eq '.cpp' -or $_.Extension -eq '.inc' -or $_.Extension -eq '.h' })) {
    $srcRaw += [IO.File]::ReadAllText($f.FullName) + [Environment]::NewLine
}

# 调用点 = 名字后面紧跟 '(' ，且前面不是"定义 (CCodeGen::)"也不是"声明 (bool / std::string)"
function Get-CallSites([string]$name) {
    $rx = [regex]([regex]::Escape($name) + '\(')
    $hits = @()
    foreach ($path in $codeByFile.Keys) {
        $t = $codeByFile[$path]
        foreach ($m in $rx.Matches($t)) {
            # $m.Index 指的是**名字**那一处, 所以 $pre 以名字前的那个词收尾:
            # 定义 = "…bool CCodeGen::" / 声明 = "…bool " —— 两者都不算调用点。
            $pre = $t.Substring(0, $m.Index)
            if ($pre -match 'CCodeGen::\s*$') { continue }
            if ($pre -match '(bool|std::string|size_t)\s+$') { continue }
            $hits += (Split-Path -Leaf $path)
        }
    }
    return $hits
}

$viol = @()

# ---- R1 / R2 ----
foreach ($pair in @(@('typeTakesImplicitCStr', 'cgen_expr_call_arg_emit.inc'),
                    @('builtinNoParamTableStringSlot', 'cgen_expr_call_arg_emit.inc'))) {
    $nm = $pair[0]
    $want = $pair[1]
    $sites = @(Get-CallSites $nm)
    if ($sites.Count -ne 1) {
        $viol += ('R1/R2 ' + $nm + ' 的调用点数应为 1, 实为 ' + $sites.Count +
                  ' (' + (($sites | Sort-Object -Unique) -join ', ') +
                  ') —— 这条规则又长出第二位主人了')
    } elseif ($sites[0] -ne $want) {
        $viol += ('R1/R2 ' + $nm + ' 的唯一调用点应在 ' + $want + ', 现在在 ' + $sites[0])
    }
}

# ---- R3 ----
if ($srcRaw.Contains('strArgPos108c')) {
    $viol += 'R3 strArgPos108c 又出现了 (那张 13 枚名单已并入 Fix 175 + 位号表, 见 §B174)'
}

# ---- R4 ----
if ([regex]::IsMatch($argtype, 'vb6_CStr')) {
    $viol += ('R4 cgen_expr_call_builtin_argtype.inc 的代码里出现 vb6_CStr* —— 数值折串的形状' +
              '只许出自 wrapToBSTR 一处 (这一站现在只该管裸 COM 结果解包 / ObjPtr 取值)')
}

# ---- R5 ----
$castN = @([regex]::Matches($allCode, [regex]::Escape('vb6_CStrLong((int32_t)('))).Count
if ($castN -ne 0) {
    $viol += ('R5 vb6_CStrLong((int32_t)( 在 src/backend 出现 ' + $castN +
              ' 处, 应为 0 —— 那是旧名单自己的形状, 与 wrapToBSTR 不同口径')
}

# ---- R6 ----
$gateCases = -1
$gateDef = [regex]::Match($util, 'bool CCodeGen::typeTakesImplicitCStr[^\{]*\{')
if ($gateDef.Success) {
    $gateBody = Get-BlockBody $util ($gateDef.Index + $gateDef.Value.Length - 1)
    if ($gateBody) { $gateCases = @([regex]::Matches($gateBody, 'case Vb6Type::')).Count }
}
if ($gateCases -lt 14) {
    $viol += ('R6 档位闸读出 ' + $gateCases + ' 档 (< 14) —— 两位旧主人的清单并集是 14 档, ' +
              '少一档就是 Left/Replace 那类实参重新裸交给 BSTR')
}
# 位号表的覆盖面 = 两条形状各数一次：InStr 那一枚带 arity 判断 (callee == "vb6_InStr")，
# 其余十枚住在 kNoTableFormalKinds176 那张档表里 ({"vb6_GetSetting", {"BSTR",…}}, ...)。
# 账 #305 §B176: 这张表从「哪几位是字符串」升成「第 pos 位要什么 C 档」，是这一问唯一的主人；
# `builtinNoParamTableStringSlot` 只剩一行投影 (== "BSTR")，所以名单必须按**档表**那一段数。
$slotDef = [regex]::Match($util, 'std::string CCodeGen::builtinNoParamTableFormalCType[^\{]*\{')
$tableNames = 0
if ($slotDef.Success) {
    $slotBody = Get-BlockBody $util ($slotDef.Index + $slotDef.Value.Length - 1)
    if ($slotBody) {
        $tableNames = @([regex]::Matches($slotBody, 'callee == "vb6_[A-Za-z]+"')).Count
        $tableNames += @([regex]::Matches($slotBody, '\{"vb6_[A-Za-z]+",\s*\{')).Count
    }
}
if ($tableNames -lt 11) {
    $viol += ('R6 位号表只认到 ' + $tableNames + ' 枚内在函数名 (< 11) —— 少一枚就是那一族' +
              '的字符串形参位又没人答 (注册表那四位今天语料命中 461 处数字字面量, §B175)')
}
$wrapN = @([regex]::Matches($emit, 'wrapToBSTR\(')).Count
if ($wrapN -lt 1) {
    $viol += 'R6 cgen_expr_call_arg_emit.inc 里不再问 wrapToBSTR —— 折算形状那一站断了'
}

# ---- R7 ----
$conv = $codeByFile[$convInc]
$convWrap = @([regex]::Matches($conv, 'wrapToBSTR\(')).Count
$convRename = @([regex]::Matches($conv, 'callee = "vb6_MsgBox1"')).Count
if ($convWrap -ne 0) {
    $viol += ('R7 cgen_expr_call_conv_output.inc 的代码里又有 ' + $convWrap +
              ' 处 wrapToBSTR —— MsgBox 那一形又长出第二位主人 (它只吃文本前缀, 且只管' +
              ' prompt 一位; 位号与形状都问 Fix 175 那一站, 见 §B175)')
}
if ($convRename -ne 1) {
    $viol += ('R7 conv_output 里的 arity 改道 (callee = "vb6_MsgBox1") 应为 1 处, 实为 ' +
              $convRename + ' 处 —— 那一问与形参档位无关, 撤了就没人补 1/2/3 参三形')
}

# ---- R8 (账 #305 §B176, 第六十七刀) ----
# 「没有形参表的那一族, 第 pos 位要什么 C 档」= 一枚出口, 读者只许两位:
#   cgen_util_type.cpp 里那行投影 (`== "BSTR"`) + arg_variant 那条臂 (Fix 038b-2)。
# 同时钉住: 那张按 callee 名登的 C 签名表 (`getRuntimeParamCType`) 只剩**一枚回落读者**,
# 它的行是按 VB 写出来的 arity 登的 —— 谁再拿它答位号, 就会把 InStr 的 int32_t start 位答成 BSTR。
$kindSites = @(Get-CallSites 'builtinNoParamTableFormalCType')
$kindWant = @('cgen_expr_call_arg_variant.inc', 'cgen_util_type.cpp')
if ($kindSites.Count -ne 2) {
    $viol += ('R8 builtinNoParamTableFormalCType 的调用点数应为 2 (投影 + arg_variant), 实为 ' +
              $kindSites.Count + ' (' + (($kindSites | Sort-Object -Unique) -join ', ') +
              ') —— 这一问又长出第三位读者/主人了')
} elseif ((($kindSites | Sort-Object -Unique) -join ',') -ne ($kindWant -join ',')) {
    $viol += ('R8 档表的读者应是 ' + ($kindWant -join ' + ') + ', 现在是 ' +
              (($kindSites | Sort-Object -Unique) -join ', '))
}
$rtSites = @(Get-CallSites 'getRuntimeParamCType')
if ($rtSites.Count -ne 1 -or $rtSites[0] -ne 'cgen_expr_call_arg_variant.inc') {
    $viol += ('R8 getRuntimeParamCType 的调用点应只剩 1 处 (arg_variant 的回落), 实为 ' +
              $rtSites.Count + ' (' + (($rtSites | Sort-Object -Unique) -join ', ') +
              ') —— 那张 C 签名表不许再答位号 (§B176 形② 就是这么来的)')
}
if (-not [regex]::IsMatch($argVariant, 'builtinNoParamTableFormalCType\(')) {
    $viol += 'R8 arg_variant 那条臂不再问档表 —— 位号又回到只认 C 签名表的状态 (§B176)'
}

# ---- R9 (账 #305 §B177, 第六十八刀) ----
# needle 位 = RTL 那枚 _Generic 宏的**分派位**, 它一位两答: 数值实参要折成串 (Fix 175 那一站),
# Variant 实参要**裸交** (宏自己走 vb6_InStrVar 解包并 free)。两答必须同源 —— 位号只许在
# builtinNoParamTableDispatchSlot 里数一遍, 否则下一刀又会像六十七刀那样「改一答、掉一答」。
$rawSites = @(Get-CallSites 'builtinNoParamTableVariantMayStayRaw')
if ($rawSites.Count -ne 1 -or $rawSites[0] -ne 'cgen_expr_call_arg_variant.inc') {
    $viol += ('R9 「这一位的 Variant 可否裸交」的调用点应恰好 1 处 (arg_variant), 实为 ' +
              $rawSites.Count + ' (' + (($rawSites | Sort-Object -Unique) -join ', ') +
              ') —— 这一问又长出第二位主人')
}
$slotSites = @(Get-CallSites 'builtinNoParamTableDispatchSlot')
if ($slotSites.Count -ne 2 -or (($slotSites | Sort-Object -Unique) -join ',') -ne 'cgen_util_type.cpp') {
    $viol += ('R9 分派位那一问的调用点应恰好 2 处且都在 cgen_util_type.cpp (档 + 可否裸交两枚投影), ' +
              '实为 ' + $slotSites.Count + ' (' + (($slotSites | Sort-Object -Unique) -join ', ') +
              ') —— 位号被抄了第二遍, §B176 形② 就是这么长回来的')
}
$arityN = @([regex]::Matches($util, 'firstArgType\s*==\s*Vb6Type::String')).Count
if ($arityN -ne 1) {
    $viol += ('R9 cgen_util_type.cpp 里「首参是不是串」那一问出现 ' + $arityN +
              ' 次, 应为 1 次 —— InStr 的位号只能有一位主人')
}
$slotWantsWant = @('cgen_expr_call_arg_variant.inc', 'cgen_expr_call_builtin_fixup.inc') -join ','

# ---- R10 (账 #305 §B180, 第六十九刀) ----
# 「这一位要不要收**整枚槽**而不是收串」= 一枚问句，读者**恰好两枚**（arg_variant 撤回 Variant→BSTR
# 的解包 / builtin_fixup 照着同一问句把 callee 换成收槽的出口 vb6_StrPtrVar）。两站判据必须同源 ——
# 只改一头就会造出「发码把整枚槽交给 void* vb6_StrPtr(BSTR)」(C2440) 或「callee 没改名、槽被解包
# 又现造一枚临时串」(本刀要撤的那条) 两种半边病。
# 它刻意**不进**上面那张按位号答档的表：`vb6_StrPtr(` 在全语料 1720 处里确定不是现造串的有 953 处，
# 另有 535 处这次逐枚读过（数组元素左值 / 解引用 / 返回 BSTR 的调用），只有 41 处是槽 ——
# 逐枚分解与订正见台账 §B180；无条件按位号答槽会把那 953 处一并打成 C2440。
# 变量（`.build/k69_strptr_census.txt`），无条件按位号答槽会把它们一并打成 C2440。
$slotWants = @(Get-CallSites 'builtinWantsRawSlotFromVariant')
if ($slotWants.Count -ne 2) {
    $viol += ('R10 「这一位要不要收整枚槽」的调用点应为 2 (arg_variant 撤回解包 + builtin_fixup 改名), ' +
              '实为 ' + $slotWants.Count + ' (' + (($slotWants | Sort-Object -Unique) -join ', ') +
              ') —— 两站又开始各判各的')
} elseif ((($slotWants | Sort-Object -Unique) -join ',') -ne $slotWantsWant) {
    $viol += ('R10 这一问的读者应是 arg_variant + builtin_fixup, 现在是 ' +
              (($slotWants | Sort-Object -Unique) -join ', '))
}
$strPtrVarN = @([regex]::Matches($allCode, [regex]::Escape('vb6_StrPtrVar'))).Count
if ($strPtrVarN -ne 1) {
    $viol += ('R10 vb6_StrPtrVar 在 src/backend 出现 ' + $strPtrVarN + ' 处, 应为 1 处 —— ' +
              '改道只许在 builtin_fixup 那一处发生, 别在第二站再拼一次出口名')
}

# ---- R11 (账 #305 §B181, 第七十一刀) ----
# 「这一枚实参是不是字节数组」有两位主人：`arrayElemTypes_`（按名字答元素档，六条声明路都填、**含形参**）
# 与 `knownByteArrayVars_` ∪ `classByteArrayMembers_`（Fix 140 那两站查的文本名单）。第七十一刀之前
# 名单缺「形参」整条路 ⇒ 同一枚数组在 `StrPtr(局部)` 与 `StrPtr(形参)` 下交出两种指针（后者把 SafeArray
# **描述符**地址交给要 BSTR 的 Win32 形参，cl 在 C 里只警告）。现在补齐，于是两位主人的**登记路数**
# 必须相等：以后谁再添一条声明路而只填一张表，这一条就红。
$aeWriters = @([regex]::Matches($allCode, 'arrayElemTypes_\[\s*\w+\s*\]\s*=\s*elemType')).Count
$baWriters = @([regex]::Matches($allCode, 'knownByteArrayVars_\.insert')).Count
if ($aeWriters -ne 7 -or $baWriters -ne 7) {
    $viol += ('R11 数组元素档的登记路数 = ' + $aeWriters + '，字节数组名单的登记路数 = ' + $baWriters +
              '，两者应各为 7（模块级 2 + 局部 2 + 形参 3）—— 一张表多一条腿而另一张没有，' +
              '就是「StrPtr(形参) 交出描述符指针」那一族静默病的成因')
}

# ---- R12 (账 #305 §B181, 第七十一刀) ----
# 剥 `(*x)` 这层壳的规则在 src/backend 里有 **24 位主人**，长度闸却写着三种（`> 2` / `> 3` / `> 4`）
# ⇒ 同一个问题在不同站点答案不一样，而 `> 4` 那批对**单字母名**整片失灵（`(*b)` 正好 4 个字符）。
# 收成一处是下一格；本刀先把字节数组那两站（`vb6_StrPtr` / `vb6_StrConv` 的改名）的闸写成 `>= 4`，
# 这里钉两件事：那两站各一条 `>= 4`，且 builtin_fixup 里**不许**再留 `> 4` 的剥壳闸。
$fixupPath = Join-Path $backend 'detail\expr\cgen_expr_call_builtin_fixup.inc'
$fixupCode = if ($codeByFile.ContainsKey($fixupPath)) { $codeByFile[$fixupPath] } else { '' }
$derefGe4 = @([regex]::Matches($fixupCode, '\.size\(\)\s*>=\s*4\s*&&\s*\w+\[0\]\s*==\s*''\(''')).Count
if ($derefGe4 -ne 2) {
    $viol += ('R12 字节数组那两站的 (*x) 剥壳闸应有 2 条 `>= 4`，实为 ' + $derefGe4 +
              ' 条 —— 闸退回 `> 4` 会让单字母形参连查表那一步都没进（病形 = 描述符指针交给要 BSTR 的形参）')
}
$derefGt4Fixup = @([regex]::Matches($fixupCode, '\.size\(\)\s*>\s*4\s*&&\s*\w+\[0\]\s*==\s*''\(''')).Count
if ($derefGt4Fixup -ne 0) {
    $viol += ('R12 builtin_fixup 里还剩 ' + $derefGt4Fixup + ' 条 `> 4` 的 (*x) 剥壳闸：同一条规则的两半' +
              '不许一个修一个不修')
}

Write-Host ('census: backendFiles=' + $allFiles.Count +
            ' gateCases=' + $gateCases +
            ' slotTableNames=' + $tableNames +
            ' wrapToBSTR@emit=' + $wrapN +
            ' wrapToBSTR@conv=' + $convWrap +
            ' kindSites=' + $kindSites.Count +
            ' rtSites=' + $rtSites.Count +
            ' rawSites=' + $rawSites.Count +
            ' dispatchSites=' + $slotSites.Count +
            ' arityQuestions=' + $arityN +
            ' rawSlotReaders=' + $slotWants.Count +
            ' strPtrVarNames=' + $strPtrVarN +
            ' castShapes=' + $castN)

if ($viol.Count -gt 0) {
    foreach ($v in $viol) { Write-Host ('  ' + $v) }
    Write-Host ('FAIL implicit_cstr_station: ' + $viol.Count +
                ' 条违规 —— 这条规则的站点在 cgen_expr_call_arg_emit.inc (Fix 175): ' +
                '位号问形参表 (缺表时问 builtinNoParamTableFormalCType 那一枚档表), ' +
                '档位问 typeTakesImplicitCStr, ' +
                '形状走 wrapToBSTR; 要加第二位主人请先说明这四个问为什么分不开')
    exit 1
}
Write-Host 'PASS implicit_cstr_station (R1..R12)'
exit 0
