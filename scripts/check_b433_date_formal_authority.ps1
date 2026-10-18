# check_b433_date_formal_authority.ps1 - 「日期形参」这一问不许拿 C 签名反推 (账 #433 §B207 / 第一百零五刀)
#
# 为什么单开这一道: 「这一位实参要不要按**日期**解析」这个问句, C 侧看不见答案 —— 日期形参与数值
#   形参**都是 double**。而引擎对文本实参的答案**按 builtin 分两种**:
#     FormatDateTime("46027", 0) = 2026/1/5   (纯数字文本按序列号那天)
#     Sqr("4")                  = 2          (数值折算)
#   拿 `rtParamType == "double"` / `paramBase == Double` 当判据, 会把 Sqr / Abs / Pow / CLng 这一族
#   一起误伤(§B174 教训① 的同一形状: 那张「哪几位是字符串」的表按 C 签名反推就答错过)。所以这一问
#   自带名单 —— 与 C 签名各走各的, 谁也不许替谁答。
#
# 规则（改坏了会红，不是装饰）:
#   R1  builtinNoParamTableFormalIsDate 必须存在，且**自带**一张表（不许退化成"读 C 签名"）
#   R2  名单里每一枚都必须在 cgen_util_type.cpp 的 kNoTableFormalKinds176 里也登了同一个名字
#       —— 因为发码那一臂要先 rtParamType 答得出 "double" 才会走到本问（否则整条臂跳过）
#   R3  名单里的位次必须 <= 同一行在 kNoTableFormalKinds176 登记的**长度**（不许指到不存在的位）
#   R4  **反名单**：kNoTableFormalKinds176 里所有登记了 double 形参、却**不在**日期名单里的
#       名字，必须逐个写明"为什么不按日期解析"（Sqr/Abs/CLng/CDbl/Cur/Format$ 那一族）
#   R5  发码那一臂必须真的按日期走 vb6_CDate，且**装箱**（vb6_CDate 的形参是 vb6_VARIANT，
#       裸发 BSTR 是反向 C2440 —— 本刀先踩过这一脚）
#   R6  数值那一族的名字**不许**出现在日期名单里（防止 R4 被自己抄破）
#
# 用法:  powershell -File scripts\check_b433_date_formal_authority.ps1
# 退出码: 0 = 全绿; 1 = 红

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$typeCpp = Join-Path $root 'src\backend\cgen_util_type.cpp'
$argInc = Join-Path $root 'src\backend\detail\expr\cgen_expr_call_arg_variant.inc'
foreach ($p in @($typeCpp, $argInc)) {
    if (-not (Test-Path -LiteralPath $p)) { Write-Host ("FAIL missing " + $p) -ForegroundColor Red; exit 1 }
}
$typeTxt = [IO.File]::ReadAllText($typeCpp)
$argTxt  = [IO.File]::ReadAllText($argInc)
$bad = @()

# ---- R2/R3/R6: 把两张表的对应关系读出来 ----
# 日期名单 = builtinNoParamTableFormalIsDate 里那张 kDateFormalPos433
$dm = [regex]::Match($typeTxt, '(?s)kDateFormalPos433\s*=\s*\{(?<body>.*?)\n\s*\};')
if (-not $dm.Success) {
    $bad += 'R1 builtinNoParamTableFormalIsDate has no kDateFormalPos433 table -> the question lost its own authority'
    Write-Host 'FAIL b433_date_formal_authority: builtinNoParamTableFormalIsDate has no table' -ForegroundColor Red
    exit 1
}
$datePos = @{}
foreach ($m in [regex]::Matches($dm.Groups['body'].Value, '\{"(?<n>vb6_[A-Za-z0-9_]+)",\s*\{(?<p>[0-9,\s]*)\}\}')) {
    $ps = @([regex]::Matches($m.Groups['p'].Value, '[0-9]+') | ForEach-Object { [int]$_.Value })
    $datePos[$m.Groups['n'].Value] = $ps
}
if ($datePos.Count -lt 8) {
    $bad += ('R1 date list holds only ' + $datePos.Count + ' names (floor 8) -> the scan or the table shrank')
}

# C 档表 = kNoTableFormalKinds176
$cm = [regex]::Match($typeTxt, '(?s)kNoTableFormalKinds176\s*=\s*\{(?<body>.*?)\n\s*\};')
if (-not $cm.Success) { $bad += 'R2 kNoTableFormalKinds176 table not found'; $cm = [pscustomobject]@{ Groups = @{ 'body' = @{ Value = '' } } } }
$cTable = @{}
foreach ($m in [regex]::Matches($cm.Groups['body'].Value, '\{"(?<n>vb6_[A-Za-z0-9_]+)",\s*\{(?<p>[^}]*)\}\}')) {
    $cTable[$m.Groups['n'].Value] = @([regex]::Matches($m.Groups['p'].Value, '"[^"]*"')).Count
}

foreach ($n in @($datePos.Keys | Sort-Object)) {
    if (-not $cTable.ContainsKey($n)) {
        $bad += ('R2 ' + $n + ' is in the date list but NOT in kNoTableFormalKinds176 -> rtParamType stays empty, ' +
                 'so the emit arm never reaches the date question (this is why it silently did nothing)')
        continue
    }
    foreach ($p in $datePos[$n]) {
        if ($p -ge $cTable[$n]) {
            $bad += ('R3 ' + $n + ' date slot ' + $p + ' is beyond its registered arity ' + $cTable[$n] +
                     ' -> the slot does not exist in the C-type table')
        }
    }
}

# ---- R4: double 形参却不在日期名单的，必须逐个有理由 ----
$NOT_DATE = @{
    'vb6_Abs' = '绝对值: 数值形参, 文本实参走数值折算(引擎 Abs("-4")=4)'
    'vb6_Sgn' = '符号: 同上, 数值形参'
    'vb6_Sqr' = '开方: 引擎 Sqr("4")=2 —— 本族反例, 按日期解析会答 1899 年'
    'vb6_CLng' = 'CLng 形参是数值, 文本按 Val 折算'
    'vb6_CDbl' = 'CDbl 形参是数值, 文本按 Val 折算'
    'vb6_CCur' = 'CCur 形参是数值(货币), 文本按 Val 折算'
}
foreach ($m in [regex]::Matches($cm.Groups['body'].Value, '\{"(?<n>vb6_[A-Za-z0-9_]+)",\s*\{(?<p>[^}]*)\}\}')) {
    $n = $m.Groups['n'].Value
    $p = $m.Groups['p'].Value
    if ($p -notmatch '"double"') { continue }          # 只查有 double 形参的行
    if ($datePos.ContainsKey($n)) { continue }        # 日期名单里的不在本检查范围
    if (-not $NOT_DATE.ContainsKey($n)) {
        $bad += ('R4 ' + $n + ' registers a double formal but is in NEITHER the date list NOR a written reason ' +
                 '-> either answer it as a date or write down why it is not one (silent hole)')
    }
}
foreach ($n in @($NOT_DATE.Keys)) {
    if (([string]($NOT_DATE[$n])).Length -lt 12) {
        $bad += ('R4 not-date entry ' + $n + ' has a reason shorter than 12 chars -> an empty reason is a silent hole')
    }
}

# ---- R6: 数值那一族不许被抄进日期名单 ----
foreach ($n in @($NOT_DATE.Keys)) {
    if ($datePos.ContainsKey($n)) {
        $bad += ('R6 ' + $n + ' is a numeric-formal builtin but appears in the DATE list -> numeric text ' +
                 'would be parsed as a date (engine Sqr("4")=2, not a 1900 date)')
    }
}

# ---- R5: 发码那一臂必须真的走 vb6_CDate 且装箱 ----
if ($argTxt -notmatch 'vb6_CDate\(vb6_VariantFromValue\(') {
    $bad += 'R5 the emit arm does not box the text before vb6_CDate -> vb6_CDate takes vb6_VARIANT, ' +
             'so a bare BSTR is the reverse C2440 (BSTR -> vb6_VARIANT); this knife hit that first'
}
if ($argTxt -notmatch 'builtinNoParamTableFormalIsDate') {
    $bad += 'R5 the emit arm never calls builtinNoParamTableFormalIsDate -> the date question has two answers again'
}
if ($argTxt -match 'argVal\s*=\s*"vb6_DateValue\(') {
    $bad += 'R5 the emit arm uses vb6_DateValue -> engine DateValue("46027") reports 13 (strict), ' +
             'but FormatDateTime("46027",0) answers 2026/1/5 (permissive); the strict outlet kills a legal arg'
}
# 判别式不许退化成 C 签名
if ($argTxt -match 'rtParamType\s*==\s*"double"[^\n]*\n[^\n]*builtinNoParamTableFormalIsDate') {
    $bad += 'R5 the emit arm keys the date conversion off rtParamType == "double" -> that is exactly the ' +
             'C-signature inference this knife exists to forbid (it would capture Sqr/Abs/CLng)'
}

if ($bad.Count -eq 0) {
    Write-Host ('PASS b433_date_formal_authority: date list ' + $datePos.Count + ' names / ' +
                (($datePos.Values | ForEach-Object { $_.Count }) | Measure-Object -Sum).Sum + ' slots | ' +
                'not-date ' + $NOT_DATE.Count + ' | C-table rows ' + $cTable.Count + ' | violations 0') -ForegroundColor Green
    exit 0
}
foreach ($b in $bad) { Write-Host ('FAIL ' + $b) -ForegroundColor Red }
exit 1