# ============================================================
# check_com_latebind_guard.ps1 — 账 #305 第二百六十二刀 (丙) 的结构性哨兵
#
# 为什么单开这一道：这一族的缺陷形状是「接收者不是真 IDispatch 时**一声不吭**」。
# 七个晚绑定出口 (ComCall / ComCallByDispid / ComGetProp / ComGetPropArg /
# ComSetProp / ComSetPropArg / ComSetRef) 以前各行其是: 有的打一行 "not found"
# 就走人, 有的 (CallByDispid / NULL 接收者) 连日志都没有。静默 = 下回再漏时没人
# 知道漏在哪一枚 ⇒ 判据只能读「stderr 里那几行标记的条数」。
#
# 这一刀**行为不变** (仍按原路返回 NULL / 空), 只把「静默」换成「响亮」:
#   往 stderr 打一行 [C3_LATEBIND_MISS #n] 并把计数器 +1。
# 刻意不在这儿抛错 —— 无处理器的 vb6_RaiseError 会 exit(424) 把进程带走。
#
# 规则：
#   R1  七个出口一个都不许漏护栏: 从 vb6com_invoke.c **自动枚举**那一族出口的定义
#       (vb6_ComCall / CallByDispid / GetProp / GetPropArg / SetProp / SetPropArg /
#       SetRef), 数量必须恰好 7; 每个体内恰好一处 vb6_ComLateBindMiss( 调用,
#       它必须紧跟在 the IsMiss if-line 那一行下面, 且排在体内
#       第一条 return 之前 (护栏要赶在早退之前, 否则 NULL 接收者永远静默)。
#       加第八个出口 / 撤掉一处调用 ⇒ 这条立刻红。
#   R2  伪接收者豁免三族 (字体 / Extender / 宿主对象) 都在 vb6_ComLateBindIsPseudo
#       体内各恰好一次; 且 vb6_ComLateBindIsMiss 必须**先问 pseudo 再问
#       vb6_ComIsDispatchable** —— 顺序反了会把合法宿主对象判成漏 (判据就永远红)。
#   R3  护栏不改行为: vb6_ComLateBindMiss 体内计数 +1 恰好一次、标记恰好一处,
#       且不许出现 vb6_RaiseError / exit( / TerminateProcess / abort / return
#       (出口是 void, 一旦带控制流就成了「行为变了」)。
#   R4  计数器出口只住一处: 定义在 vb6com_invoke.c 恰好 1 次, 声明在 vb6com.h 与
#       vb6rtl_class_com.h 各恰好 1 次; 计数变量 g_com_latebind_miss 定义恰好 1 次。
#   R5  宿主通道的兄弟报告 (同一刀里另半): vb6_Host_UnknownMemberCount 定义 1 次 /
#       声明 1 次, 且三个宿主模型 .inc (Call / GetProp / SetProp) 各恰好一处
#       [C3_HOST_MEMBER_MISS # 标记 —— 那一族的静默漏与本刀同源, 两半一起钉。
#   R6  判据在 runner 里落地: run_tests.ps1 的那张期望表 + 三个读数出口
#       (Invoke-TestExe 的 .NET 路 / Start-Process 路 / Test-GuiVbp 路) + 并行
#       runspace 里那份**内联**计数 (runspace 调不到脚本函数, 不内联就等于没判)。
#   R7  负控在册: tests\b446_latebind\B446.vbp 在, 且在 run_tests.ps1 里登记两行
#       (x64 + x86) —— 负控必须恰好响一次 (Nothing 接收者), 多一次少一次都红。
#   R8  这枚哨兵自己不许是死码: 在 run_tests.ps1 里出现恰好两次 (定义 + 静态分片
#       登记) —— 哨兵写了不登记, 门就永远不跑它, 那一族缺口又静默回去了。
#
# 用法: powershell -File scripts\check_com_latebind_guard.ps1 [-Root <目录>]   (PASS ⇒ exit 0)
# 负控在 src 的副本上跑(-Root)，共享工作树一格不动。

param([string]$Root = "")

$ErrorActionPreference = 'Stop'
if ($Root -eq "") { $Root = Split-Path -Parent $PSScriptRoot }

$invPath  = Join-Path $Root 'src\rtl\core\vb6com\vb6com_invoke.c'
$comH     = Join-Path $Root 'src\rtl\core\vb6com\vb6com.h'
$clsH     = Join-Path $Root 'src\rtl\core\vb6rtl\vb6rtl_class_com.h'
$hostC    = Join-Path $Root 'src\rtl\core\vb6forms\uc\uc_hostmodel.c'
$ctrlH    = Join-Path $Root 'src\rtl\core\vb6forms\vb6forms_controls.h'
$incCall  = Join-Path $Root 'src\rtl\core\vb6forms\uc\detail\uc_hostmodel_call.inc'
$incGet   = Join-Path $Root 'src\rtl\core\vb6forms\uc\detail\uc_hostmodel_getprop.inc'
$incSet   = Join-Path $Root 'src\rtl\core\vb6forms\uc\detail\uc_hostmodel_setprop.inc'
$rtPath   = Join-Path $Root 'tests\run_tests.ps1'
$fixture  = Join-Path $Root 'tests\b446_latebind\B446.vbp'

$fail = @()

function Strip-Comments262([string]$text) {
    $out = New-Object System.Text.StringBuilder
    $i = 0
    $n = $text.Length
    $inBlock = $false
    $inLine = $false
    $inStr = $false
    while ($i -lt $n) {
        $c = $text[$i]
        $d = if ($i + 1 -lt $n) { $text[$i + 1] } else { [char]0 }
        if ($inLine) { if ($c -eq "`n") { $inLine = $false; [void]$out.Append($c) }; $i++; continue }
        if ($inBlock) { if ($c -eq "*" -and $d -eq "/") { $inBlock = $false; $i += 2; continue }
                        if ($c -eq "`n") { [void]$out.Append($c) }; $i++; continue }
        if ($inStr) { [void]$out.Append($c); if ($c -eq "\") { if ($i + 1 -lt $n) { [void]$out.Append($text[$i+1]) }; $i += 2; continue }
                      if ($c -eq '"') { $inStr = $false }; $i++; continue }
        if ($c -eq "/" -and $d -eq "*") { $inBlock = $true; $i += 2; continue }
        if ($c -eq "/" -and $d -eq "/") { $inLine = $true; $i += 2; continue }
        if ($c -eq '"') { $inStr = $true }
        [void]$out.Append($c)
        $i++
    }
    return $out.ToString()
}

function Read-Lines262([string]$p) {
    $raw = [System.IO.File]::ReadAllText($p)
    return ,($raw -split "`r?`n")
}

# 函数体: 从定义行到下一个顶格 '}' (这仓的 C 文件都用顶格收尾)
function Get-Body262([string[]]$lines, [string]$rx) {
    $s = -1
    for ($i = 0; $i -lt $lines.Count; $i++) { if ($lines[$i] -match $rx) { $s = $i; break } }
    if ($s -lt 0) { return @() }
    for ($j = $s; $j -lt $lines.Count; $j++) {
        if ($j -gt $s -and $lines[$j] -match '^\}') { return $lines[$s..($j - 1)] }
    }
    return $lines[$s..($lines.Count - 1)]
}

function Count-In262([string[]]$body, [string]$needle) {
    $n = 0
    foreach ($l in $body) { if ($l.Contains($needle)) { $n++ } }
    return $n
}
function Index-Of262([string[]]$body, [string]$needle) {
    for ($i = 0; $i -lt $body.Count; $i++) { if ($body[$i].Contains($needle)) { return $i } }
    return -1
}

$missing = @($invPath, $comH, $clsH, $hostC, $ctrlH, $incCall, $incGet, $incSet, $rtPath) |
           Where-Object { -not (Test-Path $_) }
if ($missing) {
    foreach ($m in $missing) { $fail += ("missing file " + $m) }
    foreach ($f in $fail) { Write-Host ("FAIL " + $f) }
    exit 1
}

$invRaw  = (Read-Lines262 $invPath) -join "`n"
$invCode = Strip-Comments262 $invRaw
$invLines = $invCode -split "`r?`n"

# ---- R1: 七个出口一个都不许漏护栏 (出口名单从文件里自动枚举) ----
$exitRx = '^(?:void\*|void|HRESULT|int32_t)\s+vb6_Com(Call|CallByDispid|GetProp|GetPropArg|SetProp|SetPropArg|SetRef)\s*\('
$exits = @()
for ($i = 0; $i -lt $invLines.Count; $i++) {
    if ($invLines[$i] -match $exitRx) { $exits += ,@($Matches[1], $i) }
}
if ($exits.Count -ne 7) {
    $fail += ("R1 the late-bind exit family has " + $exits.Count + " definitions (want exactly 7: " +
              (($exits | ForEach-Object { 'vb6_Com' + $_[0] }) -join ', ') +
              ") - 加了出口就得同时加护栏, 撤了出口就得同步改这道尺")
}
foreach ($e in $exits) {
    $name = 'vb6_Com' + $e[0]
    $body = Get-Body262 $invLines ('^(?:void\*|void|HRESULT|int32_t)\s+' + [regex]::Escape($name) + '\s*\(')
    if ($body.Count -lt 5) { $fail += ("R1 cannot locate the body of " + $name); continue }
    $nGuard = Count-In262 $body 'vb6_ComLateBindMiss('
    if ($nGuard -ne 1) {
        $fail += ("R1 guard calls inside " + $name + " = " + $nGuard +
                  " (want exactly 1) - 接收者不是真 IDispatch 时这一族必须响亮")
        continue
    }
    $iGuard = Index-Of262 $body 'vb6_ComLateBindMiss('
    $iIf    = Index-Of262 $body 'if (vb6_ComLateBindIsMiss(disp)) {'
    if ($iIf -lt 0 -or $iIf -ne ($iGuard - 1)) {
        $fail += ("R1 in " + $name + " the guard call must sit right under the IsMiss if-line" +
                  " (if at " + $iIf + ", call at " + $iGuard + ")")
    }
    $iRet = -1
    for ($k = 0; $k -lt $body.Count; $k++) { if ($body[$k] -match '\breturn\b') { $iRet = $k; break } }
    if ($iRet -ge 0 -and $iGuard -gt $iRet) {
        $fail += ("R1 in " + $name + " the guard runs after the first return (guard " + $iGuard +
                  ", return " + $iRet + ") - 护栏必须赶在早退之前, 否则 NULL 接收者永远静默")
    }
}

# ---- R2: 伪接收者豁免三族 + 问的顺序 ----
$pseudoBody = Get-Body262 $invLines '^static int32_t vb6_ComLateBindIsPseudo\s*\('
if ($pseudoBody.Count -lt 4) { $fail += 'R2 cannot locate vb6_ComLateBindIsPseudo' }
else {
    foreach ($fam in @('vb6_UC_IsFont(', 'vb6_UC_IsExtender(', 'vb6_Host_IsHostObject(')) {
        $n = Count-In262 $pseudoBody $fam
        if ($n -ne 1) { $fail += ("R2 pseudo-exempt families: " + $fam + " = " + $n + " (want exactly 1)") }
    }
}
$missBody = Get-Body262 $invLines '^static int32_t vb6_ComLateBindIsMiss\s*\('
if ($missBody.Count -lt 3) { $fail += 'R2 cannot locate vb6_ComLateBindIsMiss' }
else {
    # 顺序按**字符偏移**比, 不按行号比: 两句写在同一行里也是反的 (行号比会看不见)。
    $missText = $missBody -join "`n"
    $iPseudo = $missText.IndexOf('vb6_ComLateBindIsPseudo(')
    $iDisp   = $missText.IndexOf('vb6_ComIsDispatchable(')
    if ($iPseudo -lt 0 -or $iDisp -lt 0) {
        $fail += 'R2 vb6_ComLateBindIsMiss must ask both the pseudo predicate and the dispatchability oracle'
    } elseif ($iPseudo -gt $iDisp) {
        $fail += ("R2 vb6_ComLateBindIsMiss asks dispatchability before the pseudo predicate (pseudo at " + $iPseudo +
                  ", dispatchable at " + $iDisp + ") - 顺序反了会把合法宿主对象判成漏")
    }
}

# ---- R3: 护栏不改行为 ----
$guardBody = Get-Body262 $invLines '^static void vb6_ComLateBindMiss\s*\('
if ($guardBody.Count -lt 4) { $fail += 'R3 cannot locate vb6_ComLateBindMiss' }
else {
    $nInc = Count-In262 $guardBody 'g_com_latebind_miss++;'
    if ($nInc -ne 1) { $fail += ("R3 counter increments inside the guard = " + $nInc + " (want exactly 1)") }
    $nMark = Count-In262 $guardBody '[C3_LATEBIND_MISS #'
    if ($nMark -ne 1) { $fail += ("R3 the [C3_LATEBIND_MISS #] marker inside the guard = " + $nMark + " (want exactly 1)") }
    foreach ($bad in @('vb6_RaiseError', 'exit(', 'TerminateProcess', 'abort')) {
        $n = Count-In262 $guardBody $bad
        if ($n -ne 0) {
            $fail += ("R3 the guard contains " + $bad + " (" + $n + ") - 护栏只许响亮, 不许改行为" +
                      " (无处理器的 vb6_RaiseError 会 exit(424) 把进程带走)")
        }
    }
    $nRet = 0
    foreach ($l in $guardBody) { if ($l -match '\breturn\b') { $nRet++ } }
    if ($nRet -ne 0) { $fail += ("R3 the guard returns " + $nRet + " time(s) - 它是 void 出口, 带控制流就成了一种行为变更") }
}
$nMarkAll = ([regex]::Matches($invCode, '\[C3_LATEBIND_MISS #')).Count
if ($nMarkAll -ne 1) {
    $fail += ("R3 the [C3_LATEBIND_MISS #] marker appears " + $nMarkAll +
              " times in vb6com_invoke.c (want 1) - 第二枚出口就是第二份计数口径")
}

# ---- R4: 计数器出口只住一处 ----
$nDefCount = ([regex]::Matches($invCode, 'int32_t\s+vb6_ComLateBindMissCount\s*\(\s*void\s*\)')).Count
if ($nDefCount -ne 1) { $fail += ("R4 definitions of vb6_ComLateBindMissCount = " + $nDefCount + " (want exactly 1)") }
$nVarDef = ([regex]::Matches($invCode, 'static int32_t g_com_latebind_miss\s*=')).Count
if ($nVarDef -ne 1) { $fail += ("R4 definitions of g_com_latebind_miss = " + $nVarDef + " (want exactly 1)") }
foreach ($h in @($comH, $clsH)) {
    $hc = Strip-Comments262 ((Read-Lines262 $h) -join "`n")
    $n = ([regex]::Matches($hc, 'int32_t\s+vb6_ComLateBindMissCount\s*\(\s*void\s*\)')).Count
    if ($n -ne 1) {
        $fail += ("R4 declaration of vb6_ComLateBindMissCount in " + (Split-Path -Leaf $h) + " = " + $n + " (want 1)")
    }
}

# ---- R5: 宿主通道的兄弟报告 (同一刀的另一半) ----
$hostCode = Strip-Comments262 ((Read-Lines262 $hostC) -join "`n")
$nHostDef = ([regex]::Matches($hostCode, 'int32_t\s+vb6_Host_UnknownMemberCount\s*\(\s*void\s*\)')).Count
if ($nHostDef -ne 1) { $fail += ("R5 definitions of vb6_Host_UnknownMemberCount = " + $nHostDef + " (want exactly 1)") }
$ctrlCode = Strip-Comments262 ((Read-Lines262 $ctrlH) -join "`n")
$nHostDec = ([regex]::Matches($ctrlCode, 'int32_t\s+vb6_Host_UnknownMemberCount\s*\(\s*void\s*\)')).Count
if ($nHostDec -ne 1) { $fail += ("R5 declaration of vb6_Host_UnknownMemberCount in vb6forms_controls.h = " + $nHostDec + " (want 1)") }
foreach ($inc in @($incCall, $incGet, $incSet)) {
    $ic = Strip-Comments262 ((Read-Lines262 $inc) -join "`n")
    $n = ([regex]::Matches($ic, '\[C3_HOST_MEMBER_MISS #')).Count
    if ($n -ne 1) {
        $fail += ("R5 the [C3_HOST_MEMBER_MISS #] marker in " + (Split-Path -Leaf $inc) + " = " + $n + " (want 1)")
    }
}

# ---- R6: 判据在 runner 里落地 ----
$rtCode = Strip-Comments262 ((Read-Lines262 $rtPath) -join "`n")
$nAllow = ([regex]::Matches($rtCode, 'LateBindMissAllow')).Count
if ($nAllow -lt 3) { $fail += ("R6 LateBindMissAllow sites in run_tests.ps1 = " + $nAllow + " (want >=3: table + two readers)") }
$nDetailDef = ([regex]::Matches($rtCode, 'function Get-LateBindMissDetail')).Count
if ($nDetailDef -ne 1) { $fail += ("R6 Get-LateBindMissDetail definitions = " + $nDetailDef + " (want 1)") }
$nCountDef = ([regex]::Matches($rtCode, 'function Get-LateBindMissCount')).Count
if ($nCountDef -ne 1) { $fail += ("R6 Get-LateBindMissCount definitions = " + $nCountDef + " (want 1)") }
$nUse = ([regex]::Matches($rtCode, 'Get-LateBindMissDetail\s+-Name')).Count
if ($nUse -ne 3) {
    $fail += ("R6 Get-LateBindMissDetail call sites in run_tests.ps1 = " + $nUse +
              " (want 3: Invoke-TestExe 的 .NET 路 / Start-Process 路 / Test-GuiVbp 路)")
}
# 并行 runspace 里那份**内联**计数: runspace 调不到脚本函数, 不内联就等于没判。
$inlineNeedle = 'if ([string]$l -match ''\[C3_LATEBIND_MISS #'''
$nInline = ([regex]::Matches($rtCode, [regex]::Escape($inlineNeedle))).Count
if ($nInline -ne 2) {
    $fail += ("R6 inline [C3_LATEBIND_MISS #] counters in run_tests.ps1 = " + $nInline +
              " (want 2: Get-LateBindMissCount + 并行 runspace 那份)")
}

# ---- R7: 负控在册 ----
if (-not (Test-Path $fixture)) { $fail += ("R7 the negative-control fixture is gone: " + $fixture) }
$nReg = ([regex]::Matches($rtCode, 'b446_latebind_guard')).Count
if ($nReg -ne 2) {
    $fail += ("R7 b446_latebind_guard registrations in run_tests.ps1 = " + $nReg +
              " (want 2: x64 + x86) - 负控必须两架构都在 (调用约定按指针宽度变)")
}
$allowRows = [regex]::Matches($rtCode, "(?m)^\s*'B446'\s*=\s*(\d+)\s*(#.*)?$")
$nAllowEntry = $allowRows.Count
if ($nAllowEntry -ne 1) {
    $fail += ("R7 the expected-count table entry 'B446' = 1 = " + $nAllowEntry +
              " (want exactly 1) - 负控要**恰好**响一次, 写成开关就没有判据")
} elseif ($allowRows[0].Groups[1].Value -ne '1') {
    $fail += ("R7 the expected-count table entry 'B446' = " + $allowRows[0].Groups[1].Value +
              " (want 1) - 负控必须**恰好**响一次; 0 等于把这条判据关掉")
}

# ---- R8: 这枚哨兵自己不许是死码 ----
$nSelfReg = ([regex]::Matches($rtCode, 'Test-ComLatebindGuard')).Count
if ($nSelfReg -ne 2) {
    $fail += ("R8 Test-ComLatebindGuard sites in run_tests.ps1 = " + $nSelfReg +
              " (want 2: 定义 + 静态分片登记) - 哨兵自己不登记就是死码, 门永远不跑它")
}

if ($fail.Count -gt 0) {
    foreach ($f in $fail) { Write-Host ("FAIL " + $f) }
    exit 1
}
Write-Host ("PASS check_com_latebind_guard: exits=7 guarded=7 pseudo-exempt=3 order=pseudo-first guard=behavior-preserving counter-outlets=2 host-sibling=3 runner-sites=3 inline-counters=2 negative-control=2 self-registered=1")
exit 0
