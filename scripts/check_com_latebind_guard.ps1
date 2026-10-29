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
#       声明 1 次。⚠ 第二百七十三刀把「报一行」的格式从三份手抄收成一枚出口
#       (uc_hostmodel.c 里的 vb6_ho_reportUnknown) ⇒ 现在钉的是: 标记全 TU 恰好一枚 +
#       调用站点 Call=2 (字体那一臂 + 尾巴) / GetProp=2 / SetProp=2 + 字体方法主人
#       vb6_ho_fontMethod 定义 1 次、被 Call 那一臂用 1 次。那一族的静默漏与本刀同源。
#   R6  判据在 runner 里落地: run_tests.ps1 的那张期望表 + 三个读数出口
#       (Invoke-TestExe 的 .NET 路 / Start-Process 路 / Test-GuiVbp 路) + 并行
#       runspace 里那份**内联**计数 (runspace 调不到脚本函数, 不内联就等于没判)。
#   R7  负控在册: tests\b446_latebind\B446.vbp 在, 且在 run_tests.ps1 里登记两行
#       (x64 + x86) —— 负控必须恰好响一次 (Nothing 接收者), 多一次少一次都红。
#   R9  第二百七十六刀那一档 (Font.hFont) 的形状: ①「八档 → LOGFONT」只住一位主人
#       (uc_controls.c 里 CreateFontIndirectW 恰好 1, 而 vb6rtl_com.c 那份就地抄写的 lfWeight 恰好 0 = 负控);
#       ②缓存自失效那条腿两段都在 (作废旧句柄 + 落新句柄 各 1);
#       ③kind 5 = 句档在两张 switch 里都有答案 (读那张发 VT_I8, 写那张按只读忽略);
#       ④宿主通道与 COM 出口同一个答案 (vb6_UC_FontHFont 定义 1 / 在 uc_hostmodel.c 用 1);
#       ⑤四行 K276: 读数在 runner 的期望表里在册。
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
$ucC      = Join-Path $Root 'src\rtl\core\vb6forms\uc\uc_controls.c'
$rtlC     = Join-Path $Root 'src\rtl\core\vb6rtl\vb6rtl_com.c'
$formsC   = Join-Path $Root 'src\rtl\core\vb6forms\vb6forms.c'
$ctrlC    = Join-Path $Root 'src\rtl\core\vb6forms\vb6forms_ctrl.c'
$hostWinC = Join-Path $Root 'src\rtl\core\vb6forms\uc\uc_host_window.c'
$ucCreateC = Join-Path $Root 'src\rtl\core\vb6forms\uc\detail\uc_host_create.inc'
$formWpInc = Join-Path $Root 'src\backend\detail\module\cgen_form_wndproc_create.inc'
$ucHostC      = Join-Path $Root 'src\rtl\core\vb6forms\uc\uc_host.c'
$ucHostmodelC = Join-Path $Root 'src\rtl\core\vb6forms\uc\uc_hostmodel.c'
$ucControlsC  = Join-Path $Root 'src\rtl\core\vb6forms\uc\uc_controls.c'
$gpInc        = Join-Path $Root 'src\rtl\core\vb6forms\uc\detail\uc_hostmodel_getprop.inc'
$ucInternalH  = Join-Path $Root 'src\rtl\core\vb6forms\vb6forms_uc_internal.h'
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

$missing = @($invPath, $comH, $clsH, $hostC, $ctrlH, $incCall, $incGet, $incSet, $rtPath, $ucC, $rtlC, $formsC, $ctrlC, $hostWinC, $ucCreateC, $formWpInc) |
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
# 第二百七十三刀: 报告格式有了一位主人 (vb6_ho_reportUnknown, 住 uc_hostmodel.c)。
# 于是这里钉三件事: ① 主人定义恰好一次; ② 那枚标记在全 TU (主人 + 三份 .inc) 恰好一次
# —— 有人手抄回格式就红; ③ 站点数钉死 (Call 两处 = 字体那一臂 + 尾巴, GetProp/SetProp 各一)。
$nReporter = ([regex]::Matches($hostCode, 'static\s+void\s+vb6_ho_reportUnknown\s*\(')).Count
if ($nReporter -ne 1) { $fail += ("R5 definitions of vb6_ho_reportUnknown = " + $nReporter + " (want exactly 1)") }
$markerAll = ([regex]::Matches($hostCode, '\[C3_HOST_MEMBER_MISS #')).Count
$wantSites = @{ $incCall = 2; $incGet = 2; $incSet = 2 }
foreach ($inc in @($incCall, $incGet, $incSet)) {
    $ic = Strip-Comments262 ((Read-Lines262 $inc) -join "`n")
    $markerAll += ([regex]::Matches($ic, '\[C3_HOST_MEMBER_MISS #')).Count
    $n = ([regex]::Matches($ic, 'vb6_ho_reportUnknown\s*\(')).Count
    if ($n -ne $wantSites[$inc]) {
        $fail += ("R5 calls of vb6_ho_reportUnknown in " + (Split-Path -Leaf $inc) + " = " + $n + " (want " + $wantSites[$inc] + ")")
    }
}
if ($markerAll -ne 1) { $fail += ("R5 the [C3_HOST_MEMBER_MISS #] marker across uc_hostmodel.c + the three .inc = " + $markerAll + " (want exactly 1 -- the format has one owner)") }
# 字体那一臂的主人 (第二百七十三刀新格): 定义一次、被 Call 那臂用一次, 且那条
# 「对任何方法名一律 return 1」的旧写法不许回来 (它就是把 13 发 Clone 变成静默的那一行)。
$nFontDef = ([regex]::Matches($hostCode, 'static\s+int32_t\s+vb6_ho_fontMethod\s*\(')).Count
if ($nFontDef -ne 1) { $fail += ("R5 definitions of vb6_ho_fontMethod = " + $nFontDef + " (want exactly 1)") }
$callCode = Strip-Comments262 ((Read-Lines262 $incCall) -join "`n")
$nFontUse = ([regex]::Matches($callCode, 'vb6_ho_fontMethod\s*\(')).Count
if ($nFontUse -ne 1) { $fail += ("R5 uses of vb6_ho_fontMethod in uc_hostmodel_call.inc = " + $nFontUse + " (want 1)") }
if ($callCode -match 'vb6_uc_isFont\(obj\)\)\s*return\s*1\s*;') {
    $fail += "R5 the blanket 'if (vb6_uc_isFont(obj)) return 1;' is back -- it silences every font method (knife 272 reading)"
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

# ---- R9: 第二百七十六刀那一档 (Font.hFont) 的形状 ----
# 针全部取在**剒掉注释之后**的代码上 (与 R1..R8 同一把尺): 注释里再次提到这些名字不会把读数数高。
$ucCode  = Strip-Comments262 ((Read-Lines262 $ucC) -join "`n")
$rtlCode = Strip-Comments262 ((Read-Lines262 $rtlC) -join "`n")
$nCreate = ([regex]::Matches($ucCode, 'CreateFontIndirectW')).Count
if ($nCreate -ne 1) {
    $fail += ("R9 CreateFontIndirectW in uc_controls.c = " + $nCreate +
              " (want 1) - 八档→LOGFONT 这一问只许住一位主人 (vb6_UC_FontLogFont)")
}
$nCopied = ([regex]::Matches($rtlCode, 'lfWeight')).Count
if ($nCopied -ne 0) {
    $fail += ("R9 the in-place eight-fields->LOGFONT copy is back in vb6rtl_com.c (lfWeight = " + $nCopied +
              ") (want 0) - 那份抄写回来就是第二套字重/兜底 (negative control)")
}
$nInval = ([regex]::Matches($ucCode, 'DeleteObject\(\(HFONT\)f->hFontCache\)')).Count
if ($nInval -ne 1) {
    $fail += ("R9 the cache-invalidation leg in uc_controls.c = " + $nInval +
              " (want 1) - 八档改了不作废缓存 = 把旧字体继续交给新的 Bold")
}
$nStore = ([regex]::Matches($ucCode, 'f->hFontCache = \(void\*\)h;')).Count
if ($nStore -ne 1) { $fail += ("R9 the cache-store leg in uc_controls.c = " + $nStore + " (want 1)") }
$nKind5 = ([regex]::Matches($invCode, 'case 5:')).Count
if ($nKind5 -ne 2) {
    $fail += ("R9 'case 5:' in vb6com_invoke.c = " + $nKind5 +
              " (want 2: the read arm packs VT_I8, the write arm ignores a propget-only name)")
}
$nHArm = ([regex]::Matches($ucCode, '_wcsicmp\(name, L"hFont"\)')).Count
if ($nHArm -ne 1) { $fail += ("R9 hFont arm in vb6_UC_FontField = " + $nHArm + " (want 1)") }
$nHDef = ([regex]::Matches($ucCode, 'void\* vb6_UC_FontHFont\s*\(')).Count
if ($nHDef -ne 1) { $fail += ("R9 definitions of vb6_UC_FontHFont = " + $nHDef + " (want 1)") }
$nHUse = ([regex]::Matches($hostCode, 'vb6_UC_FontHFont\s*\(')).Count
if ($nHUse -ne 1) {
    $fail += ("R9 uses of vb6_UC_FontHFont in uc_hostmodel.c = " + $nHUse +
              " (want 1) - 宿主通道与 COM 出口必须从同一位主人拿答案")
}
$nK276 = ([regex]::Matches($rtCode, 'K276:')).Count
if ($nK276 -ne 7) {
    $fail += ("R9 K276 expectation rows in run_tests.ps1 = " + $nK276 +
              " (want 7) - 判据升格进 runner 才算在册 (R6 同族)")
}

# ---- R10: 第二百七十七刀那一档 (磅 -> 像素 + 默认字族名) 的形状 ----
# 唯一主人一枚、调用方三处、两条抄写的反面针, 加"兜底名不许再写 Segoe UI"。
$formsCode = Strip-Comments262 ((Read-Lines262 $formsC) -join "`n")
$ctrlCode  = Strip-Comments262 ((Read-Lines262 $ctrlC) -join "`n")
$hwCode    = Strip-Comments262 ((Read-Lines262 $hostWinC) -join "`n")
$nHDef2 = ([regex]::Matches($ucCode, 'int32_t\s+vb6_UC_FontHeightPx\s*\(')).Count
if ($nHDef2 -ne 1) { $fail += ("R10 definition of vb6_UC_FontHeightPx = " + $nHDef2 + " (want 1)") }
$nHUse2 = ([regex]::Matches($ucCode, '=\s*vb6_UC_FontHeightPx\(')).Count +
         ([regex]::Matches($formsCode, 'vb6_UC_FontHeightPx\(')).Count +
         ([regex]::Matches($ctrlCode, 'vb6_UC_FontHeightPx\(')).Count
if ($nHUse2 -ne 3) {
    $fail += ("R10 consumers of vb6_UC_FontHeightPx = " + $nHUse2 + " (want 3: uc_controls / vb6forms / vb6forms_ctrl)")
}
$nMulDiv = ([regex]::Matches($formsCode, 'MulDiv\(825')).Count
$nHalf  = ([regex]::Matches($ctrlCode, '72\.0f\s*\+\s*0\.5f')).Count
$nHalf2 = ([regex]::Matches($ucCode, '72\.0\s*\+\s*0\.5')).Count
if ($nMulDiv + $nHalf + $nHalf2 -ne 0) {
    $fail += ("R10 the hand-copied conversions are back (muldiv=" + $nMulDiv +
              " setter=" + $nHalf + " owner=" + $nHalf2 + ") (want 0) - 第二套口径就是第二套答案")
}
$nName = ([regex]::Matches($hwCode, 'SysAllocString\(L"MS Sans Serif"\)')).Count
$nOld  = ([regex]::Matches($hwCode, 'SysAllocString\(L"Segoe UI"\)')).Count
if ($nName -ne 1 -or $nOld -ne 0) {
    $fail += ("R10 UC default family: MS Sans Serif = " + $nName + " (want 1), Segoe UI = " + $nOld +
              " (want 0) - 参考那 69 枚窗口读回来的是前者")
}

# ---- R11: 第二百七十八刀那一档 (把字体**发到**窗体顶层与 UC 宿主那两户) 的形状 ----
# 前两刀让伪字体答得出八档与 hFont, 这一刀管"发出去"那一站。规矩: 一站只拼答案, 造 / WM_SETFONT /
# 自存 / 删旧 全借 vb6forms_ctrl.c 里那位既有主人 —— 第二份发信人就是第二套作废账 (它自带
# "只删我们自己造的那张"的所有权判断)。⚠ 这一档买的是**存进 VB6_CtrlFont 那一格**, 不是
# "窗口自己答 WM_GETFONT": 探针 .local/k278/wmsetfont.c 两架构实测 DefWindowProc **不记** WM_SETFONT
# (裸注册类发完再问回 0), 所以自注册类要答那一问得另有落点 (第二百七十九刀的靶, 见台账行订正)。
$createCode = Strip-Comments262 ((Read-Lines262 $ucCreateC) -join "`n")
$nADef = ([regex]::Matches($ucCode, 'void\s+vb6_UC_ApplyFontToWindow\s*\(')).Count
if ($nADef -ne 1) { $fail += ("R11 definition of vb6_UC_ApplyFontToWindow = " + $nADef + " (want 1)") }
$nAUc = ([regex]::Matches($createCode, 'vb6_UC_ApplyFontToWindow\(hwnd, r->font\)')).Count
if ($nAUc -ne 1) {
    $fail += ("R11 UC-host send site (design-time font) = " + $nAUc +
              " (want 1 in uc_host_create.inc) - 这一户退回「没人发」就是尺上那 27 枚空字体")
}
$nAForm = ([regex]::Matches($formsCode, 'vb6_UC_ApplyFontToWindow\(hwnd, NULL\)')).Count
if ($nAForm -ne 1) {
    $fail += ("R11 form-toplevel send site (VB6 default arm) = " + $nAForm +
              " (want 1 in vb6forms.c) - 顶层窗体没人发字体 = 参考答 -13 而 C3 读回空那一格")
}
$nASet = ([regex]::Matches($ucCode, 'WM_SETFONT')).Count
if ($nASet -ne 0) {
    $fail += ("R11 hand-written WM_SETFONT senders in uc_controls.c = " + $nASet +
              " (want 0) - 发信人要抄第二份, 作废那本账就有人对不上")
}

# ---- R12: 第二百八十刀那一档 (窗体顶层**本人**答字体那一问) 的形状 ----
# 第二百七十九刀让自注册的 UC 宿主类本人答，这一刀管**发码发出来的**那枚窗体顶层。规矩同一条:
# 发码那边只许转调一位主人；自己 GetPropW 那一格 = 第二本账（D10 守的就是这个形状）。
$ctrlHdrCode = Strip-Comments262 ((Read-Lines262 $ctrlH) -join "`n")
$formWpCode  = Strip-Comments262 ((Read-Lines262 $formWpInc) -join "`n")
$nFA = ([regex]::Matches($ctrlCode, 'void\*\s+vb6_FormAnswerGetFont\w*\s*\(void\*\s+hwnd\)\s*\{')).Count
if ($nFA -ne 1) { $fail += ('R12 definition of vb6_FormAnswerGetFont = ' + $nFA + ' (want 1)') }
$nFAbody = ([regex]::Matches($ctrlCode, 'vb6_FormAnswerGetFont\w*\s*\(void\*\s+hwnd\)\s*\{\s*\r?\n\s*return\s+\(void\*\)\s*vb6_ControlFontStored')).Count
if ($nFAbody -ne 1) { $fail += ('R12 vb6_FormAnswerGetFont must hand the question to vb6_ControlFontStored = ' + $nFAbody + ' (want 1; 0 = 薄壳自己另找一遍 = 第二本账)') }
$nFADec = ([regex]::Matches($ctrlHdrCode, '(?m)^void\*\s+vb6_FormAnswerGetFont\s*\(void\*\s+hwnd\);\s*$')).Count
if ($nFADec -ne 1) { $fail += ('R12 vb6_FormAnswerGetFont declared in the header the generated TU sees = ' + $nFADec + ' (want 1; 0 = 发进用户 TU 的名字没声明 -> cl C2146/C4013)') }
$nFAEmit = ([regex]::Matches($formWpCode, 'vb6_FormAnswerGetFont\(\(void\*\)hwnd\)')).Count
$nFACase = ([regex]::Matches($formWpCode, 'case WM_GETFONT:')).Count
if ($nFAEmit -ne 1 -or $nFACase -ne 1) { $fail += ('R12 emitted form-toplevel arm = emit ' + $nFAEmit + ' + case ' + $nFACase + ' (want 1 + 1; 0 = 顶层又没人答，尺上那 1 枚空)') }
$nFABypass = ([regex]::Matches($formWpCode, 'VB6_CtrlFont')).Count
if ($nFABypass -ne 0) { $fail += ('R12 generated code reads the VB6_CtrlFont slot directly = ' + $nFABypass + ' (want 0) - 绕过主人自己读那一格就是第二本账') }


# ---- R13: 第二百八十二刀那一档 (无窗宿主 InvisibleAtRuntime 进容器枚举) 的形状 ----
# 第二百五十八刀不建宿主窗口之后, Controls/ParentControls/TypeName/宿主属性读四条路的钥匙全是 hwnd,
# 那三枚 ImageList 就从集合里消失了 => 靶工程 `For Each ControlEnum In UserControl.ParentControls` +
# `TypeName(...) = "ImageList"` + `.hImageList` 整条接线断掉, 控件上一个图标都不挂 (台账行 5041 的读数)。
# 规矩: 按实例查登记记录只许一位主人 (vb6_ho_findInstance, 且只认 hwnd==NULL 那一档 —— 给 ho_find 加
# 第二条钥匙会把一枚 UC 实例认成别的控件的窗口, uc_host.c 里那段「已经就是 HWND 就别再查表」写的就是它),
# 四条路各自转调它, 不许再抄一份指针比对。
$hostCode = Strip-Comments262 ((Read-Lines262 $ucHostC) -join "`n")
$hmCode   = Strip-Comments262 ((Read-Lines262 $ucHostmodelC) -join "`n")
$collCode = Strip-Comments262 ((Read-Lines262 $ucControlsC) -join "`n")
$gpCode   = Strip-Comments262 ((Read-Lines262 $gpInc) -join "`n")
$ucIntHdr = Strip-Comments262 ((Read-Lines262 $ucInternalH) -join "`n")
$nWO = ([regex]::Matches($hostCode, 'vb6_HostObjRec\*\s+vb6_ho_findInstance\w*\s*\(')).Count
if ($nWO -ne 1) { $fail += ('R13 definition of vb6_ho_findInstance = ' + $nWO + ' (want 1)') }
$nWOdecl = ([regex]::Matches($ucIntHdr, '(?m)^vb6_HostObjRec\*\s+vb6_ho_findInstance\s*\(')).Count
if ($nWOdecl -ne 1) { $fail += ('R13 vb6_ho_findInstance declared for the uc module = ' + $nWOdecl + ' (want 1; 0 = 三个调用点各自 extern 一份)') }
$nWOgp = ([regex]::Matches($gpCode, 'vb6_ho_findInstance\(obj\)')).Count
$nWOtn = ([regex]::Matches($hmCode, 'vb6_ho_findInstance\(obj\)')).Count
if ($nWOgp -ne 1 -or $nWOtn -ne 1) { $fail += ('R13 instance-lookup outlets: getprop ' + $nWOgp + ' + TypeName ' + $nWOtn + ' (want 1 + 1; 0 = 无窗实例又走不到自有 Property 那一站)') }
$nWArm = ([regex]::Matches($collCode, 'vb6_uc_findByInstance\(rec->instance\)')).Count
$nWArm2 = ([regex]::Matches($collCode, 'vb6_uc_findByInstance\(r->instance\)')).Count
$nWItem = ([regex]::Matches($collCode, 'item = r->instance;')).Count
$nWRet = ([regex]::Matches($collCode, 'return item;')).Count
if ($nWArm -ne 1 -or $nWArm2 -ne 1 -or $nWItem -ne 1 -or $nWRet -ne 1) { $fail += ('R13 windowless arms in the collection: collectChildren ' + $nWArm + ' + by-name-lookup ' + $nWArm2 + ' + by-name-delivers-instance ' + $nWItem + ' + returns-it ' + $nWRet + ' (want 1 + 1 + 1 + 1; 0 = 臂在但交不出实例, 等于没有)') }
$nWBlind = ([regex]::Matches($collCode, 'isForm \|\| !r->hwnd\)\s*continue')).Count
if ($nWBlind -ne 0) { $fail += ('R13 the old hwnd-only gate is back in uc_controls.c = ' + $nWBlind + ' (want 0) - 那道闸一回来, ImageList 就又不在集合里了') }
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
Write-Host ("PASS check_com_latebind_guard: exits=7 guarded=7 pseudo-exempt=3 order=pseudo-first guard=behavior-preserving counter-outlets=2 host-sibling=3 runner-sites=3 inline-counters=2 negative-control=2 hfont-owner=1 hfont-kind5=2 height-owner=1 height-consumers=3 name-default=ms-sans-serif apply-owner=1 apply-senders=2 self-registered=1") form-ans-owner=1 form-ans-body=1 form-ans-decl=1 form-ans-emit=1 windowless-owner=1 windowless-outlets=2 windowless-arms=2
exit 0
