# 账 #209 + 账 #214 的结构性哨兵 (只扫源码, 不起 cl)
#
# 为什么单开这一道: 一维动态数组的**元素**访问在 RTL 里只有一个入口 —— VB6_SA_AT。
# 这一刀之前它是裸指针算术 (`((type*)((arr)->data))[idx - (arr)->lBound]`), 描述符是 NULL
# (动态数组从未 ReDim / 已被 Erase) 或下标越界时它一声不吭地读 NULL+0xc → 原生 0xC0000005。
# 实测 Charts 2020 ucChartBar demo 点 Random: 三次崩在同一偏移 0x1abf2, 符号化落在
# 生成代码 ucChartBar.c:591 的 `With m_Serie(Index)`。VB6 在这一条是运行时错误 9, 和
# vb6_UBound/vb6_LBound 的 rev2 同一族 —— 那两处已经抛 9, 这一处漏了。
#
# 现在两支都收在同一形状的出口上: 一维走 vb6_SaElemPtr, 多维走 vb6_SaNdElemPtr -- (各一条 inline 热路径 + 一条冷路径)
# 账 #214 是这一族的下一半: VB6_SA_ND_AT1/2/3 当初与 #209 修法前的一维一模一样 (描述符在不在、
# 每一维在不在范围, 一个都不问), 而 4 秩以上另有一条 `_ndoff_` 兜底 —— 那条更糟: 发码侧把实参写成
# `(int[]){i0, i1}` 只塞两枚下标却按实际秩数交出去, 于是 `Dim a4(1 To 2,1 To 2,1 To 2,1 To 2)`
# **在范围内也** 0xC0000005 (x86/x64 同形)。两支合起来存量 1468 处 / 4 份工程, 大头是 VBFlexGridDemo。
# 所以 A4 现在改成“一条都不许有”, 另加 A6 (多维那一支的唯一入口) 与 A7 (发码侧的形状)。
#
# 规则 (改坏了会红, 不是装饰):
#   A1  VB6_SA_AT 的宏定义恰好 1 处, 且宏体走 vb6_SaElemPtr (不许再退回裸算术)
#   A2  vb6_SaElemPtr 的 inline 定义恰好 1 处, 体内两条比较(下界/上界)与 NULL 那条都还在
#   A3  vb6_SaElemFail 声明 1 + 定义 1, 定义体里 vb6_ErrRaise(9 那条还在
#   A4  src/backend 里非注释的 `->data` 元素寻址行 = 0 (两支都不许再手算元素地址)
#   A5  src/rtl 与 src/backend 里 `->data))[` 这种"宏里手算下标"的形状 = 0
#   A6  多维那一支: AT1/AT2/AT3/ATN 四条宏各 1 处且宏体都经 vb6_SaNdElemPtr;
#       vb6_SaNdElemPtr 定义 1 处, 体内形状问句(dimCount)+两条上下界比较+冷路径调用都在;
#       vb6_SaNdElemFail 声明 1 + 定义 1, 定义体真的抛 9
#   A7  发码侧 4 秩以上那一条: 发 VB6_SA_ND_ATN, 下标实参按 `((const int32_t[]){` 拼(最外层括号
#       不是装饰, 少了它就是 cl C4002), 且不再手拼 `(int[]){` / 不再直接发 vb6_SafeArrayND_Offset(

# --- A8..A11（账 #305 §B154，第四十二刀）：一枚被 ReDim 重建的描述符「按哪一档分配」只许有一处答案 ---
# 旧形状 = cgen_redim.cpp 只问 ReDim 语句自己的 As 子句，没写 As 就回落 vb6_sa_variant；而**访问侧**
# （VB6_SA_AT / VB6_SA_ND_AT2）问的是声明表 arrayElemTypes_。同一枚描述符两个答案：造出来 24 字节
# 一步、写进去 8 字节一步，直接读写自己一致所以看着是好的，而凡信描述符自己那一档的消费方
# （Destroy / Erase / 装箱后的元素读）全走错步长 —— tests/test_redim_elem.bas 的 R3/R4 两枚 box-cell
# 在修前读回**空串**（两台同形，读数 .build/b1373_fix/{rnb6,rf6,rf3}.txt）。
#   A8  唯一出口 resolveReDimElemType：定义 1、声明 1、在 cgen_redim.cpp 被叫 1 次以上，函数体两头都
#       要问（As 子句 + arrayElemTypes_）；cgen_redim.cpp 里不许再出现「声明处只问 As」那一行。
#   A9  src/backend 非注释里 vb6_sa_variant 的字面量恰好 3 处：分档表的 Variant 档、它的 default 档、
#       以及 Variant 槽那一枚**刻意写死**的 Preserve（那一支的档是权威，不是猜）。第四处 = 又有人在
#       某个调用点手写兜底档。
#   A10 多维描述符投进 _Generic 装箱：装箱里那一层投影恰好 1 处；反向的「强转盖住装箱」恰好 0 处
#       （那是 BASE 上 cl C2440 的形状；裸递 ND 描述符则掉进 _Generic 的 default = vb6_VariantObject，
#        槽的 vt 成了 DISPATCH，Set/GetND 只认 vt & vtArray ⇒ 一声不响地既不写也不读）。
#   A11 声明表自己不许空转：写档站点 >= 8、.find 站点 >= 3、.erase 站点 == 1（过程开始清表那一条还在；
#       撤掉它，上一个过程的元素档会漏给下一个过程）。
# --- A12/A13（账 #305 §B155 格④，第四十三刀）：多维 Preserve 那枚出口自己也得收档 ---
# vb6_SafeArrayReDimPreserveND 的形参表里原本压根没有 elemType，于是 arr==NULL（首次分配就走 Preserve）
# 只能按 vb6_sa_empty 建 —— 尺寸表对 empty 答 default 4 字节，而访问侧按声明档走步长（x64：String 8、
# Variant 24），写出去就落在只有 1/2 到 1/6 大小的块外面。这一族是**响**的：探针 z.bas 修前实测
# 0xC0000374 与 0xC0000005 两种死法、另有一次侥幸跑完（读数 .build/b1382_preserve/ 与 .build/b1395_k43/），
# 而**直接读与直接写用的是同一枚错步长**，所以两者一致、看着是好的 —— 能分辨修前的只有「信描述符自己
# 那一档的消费方」（装箱后的元素读）那一条，判据就按那条钉。
#   A12 这一枚出口的头与体都收 elemType（各恰好 1；头追不上体在 runner 上是 cl C2197，见账 #240）；
#       体内首次分配那一条必须按调用方给的档建，vb6_sa_empty 那枚兜底不许再出现，udt 那一挡也还得在
#       （尺寸表对 udt 答 0 ⇒ calloc(n, 0) 给一枚非空的零长指针，写进去就是纯堆越界）。
#   A13 后端发这一枚的行恰好 1 处，且那一处把 saElemType 递了进去（0 处 = RTL 有形参而后端不递：C 里
#       实参不足只报 warning 照编，于是「运行时自己猜档」原地复活）。
#
# 用法:  pwsh -File scripts\check_sa_access.ps1
# 退出码: 0 = 全绿; 1 = 红

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$bad = @()

function Lines([string]$path) {
    ([System.IO.File]::ReadAllText($path) -split "`r?`n")
}

$rtlDir = Join-Path $root "src\rtl"
$beDir = Join-Path $root "src\backend"
$arrH = Join-Path $root "src\rtl\core\vb6rtl\vb6rtl_array.h"
$arrC = Join-Path $root "src\rtl\core\vb6rtl\vb6rtl_array.c"
foreach ($p in @($arrH, $arrC)) {
    if (-not (Test-Path -LiteralPath $p)) {
        Write-Host ("FAIL A0 file missing: " + $p) -ForegroundColor Red
        exit 1
    }
}

# A1: 宏只有定义点这一处写法, 而且必须经 vb6_SaElemPtr
$macroDefs = @()
$macroBodyCallsHelper = $false
foreach ($f in (Get-ChildItem -LiteralPath $rtlDir -Recurse -File | Where-Object { $_.Extension -in ".c", ".h" })) {
    $ln = 0
    foreach ($line in (Lines $f.FullName)) {
        $ln++
        if ($line.Trim() -match '^#\s*define\s+VB6_SA_AT\(') {
            $macroDefs += ($f.Name + ":" + $ln)
            $body = (Lines $f.FullName)[$ln..([Math]::Min($ln + 2, (Lines $f.FullName).Count - 1))] -join " "
            if ($body -match "vb6_SaElemPtr") { $macroBodyCallsHelper = $true }
        }
    }
}
if ($macroDefs.Count -ne 1) {
    $bad += ("A1 VB6_SA_AT macro definitions = " + $macroDefs.Count + " (want exactly 1) -> " + ($macroDefs -join " | "))
}
if ($macroDefs.Count -eq 1 -and -not $macroBodyCallsHelper) {
    $bad += "A1 VB6_SA_AT body no longer routes through vb6_SaElemPtr (the check moved back to raw math)"
}

# A2: 那个唯一的 inline, 三条检查一条不许哑
$h = [System.IO.File]::ReadAllText($arrH)
$defs = @([regex]::Matches($h, 'static\s+inline\s+void\*\s+vb6_SaElemPtr\s*\('))
if ($defs.Count -ne 1) {
    $bad += ("A2 vb6_SaElemPtr defined " + $defs.Count + " times (want 1)")
} else {
    $blk = [regex]::Match($h, 'static\s+inline\s+void\*\s+vb6_SaElemPtr[\s\S]{0,800}?\r?\n\}')
    if ($blk.Value -notmatch '!arr') { $bad += "A2 NULL-descriptor check is gone from the helper" }
    if ($blk.Value -notmatch 'idx\s*<\s*arr->lBound') { $bad += "A2 lower-bound compare is gone from the helper" }
    if ($blk.Value -notmatch 'idx\s*>\s*arr->uBound') { $bad += "A2 upper-bound compare is gone from the helper" }
    if ($blk.Value -notmatch 'vb6_SaElemFail') { $bad += "A2 the cold-path call is gone from the helper" }
}

# A3: 冷路径声明 + 定义各 1, 且真的抛 9
$decl = @([regex]::Matches($h, 'void\s+vb6_SaElemFail\s*\(')).Count
$csrc = [System.IO.File]::ReadAllText($arrC)
$defn = @([regex]::Matches($csrc, '\r?\nvoid\s+vb6_SaElemFail\s*\(')).Count
if ($decl -ne 1) { $bad += ("A3 vb6_SaElemFail 声明 = " + $decl + " (want 1)") }
if ($defn -ne 1) { $bad += ("A3 vb6_SaElemFail 定义 = " + $defn + " (want 1)") }
$cblk = [regex]::Match($csrc, 'void\s+vb6_SaElemFail[\s\S]{0,1200}?\r?\n\}')
if (-not $cblk.Success -or $cblk.Value -notmatch 'vb6_ErrRaise\(\s*9\b') {
    $bad += "A3 cold path no longer raises runtime error 9 (silent return would spare sites without On Error)"
}

# A4: 后端不再许有任何一处手算元素地址的行 (两支都收进出口了)
$rawData = @()
foreach ($f in (Get-ChildItem -LiteralPath $beDir -Recurse -File | Where-Object { $_.Extension -in ".cpp", ".inc", ".hpp" })) {
    $ln = 0
    foreach ($line in (Lines $f.FullName)) {
        $ln++
        $t = $line.Trim()
        if ($t.StartsWith("//")) { continue }
        if ($line.Contains('->data')) { $rawData += ($f.Name + ":" + $ln) }
    }
}
if ($rawData.Count -ne 0) {
    $bad += ("A4 后端手算元素地址的行 = " + $rawData.Count +
             " (want 0: 一维走 VB6_SA_AT, 多维走 VB6_SA_ND_AT*, 都不该再碰 ->data) -> " + ($rawData -join " | "))
}

# A5: "宏里手算下标"这个形状在整个仓里不许复活
$shape = 0
foreach ($dir in @($rtlDir, $beDir)) {
    foreach ($f in (Get-ChildItem -LiteralPath $dir -Recurse -File | Where-Object { $_.Extension -in ".c", ".h", ".cpp", ".inc", ".hpp" })) {
        foreach ($line in (Lines $f.FullName)) {
            if ($line.Trim().StartsWith("//")) { continue }
            if ($line.Contains('->data))[')) { $shape++ }
        }
    }
}
if ($shape -ne 0) {
    $bad += ("A5 裸 `->data))[` 元素寻址 = " + $shape + " 处 (want 0; 一维那支必须只有宏这一个入口)")
}

# A6: 多维那一支的入口与热路径问句
$ndMacros = @()
$ndViaHelper = 0
foreach ($f in (Get-ChildItem -LiteralPath $rtlDir -Recurse -File | Where-Object { $_.Extension -in ".c", ".h" })) {
    $fl = Lines $f.FullName
    $ln = 0
    foreach ($line in $fl) {
        $ln++
        if ($line -match '^\s*#\s*define\s+(VB6_SA_ND_AT\d|VB6_SA_ND_ATN)\b') {
            $ndMacros += ($Matches[1] + "@" + $f.Name + ":" + $ln)
            $hi = [Math]::Min($ln + 2, $fl.Count - 1)
            $body = $fl[$ln..$hi] -join " "
            if ($body -match "vb6_SaNdElemPtr") { $ndViaHelper++ }
        }
    }
}
if ($ndMacros.Count -ne 4) {
    $bad += ("A6 多维元素访问宏 = " + $ndMacros.Count + " 处 (want 4: AT1/AT2/AT3/ATN) -> " + ($ndMacros -join " | "))
}
if ($ndMacros.Count -eq 4 -and $ndViaHelper -ne 4) {
    $bad += ("A6 宏体经 vb6_SaNdElemPtr 的条数 = " + $ndViaHelper + " (want 4; 有一支退回裸算术了)")
}
$ndDefs = @([regex]::Matches($h, 'static\s+inline\s+void\*\s+vb6_SaNdElemPtr\s*\('))
if ($ndDefs.Count -ne 1) {
    $bad += ("A6 vb6_SaNdElemPtr defined " + $ndDefs.Count + " times (want 1)")
} else {
    $ndblk = [regex]::Match($h, 'static\s+inline\s+void\*\s+vb6_SaNdElemPtr[\s\S]{0,1200}?\r?\n\}')
    if ($ndblk.Value -notmatch '!a') { $bad += "A6 NULL-descriptor check is gone from the ND helper" }
    if ($ndblk.Value -notmatch 'dimCount') { $bad += "A6 the ND helper no longer asks the shape (rank vs dimCount)" }
    if ($ndblk.Value -notmatch 'idx\[d\]\s*<\s*lb') { $bad += "A6 lower-bound compare is gone from the ND helper" }
    if ($ndblk.Value -notmatch 'idx\[d\]\s*>=\s*lb\s*\+\s*cnt') { $bad += "A6 upper-bound compare is gone from the ND helper" }
    if ($ndblk.Value -notmatch 'vb6_SaNdElemFail') { $bad += "A6 the cold-path call is gone from the ND helper" }
}
$ndDecl = @([regex]::Matches($h, 'void\s+vb6_SaNdElemFail\s*\(')).Count
$ndDefn = @([regex]::Matches($csrc, '\r?\nvoid\s+vb6_SaNdElemFail\s*\(')).Count
if ($ndDecl -ne 1) { $bad += ("A6 vb6_SaNdElemFail 声明 = " + $ndDecl + " (want 1)") }
if ($ndDefn -ne 1) { $bad += ("A6 vb6_SaNdElemFail 定义 = " + $ndDefn + " (want 1)") }
$ndCblk = [regex]::Match($csrc, '\r?\nvoid\s+vb6_SaNdElemFail[\s\S]{0,1400}?\r?\n\}')
if (-not $ndCblk.Success -or $ndCblk.Value -notmatch 'vb6_ErrRaise\(\s*9\b') {
    $bad += "A6 ND cold path no longer raises runtime error 9 (silent return would spare sites without On Error)"
}

# A7: 发码侧 4 秩以上那一支的形状
$atnEmit = 0; $atnLiteral = 0; $handIdxList = 0; $offsetEmit = 0
foreach ($f in (Get-ChildItem -LiteralPath $beDir -Recurse -File | Where-Object { $_.Extension -in ".cpp", ".inc", ".hpp" })) {
    $ln = 0
    foreach ($line in (Lines $f.FullName)) {
        $ln++
        $t = $line.Trim()
        if ($t.StartsWith("//")) { continue }
        if ($t.Contains("VB6_SA_ND_ATN(")) { $atnEmit++ }
        if ($t.Contains('((const int32_t[])')) { $atnLiteral++ }
        if ($t.Contains('(int[]){')) { $handIdxList++ }
        if ($t.Contains('vb6_SafeArrayND_Offset(')) { $offsetEmit++ }
    }
}
if ($atnEmit -lt 1) { $bad += "A7 后端不再发 VB6_SA_ND_ATN (4 秩以上那支又回到手算偏移了)" }
if ($atnLiteral -ne 1) {
    $bad += ("A7 ATN 的下标实参不是按 ((const int32_t[]){ 拼的 = " + $atnLiteral +
             " 处 (want 1; 少最外层那对括号 ⇒ cl C4002 参数过多)")
}
if ($handIdxList -ne 0) {
    $bad += ("A7 手拼 (int[]){ 的下标列表又出现 " + $handIdxList +
             " 处 (那正是「只塞两枚下标却按实际秩数交出去」那个 bug 的形状)")
}
if ($offsetEmit -ne 0) {
    $bad += ("A7 后端直接发 vb6_SafeArrayND_Offset( 的行 = " + $offsetEmit +
             " (want 0: 元素访问只许走 AT* 那几个出口)")
}


# A8: ReDim 的元素档只问一处出口
$beFiles = @(Get-ChildItem -LiteralPath $beDir -Recurse -File | Where-Object { $_.Extension -in ".cpp", ".inc", ".hpp" })
$rdDef = 0; $rdDecl = 0; $rdCall = 0; $rdAsOnly = 0; $rdBodyBoth = 0
foreach ($f in $beFiles) {
    $txt = [System.IO.File]::ReadAllText($f.FullName)
    foreach ($line in (Lines $f.FullName)) {
        $t = $line.Trim()
        if ($t.StartsWith("//")) { continue }
        if ($t -match '^Vb6Type\s+CCodeGen::resolveReDimElemType\s*\(') { $rdDef++ }
        if ($f.Name -eq "cgen_helpers.inc" -and $t -match '^Vb6Type\s+resolveReDimElemType\s*\(') { $rdDecl++ }
        if ($f.Name -eq "cgen_redim.cpp") {
            if ($t.Contains("resolveReDimElemType(")) { $rdCall++ }
            if ($t -match '^Vb6Type\s+elemType\s*=\s*resolveArrayElemType\s*\(') { $rdAsOnly++ }
        }
    }
    if ($f.Name -eq "cgen_expr_array.cpp") {
        $blk = [regex]::Match($txt, 'Vb6Type\s+CCodeGen::resolveReDimElemType[\s\S]{0,900}?\r?\n\}')
        if ($blk.Value -match 'resolveArrayElemType\(\s*typeRef\s*\)' -and $blk.Value -match 'arrayElemTypes_\.find') { $rdBodyBoth++ }
    }
}
if ($rdDef -ne 1) { $bad += ("A8 resolveReDimElemType 定义 = " + $rdDef + " (要求 1；两处定义 = 第二份答案)") }
if ($rdDecl -ne 1) { $bad += ("A8 resolveReDimElemType 声明(cgen_helpers.inc) = " + $rdDecl + " (要求 1)") }
if ($rdCall -lt 1) { $bad += "A8 cgen_redim.cpp 不再调 resolveReDimElemType（ReDim 的元素档又回到只问 As 子句）" }
if ($rdAsOnly -ne 0) { $bad += ("A8 cgen_redim.cpp 里「声明处只问 As」的老形状 = " + $rdAsOnly + " 处 (要求 0；那正是本刀改掉的那一行)") }
if ($rdDef -eq 1 -and $rdBodyBoth -ne 1) { $bad += "A8 resolveReDimElemType 的体里没同时问 As 子句与 arrayElemTypes_（只剩一头 = 修前的形状）" }

# A9: 字面量兜底档的名单恰好三条
$saVariant = @()
foreach ($f in $beFiles) {
    $ln = 0
    foreach ($line in (Lines $f.FullName)) {
        $ln++
        $t = $line.Trim()
        if ($t.StartsWith("//")) { continue }
        if ($line.Contains("vb6_sa_variant")) { $saVariant += ($f.Name + ":" + $ln) }
    }
}
if ($saVariant.Count -ne 3) {
    $bad += ("A9 src/backend 非注释里的 vb6_sa_variant 字面量 = " + $saVariant.Count +
             " (恰好 3: 分档表 Variant 档 + default 档 + Variant 槽的 Preserve) -> " + ($saVariant -join " | "))
}
$slotKeep = 0
foreach ($s in $saVariant) { if ($s.StartsWith("cgen_redim.cpp")) { $slotKeep++ } }
if ($slotKeep -ne 1) { $bad += ("A9 Variant 槽那枚刻意写死的档不在 cgen_redim.cpp 里了 = " + $slotKeep + " (要求 1)") }

# A10: 多维描述符装箱的投影方向
$boxCast = 0; $castBox = 0
foreach ($f in $beFiles) {
    foreach ($line in (Lines $f.FullName)) {
        $t = $line.Trim()
        if ($t.StartsWith("//")) { continue }
        if ($line.Contains("vb6_VariantFromValue((vb6_SafeArray1D*)")) { $boxCast++ }
        if ($line.Contains("(vb6_SafeArray1D*)vb6_VariantFromValue")) { $castBox++ }
    }
}
if ($boxCast -ne 1) { $bad += ("A10 装箱里那一层投影 = " + $boxCast + " 处 (恰好 1；0 = 又把 ND 描述符裸递给 _Generic，它落 default 的 vb6_VariantObject)") }
if ($castBox -ne 0) { $bad += ("A10 强转盖住装箱的写法 = " + $castBox + " 处 (要求 0；BASE 上这一形实测 cl C2440)") }

# A11: 声明表的供给面与清理面
$tblWrite = 0; $tblFind = 0; $tblErase = 0
foreach ($f in $beFiles) {
    foreach ($line in (Lines $f.FullName)) {
        $t = $line.Trim()
        if ($t.StartsWith("//")) { continue }
        if ($line.Contains("arrayElemTypes_[")) { $tblWrite++ }
        if ($line.Contains("arrayElemTypes_.find")) { $tblFind++ }
        if ($line.Contains("arrayElemTypes_.erase")) { $tblErase++ }
    }
}
if ($tblWrite -lt 8) { $bad += ("A11 往 arrayElemTypes_ 写档的站点 = " + $tblWrite + " (下限 8；声明路被摘掉就没人给 ReDim 答档)") }
if ($tblFind -lt 3) { $bad += ("A11 问 arrayElemTypes_ 的站点 = " + $tblFind + " (下限 3：访问侧/类型汇总/ReDim 至少各一处)") }
if ($tblErase -ne 1) { $bad += ("A11 arrayElemTypes_.erase = " + $tblErase + " (恰好 1：过程开始的清表只许一处)") }


# A12: 多维 Preserve 这一枚自己收档 (账 #305 §B155 格④, 第四十三刀)
$ndPresDeclH = @([regex]::Matches($h, 'vb6_SafeArrayReDimPreserveND\s*\(\s*vb6_safearray_elemtype\s+elemType')).Count
$ndPresDefC = @([regex]::Matches($csrc, 'vb6_SafeArrayND\*\s+vb6_SafeArrayReDimPreserveND\s*\(\s*vb6_safearray_elemtype\s+elemType')).Count
if ($ndPresDeclH -ne 1) { $bad += ("A12 vb6_SafeArrayReDimPreserveND 的声明收 elemType = " + $ndPresDeclH + " 处 (恰好 1；0 = 形参表又回到没有档，NULL 描述符只能猜)") }
if ($ndPresDefC -ne 1) { $bad += ("A12 vb6_SafeArrayReDimPreserveND 的定义收 elemType = " + $ndPresDefC + " 处 (恰好 1；头/体个数不一致在 runner 上是 cl C2197)") }
$ndPresHead = [regex]::Match($csrc, 'vb6_SafeArrayND\*\s+vb6_SafeArrayReDimPreserveND\s*\(\s*vb6_safearray_elemtype[\s\S]{0,900}')
if ($ndPresHead.Value -notmatch 'vb6_SafeArrayReDimND\(\s*elemType') { $bad += "A12 首次分配那一条不再按调用方给的档建（回到 empty = 尺寸表 default 4 字节）" }
if ($ndPresHead.Value -match 'vb6_sa_empty') { $bad += "A12 多维 Preserve 里又出现 vb6_sa_empty 兜底（那正是本刀撤掉的那一行）" }
if ($ndPresHead.Value -notmatch 'elemType == vb6_sa_udt') { $bad += "A12 多维 Preserve 不再挡 udt 档（尺寸表对 udt 答 0 ⇒ calloc(n,0) 是非空零长指针）" }

# A13: 发码侧把档递进去
$ndPresEmit = 0; $ndPresTyped = 0
foreach ($f in $beFiles) {
    foreach ($line in (Lines $f.FullName)) {
        $t = $line.Trim()
        if ($t.StartsWith("//")) { continue }
        if ($t.Contains("vb6_SafeArrayReDimPreserveND(")) {
            $ndPresEmit++
            if ($t -match 'vb6_SafeArrayReDimPreserveND\("\s*\+\s*saElemType') { $ndPresTyped++ }
        }
    }
}
if ($ndPresEmit -ne 1) { $bad += ("A13 后端发 vb6_SafeArrayReDimPreserveND 的行 = " + $ndPresEmit + " (恰好 1)") }
if ($ndPresTyped -ne 1) { $bad += ("A13 那一行把 saElemType 递进去的写法 = " + $ndPresTyped + " 处 (恰好 1；0 = RTL 有形参而后端不递)") }
if ($bad.Count -eq 0) {
    Write-Host ("PASS SA element access: macro " + $macroDefs.Count +
                " / inline " + $defs.Count +
                " / cold-path decl " + $decl + " def " + $defn +
                " / backend raw-data lines " + $rawData.Count +
                " / raw-index shape " + $shape + " / ND macros " + $ndMacros.Count + " / ND inline " + $ndDefs.Count + " / ATN emit " + $atnEmit + " / ATN literal " + $atnLiteral + " / redim helper def " + $rdDef + " call " + $rdCall + " / sa_variant lit " + $saVariant.Count + " / box cast " + $boxCast + " / table w " + $tblWrite + " f " + $tblFind + " e " + $tblErase + " / ndPres decl " + $ndPresDeclH + " def " + $ndPresDefC + " emit " + $ndPresEmit + " typed " + $ndPresTyped) -ForegroundColor Green
    exit 0
}
foreach ($b in $bad) { Write-Host ("FAIL " + $b) -ForegroundColor Red }
exit 1
