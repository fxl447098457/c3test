# check_dochost_authority.ps1 - 账 #217 第二刀的结构性哨兵 (只扫编译器源码, 不起 cl)
#
# 为什么单开这一道: VB6 文档的隐式对象 (UserControl / PropertyPage / Extender / Ambient, 外加
# 全局库前缀 VBA) 只有"这份文档是哪一类"这一个前提能决定它存不存在。以前这个前提在仓里
# 猜过两处 —— driver 按扩展名算 isControlModule/isPropertyPageModule, 发码那边再从
# controlTypeName 里找 "PropertyPage" 字符串 (cgen_form.cpp 改动前) —— 而语义层完全不知道
# 有这回事, 于是每条 `UserControl.hDC` / `VBA.Len(x)` 都被当成未声明标识符:
# 开着 Option Explicit 每形一条 VB3001 (两份真工程 946 条), 关着每枚用到的过程发一枚
# 没人引用的 vb6_VARIANT 隐式局部 (实测 `VBA` 那枚会真发进发码)。
#
# 口径现在收成一格: Module::docKind (common/types.hpp 的 DocumentKind), 由 driver 一处写、
# 语义层与发码层两处读。
#
# 规则 (改坏了会红, 不是装饰):
#   D1  DocumentKind 定义 1 份, 四个档位齐
#   D2  docKind 的**写入点全仓 1 处** (driver_frontend) —— 多一处就是第二个权威
#   D3  isDocumentHostObject 定义 1 / 声明 1 / 调用 1, 三处都带着那个位置形参 —— 账 #278
#       第八刀把「哪个位合法」收进了唯一出口 (qualifierPos), 调用点只负责交出手里的
#       memberObjCtx_; 老形状 (调用点自己在外面 && 一次) 现在反过来钉它为 0
#   D3b 那个出口必须真的按位置分档 (体内查 qualifierPos >= 3) —— 裸位的真缺项还得响,
#       这条守的就是老 D3 守的那件事, 只是搬到判据自己那一头
#   D4  旧的字符串猜测不许回来: controlTypeName 里找 "PropertyPage" 必须为 0
#   D5  那张名字表要五枚齐全 (usercontrol / propertypage / extender / ambient / vba)
#
# 用法:  pwsh -File scripts\check_dochost_authority.ps1
# 退出码: 0 = 全绿; 1 = 红

param(
    [string]$Root = ""      # 负控用: 把整棵树指到副本上跑 (哨兵只读源码, 不起 cl)
)
$ErrorActionPreference = "Stop"
$root = if ($Root) { $Root } else { Split-Path -Parent $PSScriptRoot }
$bad = @()

function ReadSrc([string]$rel) {
    $p = Join-Path $root $rel
    if (-not (Test-Path -LiteralPath $p)) { return $null }
    return [System.IO.File]::ReadAllText($p)
}

$typesRel = 'src\common\types.hpp'
$astRel = 'src\ast\detail\ast_decl.hpp'
$frontRel = 'src\driver\driver_frontend.cpp'
$hdrRel = 'src\semantics\semantic_analyzer.hpp'
$semUtilRel = 'src\semantics\semantic_analyzer_util.cpp'
$semExprRel = 'src\semantics\semantic_analyzer_expr.cpp'
$formRel = 'src\backend\module\cgen_form.cpp'

$files = [ordered]@{ $typesRel = $null; $astRel = $null; $frontRel = $null; $hdrRel = $null;
                     $semUtilRel = $null; $semExprRel = $null; $formRel = $null }
foreach ($k in @($files.Keys)) {
    $files[$k] = ReadSrc $k
    if ($null -eq $files[$k]) { $bad += ('D0 missing file: ' + $k) }
}
if ($bad.Count -gt 0) {
    foreach ($b in $bad) { Write-Host ('FAIL ' + $b) -ForegroundColor Red }
    exit 1
}

# D1
$def = @([regex]::Matches($files[$typesRel], 'enum class DocumentKind')).Count
if ($def -ne 1) { $bad += ('D1 DocumentKind defined ' + $def + ' times (want 1)') }
foreach ($n in @('Standard', 'Form', 'UserControl', 'PropertyPage')) {
    $one = @([regex]::Matches($files[$typesRel], ('(?m)^\s*' + $n + ' = \d'))).Count
    if ($one -ne 1) { $bad += ('D1 DocumentKind missing/ambiguous member ' + $n + ' (' + $one + ')') }
}
$field = @([regex]::Matches($files[$astRel], 'DocumentKind docKind')).Count
if ($field -ne 1) { $bad += ('D1 Module::docKind declared ' + $field + ' times (want 1)') }

# D2: 全仓只许一处写 docKind
$allSrc = Get-ChildItem -Path (Join-Path $root 'src') -Recurse -Include *.cpp, *.hpp, *.inc |
          ForEach-Object { [System.IO.File]::ReadAllText($_.FullName) }
$writes = 0
foreach ($t in $allSrc) { $writes += @([regex]::Matches($t, '->docKind\s*=')).Count }
if ($writes -ne 1) { $bad += ('D2 docKind writers ' + $writes + ' times (want exactly 1 - the single authority)') }

# D3
$d3def = @([regex]::Matches($files[$semUtilRel], 'bool SemanticAnalyzer::isDocumentHostObject\(const std::string& name, bool qualifierPos\) const')).Count
$d3dec = @([regex]::Matches($files[$hdrRel], 'bool isDocumentHostObject\(const std::string& name, bool qualifierPos\) const')).Count
$d3use = @([regex]::Matches($files[$semExprRel], 'isDocumentHostObject\(node\.name, memberObjCtx_\)')).Count
if ($d3def -ne 1) { $bad += ('D3 isDocumentHostObject defined ' + $d3def + ' times taking the position (want 1)') }
if ($d3dec -ne 1) { $bad += ('D3 isDocumentHostObject declared ' + $d3dec + ' times taking the position (want 1)') }
if ($d3use -ne 1) { $bad += ('D3 call sites handing memberObjCtx_ to the authority ' + $d3use + ' times (want 1)') }
# D3c: 调用点再自己 && 一次 = 同一个决定两份答案 (第八刀之前那正是唯一形状, 现在禁掉)
$d3outer = @([regex]::Matches($files[$semExprRel], 'memberObjCtx_\s*(&&|\|\|)\s*isDocumentHostObject')).Count
if ($d3outer -ne 0) { $bad += ('D3 the caller re-gates the exemption ' + $d3outer + ' times (want 0 - two answers for one decision)') }
# D3b: 形参不是摆设 —— 体内拒裸位的那几档必须各查一次 qualifierPos
$b3 = $files[$semUtilRel].IndexOf('SemanticAnalyzer::isDocumentHostObject')
$d3pos = -1
if ($b3 -lt 0) {
    $bad += 'D3b predicate body not found'
} else {
    $n3 = $files[$semUtilRel].IndexOf('SemanticAnalyzer::', $b3 + 10)
    if ($n3 -lt 0) { $t3 = $files[$semUtilRel].Substring($b3) } else { $t3 = $files[$semUtilRel].Substring($b3, $n3 - $b3) }
    $d3pos = @([regex]::Matches($t3, 'return\s+qualifierPos|&&\s*qualifierPos')).Count
    if ($d3pos -lt 3) { $bad += ('D3b position consulted ' + $d3pos + ' times in the predicate (want >= 3) - bare uses must still warn') }
}

# D4
$sniff = 0
foreach ($t in $allSrc) { $sniff += @([regex]::Matches($t, 'controlTypeName[^;]*find\("PropertyPage"\)')).Count }
if ($sniff -ne 0) { $bad += ('D4 old string-sniffing of the designer kind back ' + $sniff + ' times (want 0)') }
$kinder = @([regex]::Matches($files[$formRel], 'DocumentKind::PropertyPage')).Count
if ($kinder -lt 1) { $bad += ('D4 cgen_form.cpp reads the authority ' + $kinder + ' times (want >= 1)') }

# D5
$bodyStart = $files[$semUtilRel].IndexOf('SemanticAnalyzer::isDocumentHostObject')
if ($bodyStart -lt 0) {
    $bad += 'D5 predicate body not found'
} else {
    $nxt = $files[$semUtilRel].IndexOf('SemanticAnalyzer::', $bodyStart + 10)
    if ($nxt -lt 0) { $body = $files[$semUtilRel].Substring($bodyStart) } else { $body = $files[$semUtilRel].Substring($bodyStart, $nxt - $bodyStart) }
    $missing = @()
    foreach ($n in @('usercontrol', 'propertypage', 'extender', 'ambient', 'vba')) {
        if ($body.IndexOf('"' + $n + '"', [System.StringComparison]::Ordinal) -lt 0) { $missing += $n }
    }
    if ($missing.Count -gt 0) { $bad += ('D5 name table missing: ' + ($missing -join ',')) }
}

if ($bad.Count -gt 0) {
    foreach ($b in $bad) { Write-Host ('FAIL ' + $b) -ForegroundColor Red }
    exit 1
}
Write-Host ('PASS doc-host authority: kind def 1 (4 档), writers ' + $writes +
    ', predicate ' + $d3def + '+' + $d3dec + '+' + $d3use + ' pos ' + $d3pos + ' outer ' + $d3outer +
    ', old sniffs ' + $sniff)
exit 0
