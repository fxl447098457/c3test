# ============================================================
# check_com_server_error_frame.ps1 — 账 #305 §B185 (第七十七刀) 的结构性哨兵
#
# 为什么单开这一道：这一格的缺陷形状是「服务端进门那趟压根没有出口」，而且它的后果不在
# 服务端里 —— `ComObj_Invoke` 收下 pExcepInfo 却从不碰、`invokeFunc` 返回 void、末尾无条件
# `return S_OK`，于是 VB 体里没人接住的一条运行期错误一路走到 `vb6_RaiseError`，那里没帧
# ⇒ 打一行 stderr 再 `exit(错误号)` ⇒ **死的是调用它的那个进程**。两台实测(改前)：越界写
# Public Integer 字段 RC=6、体里一句 Err.Raise 5 RC=5，连 `cc_dll` 那批夹具都只能靠
# 「探针没走到那一臂」侥幸绿着 ⇒ 发码、链接、语料 A/B 全都看不见这一格，只有真跑看得见。
# 行为判据住在 tests\b185_srv + Test-DispatchInvoke(两台)；这里钉的是**别再退回两遍形状**。
#
# 规则：
#   R1  那一处只许有一枚帧：vb6comserver_obj.c 的 ComObj_Invoke 体内 `setjmp(` 恰好 1 次、
#       `vb6_SaveErrState();` 与 `vb6_RestoreErrState();` 各恰好 1 次，而且 `setjmp` 那一行
#       必须排在 `method->invokeFunc(` 之前(帧要先登记再调进去，反过来就是「抛错时还没人接」)。
#   R2  登记顺序照 cgen 的 On Error GoTo 协议：`setjmp` 之后必须先登记 ptr/set/active 再调，
#       即 `vb6_error_jmp_ptr = &srvJmp` 那一行在 `setjmp(` 之后、`invokeFunc(` 之前。
#   R3  交回形状只住一处：`0x800A0000` 这个数在全 RTL 只许出现 1 次(就在 vb6_ComFailExcep
#       的函数体里)，且 `coll_fail` 体内不许再出现 `scode =`(那枚内置 Collection 的旧主人
#       必须仍是转调)。这一枚出口自己：声明 1 + 定义 1 + 调用点 2(coll_fail 与 ComObj_Invoke)。
#   R4  失败路径不许跳过收尾：ComObj_Invoke 末尾那两行无条件收尾(`if (args)` / `if (coercedArgs)`
#       各 CoTaskMemFree)必须还在，且 `vb6_ComFailExcep(` 那一次调用排在它们之后
#       (错误路径与成功路径走同一条收尾，否则每次抛错漏两批缓冲)。
#
# 用法: powershell -File scripts\check_com_server_error_frame.ps1 [-Root <目录>]   (PASS ⇒ exit 0)
# 负控在 src 的副本上跑(-Root)，共享工作树一格不动。

param([string]$Root = "")

$ErrorActionPreference = 'Stop'
if ($Root -eq "") { $Root = Split-Path -Parent $PSScriptRoot }

$objPath  = Join-Path $Root 'src\rtl\core\vb6comserver\vb6comserver_obj.c'
$comPath  = Join-Path $Root 'src\rtl\core\vb6com\vb6com.c'
$colPath  = Join-Path $Root 'src\rtl\core\vb6com\vb6com_collection.c'
$hPath    = Join-Path $Root 'src\rtl\core\vb6com\vb6com_internal.h'

$fail = @()

# 同一文件里那条 R3 的尺子只量代码：注释里引那句形状是这一仓的常规做法(§B182 的口径
# 「谁在提这句文本都算」反过来用 —— 不许把注释改哑来迁就尺子)，所以先剥注释再数。
function Strip-Comments185([string]$text) {
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

function Read-Lines([string]$p) {
    $raw = [System.IO.File]::ReadAllText($p)
    return ,($raw -split "`r?`n")
}

# 只取函数体：从函数头那一行到下一个顶格 '}' (这仓的 C 文件都用顶格收尾)
function Get-Body([string[]]$lines, [string]$headRx) {
    $s = -1
    for ($i = 0; $i -lt $lines.Count; $i++) { if ($lines[$i] -match $headRx) { $s = $i; break } }
    if ($s -lt 0) { return @() }
    for ($j = $s; $j -lt $lines.Count; $j++) {
        if ($j -gt $s -and $lines[$j] -match '^\}') { return $lines[$s..($j - 1)] }
    }
    return $lines[$s..($lines.Count - 1)]
}

function Count-In([string[]]$body, [string]$needle) {
    $n = 0
    foreach ($l in $body) { if ($l.Contains($needle)) { $n++ } }
    return $n
}
function Index-Of([string[]]$body, [string]$needle) {
    for ($i = 0; $i -lt $body.Count; $i++) { if ($body[$i].Contains($needle)) { return $i } }
    return -1
}

$obj  = Read-Lines $objPath
$invokeBody = Get-Body $obj 'static HRESULT STDMETHODCALLTYPE ComObj_Invoke'
if ($invokeBody.Count -lt 10) { Write-Host 'FAIL R1 cannot locate ComObj_Invoke body'; exit 1 }

# ---- R1 ----
$nSet  = Count-In $invokeBody 'setjmp('
$nSave = Count-In $invokeBody 'vb6_SaveErrState();'
$nRest = Count-In $invokeBody 'vb6_RestoreErrState();'
$nCall = Count-In $invokeBody 'method->invokeFunc('
if ($nSet -ne 1)  { $fail += "R1 setjmp in ComObj_Invoke = $nSet (want exactly 1)" }
if ($nSave -ne 1) { $fail += "R1 vb6_SaveErrState in ComObj_Invoke = $nSave (want exactly 1)" }
if ($nRest -ne 1) { $fail += "R1 vb6_RestoreErrState in ComObj_Invoke = $nRest (want exactly 1)" }
if ($nCall -ne 1) { $fail += "R1 method->invokeFunc in ComObj_Invoke = $nCall (want exactly 1)" }

# ---- R2 ----
$iSet  = Index-Of $invokeBody 'setjmp('
$iPtr  = Index-Of $invokeBody 'vb6_error_jmp_ptr = &srvJmp'
$iCall = Index-Of $invokeBody 'method->invokeFunc('
if ($iSet -lt 0 -or $iPtr -lt 0 -or $iCall -lt 0) {
    $fail += "R2 missing one of setjmp/jmp_ptr-registration/invokeFunc (set=$iSet ptr=$iPtr call=$iCall)" }
elseif (-not ($iSet -lt $iPtr -and $iPtr -lt $iCall)) {
    $fail += "R2 order is setjmp($iSet) -> jmp_ptr($iPtr) -> invokeFunc($iCall); must register the frame before calling in" }

# ---- R3 ----
$allRtl = @(Get-ChildItem -Path (Join-Path $Root 'src\rtl') -Recurse -Include *.c, *.h, *.inc)
$nShape = 0
foreach ($f in $allRtl) {
    $t = Strip-Comments185 ([System.IO.File]::ReadAllText($f.FullName))
    $nShape += ([regex]::Matches($t, '0x800A0000')).Count
}
if ($nShape -ne 1) { $fail += "R3 the 0x800A0000 shape appears $nShape times in src\rtl (must live in exactly one exit)" }

$col    = Read-Lines $colPath
$failBody = Get-Body $col '^static HRESULT coll_fail'
if ((Count-In $failBody 'scode =') -ne 0) {
    $fail += "R3 coll_fail assigns scode again - it must stay a thin shell over vb6_ComFailExcep" }
if ((Count-In $failBody 'vb6_ComFailExcep(') -ne 1) {
    $fail += "R3 coll_fail does not call vb6_ComFailExcep exactly once" }

$com  = Read-Lines $comPath
$defB = Get-Body $com '^HRESULT vb6_ComFailExcep'
if ($defB.Count -lt 5) { $fail += "R3 vb6_ComFailExcep definition not found in vb6com.c" }
$decls = 0
foreach ($l in (Read-Lines $hPath)) { if ($l.Contains('vb6_ComFailExcep')) { $decls++ } }
if ($decls -ne 1) { $fail += "R3 declaration count in vb6com_internal.h = $decls (want 1)" }
# 调用点 = 真的把那枚出口**用出去**的两处；本文件顶部那句 extern 声明不算(否则这一计数会随
# 跨族声明的写法漂，而漂的不是这道决定)。
$objCode = Strip-Comments185 (($obj -join "`n"))
$colCode = Strip-Comments185 ((Read-Lines $colPath) -join "`n")
$comCode = Strip-Comments185 ((Read-Lines $comPath) -join "`n")
$callsites = ([regex]::Matches($objCode, 'return vb6_ComFailExcep\s*\(')).Count + ([regex]::Matches($colCode, 'return vb6_ComFailExcep\s*\(')).Count
if ($callsites -ne 2) { $fail += "R3 callers of vb6_ComFailExcep = $callsites (want 2: coll_fail + ComObj_Invoke)" }
$externs = ([regex]::Matches($objCode, 'extern HRESULT vb6_ComFailExcep')).Count
if ($externs -ne 1) { $fail += "R3 cross-family extern of vb6_ComFailExcep in vb6comserver_obj.c = $externs (want 1)" }
$nDef = ([regex]::Matches($comCode, 'HRESULT vb6_ComFailExcep\s*\(')).Count
if ($nDef -ne 1) { $fail += "R3 definitions of vb6_ComFailExcep in vb6com.c = $nDef (want exactly 1)" }

# ---- R4 ----
# 收尾那一趟的两行(四空格那版; 早退分支里 12 空格的同名片段不算)必须还在, 且失败交回排在它们
# 之后 —— 否则每次抛错都漏 args/coercedArgs 两批缓冲, 连带 §B162 那枚数组那一臂。
function Index-Of-Re([string[]]$body, [string]$rx) {
    for ($i = 0; $i -lt $body.Count; $i++) { if ($body[$i] -match $rx) { return $i } }
    return -1
}
$iFreeA = Index-Of-Re $invokeBody '^    if \(args\) CoTaskMemFree\(args\);$'
$iFreeC = Index-Of-Re $invokeBody '^    if \(coercedArgs\) CoTaskMemFree\(coercedArgs\);$'
$iFailc = Index-Of $invokeBody 'return vb6_ComFailExcep('
if ($iFreeA -lt 0 -or $iFreeC -lt 0) {
    $fail += "R4 the unconditional cleanup tail (if (args)/if (coercedArgs) CoTaskMemFree) is gone from ComObj_Invoke" }
elseif ($iFailc -lt 0) {
    $fail += "R4 no failure return through vb6_ComFailExcep inside ComObj_Invoke" }
elseif (-not ($iFreeA -lt $iFailc -and $iFreeC -lt $iFailc)) {
    $fail += "R4 failure return($iFailc) must come after the cleanup tail args=$iFreeA coercedArgs=$iFreeC, or every raise leaks both buffers" }

if ($fail.Count -gt 0) {
    foreach ($f in $fail) { Write-Host ("FAIL " + $f) }
    exit 1
}
Write-Host ("PASS check_com_server_error_frame: frame=1 registration-ordered shape=1 callers=2 cleanup-before-return")
exit 0
