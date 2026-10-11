# check_builtin_slot_returns.ps1 —— 「内置出口交出来的 C 是不是整只槽」这张表与 RTL 对账（账 #305 §B146，第三十九刀）
#
# 为什么要有这一道：src/backend/cgen_util_type.cpp 的 variantPrefixes 是一枚**手写清单**，
# 而它回答的那个问句（「vb6_X(...) 交回来的是 vb6_VARIANT 槽还是裸标量」）的答案**住在 RTL 的
# 声明里**。两份从不汇合 ⇒ 两个方向都会走散，且都贵：
#   少一名 = 消费点按裸标量发码。实物（第三十九刀量到，BASE 真编译 6 条错）：
#          `CDec(1234567) + 1` 发成 (int32_t)(vb6_CDec(...) + 1) => C2088「+ 对于 struct 非法」，
#          `d = CDec("2.5")` => C2440。整条夹具编不过。
#   多一名 = 死名躺在表里骗人。本刀清掉 11 条（RTL 没有这枚符号、400 份语料发码里一次没出现）。
#   错位一格 = 指针返回当槽。`vb6_VARIANT *vb6_VariantArrayElemPtr(...)` 交回的是**槽的地址**，
#          按名字登记它就是把「地址」答成「槽」（本刀的 census 第二版栽过一次，故 R3 单独钉）。
#
# 判据面（任何一条不齐就红）：
#   R1 表里每一枚都必须真是「RTL 声明返回 vb6_VARIANT」的那一族 —— 例外只有两张白名单，
#      每条都要写理由：RTL 的 _Generic 宏（只有每一档变目都交槽才许登记），以及
#      **不是 RTL 出口**的名字（语料工程自己的过程在单模块模式下拼成裸 vb6_<proc>）。
#   R2 RTL 里「声明返回 vb6_VARIANT 且后端发码字符串里出现过」的每一枚都必须在表里。
#   R3 指针返回（vb6_VARIANT *f(...)）一律不许进表。
#   R4 census must not read empty: table >=25, rtlSlot >=25, backend literal names >=50,
#      少一个就报「扫描没匹配到东西」（红比 vacuous PASS 好）。
# 用法: powershell -File scripts\check_builtin_slot_returns.ps1   (PASS = exit 0)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$typeCpp = Join-Path $Root 'src\backend\cgen_util_type.cpp'
if (-not (Test-Path -LiteralPath $typeCpp)) { Write-Host ('FAIL: 读不到 ' + $typeCpp); exit 1 }

function Strip-CComments([string]$t) {
    $t = [regex]::Replace($t, '/\*[\s\S]*?\*/', ' ')
    $t = [regex]::Replace($t, '(?m)//.*$', ' ')
    return $t
}

# ---- 1) 那张手写的表 ----
$srcRaw = [IO.File]::ReadAllText($typeCpp)
$m = [regex]::Match($srcRaw, '(?s)static const std::vector<std::string> variantPrefixes = \{(?<body>.*?)\n    \};')
if (-not $m.Success) { Write-Host 'FAIL: 在 cgen_util_type.cpp 里找不到 variantPrefixes 的初始化块（表被改名/挪走了？）'; exit 1 }
$tbl = @{}
foreach ($q in [regex]::Matches($m.Groups['body'].Value, '"(vb6_[A-Za-z0-9_]+)\("')) { $tbl[$q.Groups[1].Value] = $true }
$tableNames = @($tbl.Keys | Sort-Object)
if ($tableNames.Count -lt 25) { Write-Host ('FAIL R4: 表只读出 ' + $tableNames.Count + ' 条 (<25) —— 扫描读空, 不是一条绿着的判据'); exit 1 }

# ---- 2) RTL: 谁声明返回 vb6_VARIANT ----
$rtlSlot = @{}; $rtlPtr = @{}; $rtlMacro = @{}
$rtlFiles = @(Get-ChildItem -Path (Join-Path $Root 'src\rtl') -Recurse -File -Include *.c,*.h)
foreach ($f in $rtlFiles) {
    $t = Strip-CComments ([IO.File]::ReadAllText($f.FullName))
    foreach ($k in [regex]::Matches($t, '(?m)^\s*#\s*define\s+(vb6_[A-Za-z0-9_]+)')) { $rtlMacro[$k.Groups[1].Value] = $true }
    foreach ($k in [regex]::Matches($t, 'vb6_VARIANT\s*\*\s*(vb6_[A-Za-z0-9_]+)\s*\(')) { $rtlPtr[$k.Groups[1].Value] = $true }
    foreach ($k in [regex]::Matches($t, 'vb6_VARIANT\s+(vb6_[A-Za-z0-9_]+)\s*\(')) {
        $n = $k.Groups[1].Value
        if (-not $rtlPtr.ContainsKey($n)) { $rtlSlot[$n] = $true }
    }
}
if ($rtlSlot.Keys.Count -lt 25) { Write-Host ('FAIL R4: RTL 槽声明只数出 ' + $rtlSlot.Keys.Count + ' 枚 (<25) —— 扫描读空'); exit 1 }

# ---- 3) 后端: 哪些名字真出现在发码字面量里 ----
$emitted = @{}
$beFiles = @(Get-ChildItem -Path (Join-Path $Root 'src\backend') -Recurse -File -Include *.cpp,*.inc)
foreach ($f in $beFiles) {
    $t = Strip-CComments ([IO.File]::ReadAllText($f.FullName))
    foreach ($lit in [regex]::Matches($t, '"(?:[^"\\]|\\.)*"')) {
        foreach ($k in [regex]::Matches($lit.Value, '(vb6_[A-Za-z0-9_]+)')) { $emitted[$k.Groups[1].Value] = $true }
    }
}
if ($emitted.Keys.Count -lt 50) { Write-Host ('FAIL R4: backend literals gave only ' + $emitted.Keys.Count + ' names (min 50) -- scan read nothing'); exit 1 }

# ---- 白名单（每一格都要有理由，不是「先让它绿」） ----
$allowMacro = @{ 'vb6_VariantFromValue' = '_Generic 宏: 每一档变目都交槽 (含 vb6_VARIANT: vb6_VariantIdentity 档)' }
$allowNonRtl = @{ 'vb6_DispCallByVtbl' = '不是 RTL 出口 —— 语料 mdQRCodegen.bas 自己的 Private Function, 单模块模式下 C 名是裸 vb6_<proc>, variantReturnFuncs_ 够不着 (撤掉它会丢五处 vb6_VariantToLong 提取, 见 §B146)' }

$viol = @()

# R1: 表 -> RTL
foreach ($n in $tableNames) {
    if ($rtlSlot.ContainsKey($n)) { continue }
    if ($allowMacro.ContainsKey($n) -and $rtlMacro.ContainsKey($n)) { continue }
    if ($allowNonRtl.ContainsKey($n)) { continue }
    $why = 'RTL 里没有这枚符号返回 vb6_VARIANT'
    if ($rtlPtr.ContainsKey($n)) { $why = 'RTL 里它是**指针返回** (交回槽的地址, 不是槽) —— 见 R3' }
    elseif ($rtlMacro.ContainsKey($n)) { $why = 'RTL 里它是宏, 且不在 allowMacro 的逐档核过名单里 —— 宏按名字登记会一律答错' }
    $viol += ('R1 死名/错档: ' + $n + '(' + ' —— ' + $why)
}

# R2: (RTL 槽 ∩ 后端 emit) -> 表
foreach ($n in @($rtlSlot.Keys | Sort-Object)) {
    if (-not $emitted.ContainsKey($n)) { continue }
    if ($tbl.ContainsKey($n)) { continue }
    $viol += ('R2 漏登记: ' + $n + '( —— RTL 声明返回 vb6_VARIANT, 后端发码字面量里出现过, 但表里没有 (消费点会按裸标量发码)')
}

# R3: 指针返回一律不许在表里
foreach ($n in @($rtlPtr.Keys | Sort-Object)) {
    if ($tbl.ContainsKey($n)) { $viol += ('R3 指针返回被登记成槽: ' + $n + '(') }
}

Write-Host ('census: table=' + $tableNames.Count + ' rtlSlot=' + $rtlSlot.Keys.Count +
            ' rtlPtr=' + $rtlPtr.Keys.Count + ' rtlMacro=' + $rtlMacro.Keys.Count +
            ' emittedNames=' + $emitted.Keys.Count +
            ' (RTL 槽 ∩ emit) = ' + (@($rtlSlot.Keys | Where-Object { $emitted.ContainsKey($_) }).Count))

if ($viol.Count -gt 0) {
    foreach ($v in $viol) { Write-Host ('  ' + $v) }
    Write-Host ('FAIL builtin_slot_returns: ' + $viol.Count + ' 条违规 —— 修法是把表与 RTL 对账 (补名/撤名), 不是往白名单里加名字; 真要走例外请连带写清理由并重跑 §B146 那把 400 份 A/B')
    exit 1
}
Write-Host 'PASS builtin_slot_returns (R1..R4)'
exit 0
