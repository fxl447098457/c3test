# check_select_carrier_authority.ps1 - 账 #305 第311刀的结构性哨兵（只扫编译器源码 + runner，不起 cl）
#
# 起因：Select Case 的主语静态答 Variant 时，承载档从前**只问 inferExprType 一档** ——
# 答 Variant 就落 int32_t 档 + vb6_VariantToLong 解包，而 Case 标签是串档时那一路发成
# int32_t 与 vb6_BSTR_FromStr 返回的指针相比（C 模式只警告），于是每一档都假、恒落 Case Else。
# 靶子 = ComCtlsDemo 工具栏 Select Case Button.Tag（TbrButton.cls 的 Property Get Tag() As Variant）：
# 点 "Show ListBoxW1" 该让 SysListView32 隐身、ListBox 上位，改前只弹一句 "clicked button 5"。
#
# 现在的口径 = 「主语是槽」这一问保留原样（两档：文本问句 + 按名表），但**承载档**多一条由
# Case 标签回答的岔口，而这一问只许有一位主人 selectCaseLabelsAllString。
#
# 规则（改坏了会红，不是装饰）：
#   R1  主人三位一体：定义恰 1 处、伞内声明恰 1 处（**行首**的声明，被注释掉就不算）、
#       调用恰 1 处；全 src 裸名合计恰 3 处（长出第四处 = 有人另抄了一份标签问句）
#   R2  翻档那一发的形状：isStringSelect 被改写的语句恰 1 处，且它所属的 if 同时问
#       Vb6Type::Variant 与主人 —— 两条问句缺一条就不许翻档
#   R3  承载档的三条解包出口各自恰 1 处（VariantToLong / VariantToString / VariantToDouble）
#       —— 为串档另开一条解包路 = 第二本账。数的是**发码那一形**（引号起头的
#       `"vb6_VariantToXxx("`），不是名字在文件里出现几次 —— 三条解包各有一条注释也提它，
#       按裸名数会得到 2（首轮实测），而那红是数法错、不是病灶。
#   R4  主人自己两形都问：标签本体与 To 范围的终止端各 >=1；体内不许出现 isIsClause
#       （Case Is 的右操作数就是标签，共用同一问；分档 = 两份答案）；对 clause->values
#       的内圈循环恰 1 条，且体内不许有 break/continue —— 这位主人的答案是**全部**标签，
#       提前退出 = 只扫了一部分就敢说"全是串"（N11 负控实测漏过第一轮）
#   R5  判据面：新针 CI-vartag 与混合标签那枚现状针 CI-mixtag 在 runner 各恰 1 处，
#       吃 $ciNeedles 的 Add-BasTest 恰 2 台（x64 + x86；只登记一台 = 覆盖面少一半），
#       而本哨兵自己的 Test-SelectCarrierAuthority 在 runner 恰 1 个调用点
#       （只有定义没调用 = 这一格门永远不跑）
#
# 用法: powershell -File scripts\check_select_carrier_authority.ps1   (PASS = exit 0)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$selectRel = 'src\backend\stmt\cgen_select.cpp'
$helpersRel = 'src\backend\detail\util\cgen_helpers.inc'
$runnerRel = 'tests\run_tests.ps1'

function ReadSrc([string]$rel) {
    $p = Join-Path $root $rel
    if (-not (Test-Path -LiteralPath $p)) { return $null }
    return [System.IO.File]::ReadAllText($p)
}

$bad = @()
$select = ReadSrc $selectRel
$helpers = ReadSrc $helpersRel
$runner = ReadSrc $runnerRel
foreach ($p in @($selectRel, $helpersRel, $runnerRel)) {
    if ($null -eq (ReadSrc $p)) { $bad += ('R0 missing file: ' + $p) }
}
if ($bad.Count -gt 0) {
    foreach ($b in $bad) { Write-Host ('FAIL ' + $b) -ForegroundColor Red }
    exit 1
}

# ---- R1: 主人的三张脸，各一处 ----
$defN = @([regex]::Matches($select, 'bool\s+CCodeGen::selectCaseLabelsAllString')).Count
$declN = @([regex]::Matches($helpers, '(?m)^[ \t]*bool\s+selectCaseLabelsAllString\s*\(')).Count
$callN = @([regex]::Matches($select, 'selectCaseLabelsAllString\(node\)')).Count
if ($defN -ne 1) { $bad += ('R1 definition of selectCaseLabelsAllString = ' + $defN + ' (want exactly 1)') }
if ($declN -ne 1) { $bad += ('R1 declaration in cgen_helpers.inc = ' + $declN + ' (want exactly 1)') }
if ($callN -ne 1) { $bad += ('R1 call site selectCaseLabelsAllString(node) = ' + $callN + ' (want exactly 1)') }

# 全 src 树里那个裸名的合计（定义 + 声明 + 调用 = 3；第四处出现在任何文件都是另抄一份）
$srcFiles = Get-ChildItem -Path (Join-Path $root 'src') -Recurse -File -Include *.cpp,*.hpp,*.inc
$allN = 0
foreach ($f in $srcFiles) {
    $t = [System.IO.File]::ReadAllText($f.FullName)
    $allN += @([regex]::Matches($t, 'selectCaseLabelsAllString')).Count
}
if ($allN -ne 3) { $bad += ('R1 bare-name census across src = ' + $allN + ' (want exactly 3: def + decl + call)') }

# ---- R2: 翻档那一发的形状 ----
$flipN = @([regex]::Matches($select, 'isStringSelect\s*=\s*true')).Count
if ($flipN -ne 1) { $bad += ('R2 the carrier flip isStringSelect = true = ' + $flipN + ' (want exactly 1)') }
$guard = [regex]::Match($select, '(?s)if \(!isStringSelect && !isFloatSelect &&\s*testType == Vb6Type::Variant\s*\r?\n\s*&& selectCaseLabelsAllString\(node\)\) \{\s*\r?\n\s*isStringSelect = true;')
if (-not $guard.Success) {
    $bad += 'R2 flip is not inside the single if that asks BOTH testType == Vb6Type::Variant AND the label authority (want that exact guard)'
}

# ---- R3: 三条解包出口各一处 ----
foreach ($fx in @('vb6_VariantToLong', 'vb6_VariantToString', 'vb6_VariantToDouble')) {
    $n = @([regex]::Matches($select, '"' + [regex]::Escape($fx) + '\("')).Count
    if ($n -ne 1) { $bad += ('R3 unwrap EXIT ' + $fx + ' (quoted emit form) in cgen_select.cpp = ' + $n + ' (want exactly 1)') }
}

# ---- R4: 主人自己两形都问，且不另开 Case Is 分档 ----
$bodyM = [regex]::Match($select, '(?s)bool CCodeGen::selectCaseLabelsAllString.*?\r?\n\}\r?\n')
if (-not $bodyM.Success) {
    $bad += 'R4 body of selectCaseLabelsAllString not found (the authority must stay one function)'
} else {
    $body = $bodyM.Value
    $vN = @([regex]::Matches($body, 'cv\.value')).Count
    $tN = @([regex]::Matches($body, 'cv\.toValue')).Count
    $iN = @([regex]::Matches($body, 'isIsClause')).Count
    $loopN = @([regex]::Matches($body, 'for\s*\(\s*auto&\s+cv\s*:\s*clause->values\s*\)')).Count
    $exitN = @([regex]::Matches($body, '\b(break|continue)\b')).Count
    if ($vN -lt 1) { $bad += 'R4 the authority never asks cv.value (label body)' }
    if ($tN -lt 1) { $bad += 'R4 the authority never asks cv.toValue (the Case a To b end)' }
    if ($iN -ne 0) { $bad += ('R4 the authority splits Case Is into its own arm (' + $iN + ' hits) - Is operand IS the label') }
    if ($loopN -ne 1) { $bad += ('R4 inner label loop over clause->values = ' + $loopN + ' (want exactly 1 - two loops = one of them is a partial scan)') }
    if ($exitN -ne 0) { $bad += ('R4 the authority has an early exit (break/continue = ' + $exitN + ') - it must inspect EVERY label, answer only false or true') }
}

# ---- R5: 判据面 ----
$tagN = @([regex]::Matches($runner, 'CI-vartag=bbb/bbb/aaa/else')).Count
$mixN = @([regex]::Matches($runner, 'CI-mixtag=else/num')).Count
if ($tagN -ne 1) { $bad += ('R5 runner needle CI-vartag = ' + $tagN + ' (want exactly 1, inside the shared $ciNeedles)') }
if ($mixN -ne 1) { $bad += ('R5 runner needle CI-mixtag = ' + $mixN + ' (want exactly 1: the mixed-label form is deliberately NOT flipped)') }
$basN = @([regex]::Matches($runner, 'Add-BasTest[^\r\n]*\$ciNeedles')).Count
if ($basN -ne 2) { $bad += ('R5 Add-BasTest lines using $ciNeedles = ' + $basN + ' (want exactly 2: x64 + x86)') }
# 哨兵自己也要有调用点：只有 function 定义 = 门永远不跑这一格（check_static_sentinel_registration
# 的 R1b 会报，但那是**别的**哨兵的事；本刀欠的是「这一格在跑」这句话本身）。
$callSite = @([regex]::Matches($runner, '(?m)^\s+if \(Enter-StaticShard\) \{ Test-SelectCarrierAuthority \}\s*$')).Count
if ($callSite -ne 1) { $bad += ('R5 harness call site for Test-SelectCarrierAuthority = ' + $callSite + ' (want exactly 1)') }

if ($bad.Count -gt 0) {
    $bad | ForEach-Object { Write-Host ('FAIL ' + $_) }
    exit 1
}
Write-Host ('PASS select_carrier_authority: def=' + $defN + ' decl=' + $declN + ' call=' + $callN +
            ' census=' + $allN + ' flip=' + $flipN + ' unwraps=3x1')
exit 0
