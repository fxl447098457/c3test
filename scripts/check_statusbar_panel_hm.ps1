# check_statusbar_panel_hm.ps1 —— 状态条面板宽那两处答案只许有一套（账 #206 §B41 第一格 / §B129）
#
# 立账的原委：`Panels(i).Width` 在**真 VB6 设计器**手里有两个名字、两种单位 ——
# 设计块里那一行是 `Object.Width`，值是 himetric(0.01mm)；VB 代码里读写的是缇。
# 本仓从前只认无前缀的 `Width`（`frm_parser.cpp:81` 保留点号全名 ⇒ 那一行永远查不到），
# 所以别人工程的面板几何整格丢掉：实测语料 5623 份 .frm 里 356 份 / 1634 处这么写，
# 而本仓三份状态条夹具用的都是自己造的点号形 ⇒ 全语料 A/B 对这一族永远沉默（零覆盖）。
# 这个哨兵钉住修法之后的形状，防的是：
#   ① 那条前缀认读被改掉（发码又看不见设计值了）；
#   ② himetric 与 像素/缇 之间的折算被抄第二份（§B129 那份名单现存 4 份文件 / 14 个非注释行 ——
#      第二十二刀把 ax_load.c 那一处并进权威 vb6_TwipsToHimetric，名单**往下减**是本职；
#      再长一份就是 #234「拿 DC 抄两遍」、#235「画笔色两份存储」那个形状）；
#   ③ 弹簧档的 MinWidth 又变成"加项"（夹具实测 e3=493 而 cw=467，多出来正好一枚 MinWidth）。
#   ④ 面板几何那一档的单位回到两份答案（第十八刀把像素存进 e->width，而 VB 代码那一头读写的是
#      缇 ⇒ 同一格两种单位，正是 §B41 剩下的那一格）。第二十二刀之后的口径只有一套：
#      存储=缇、排版时经 vb6_TwipToX 折像素、getter 交排版结果再经 vb6_XToTwipX 折回缇。
#
# 判据面：
#   R1 产物：tests/sbhm/SbHm.vbp 的 --emit-c 里 —— SetPanelWidthHm 恰好 2 处（两枚固定档面板）、
#      实参字面量 `1500` / `2646` 各 1 处、AddPanel 恰好 3、SetPanelAutoSize 恰好 1（弹簧那枚不给
#      Width ⇒ 它必须**不**出现在 Hm 里）、且无前缀那枚 SetPanelWidth( 在产物里恰好 0 处
#      （设计期这一趟只走 Hm 出口；运行期赋值走的是另一张表，不归这条判据管）。
#      第二十二刀起再钉两枚新出口：SetPanelMinWidthHm 恰好 3（三枚面板都写了设计 MinWidth）、
#      GetPanelLeft 恰好 3（HM14 那三处读）—— 缺一个落点就是"设计值整格不见"/"读回空"。
#   R2 源码：src/rtl 里含 2540 的**非注释行**必须逐文件对上名单（4 份 / 14 行，见下），
#      多出一份文件即红；且 vb6forms.c 里那三枚必须是权威本人（像素档 vb6_HimetricToPxX +
#      缇档一对 vb6_TwipsToHimetric / vb6_HimetricToTwips）。状态条那一族只许**转调**权威：
#      本地一次乘除都不许出现（该文件里 MulDiv 0 处、HimetricToPxX 0 处 —— 后者是第十八刀
#      那一版"把像素存进缇档"的形状，本刀之后它必须归零）。
#   R3 源码：vb6forms_statusbar.c 里 `want[i] = e->minWidth` 这一形必须 0 处（下限只答一次），
#      而兜底那一行恰好 1 处、且它折像素问的也是 vb6_TwipToX。
#   R4 防空转：扫到的 RTL 份数 >= 100、产物字节数 >= 4000、两张名单自己的长度也钉死。
#   R5（账 #300 同轮加）：`sbFinishByKey` 三处的两个出口名不许相同，且新出口 `vb6_StatusBar_GetPanelIndex`
#      必须在 header / RTL 定义 / 发码点三处各出现恰好一次（少一处就是 LNK2019 或 C2065 那一族）。
#   R7（账 #206 第二格同轮加）：一枚成员要过四道登记才"真做得动"（memberobj 名字表 / DISPID /
#      派发 case / 发码 recognizer）+ RTL 定义 + header 声明。Panel.Left 这六格各恰好 1 处，
#      设计块那行 MinWidth 的三处同钉 —— **header 那一格是实测漏过的一格**：产物照跑、日志一声不报。
#
# 出口只走 --emit-c（只到发码，不起 cl），所以这条判据是秒级的；负控用 -Root 把整棵树指到副本上跑。

param(
    [string]$Root = ''
)
$ErrorActionPreference = 'Stop'
if (-not $Root) { $Root = Split-Path -Parent $PSScriptRoot }

$viol = @()

# R2 名单：himetric 与 像素/缇 之间的折算在 RTL 里的现有住处（账 #226 同族：一份决定抄几遍）。
# 这份名单是**读数**不是愿望：第二十二刀之后实测 4 份文件 / 14 个非注释行
# （axsite/ax_load.c 那一处已被并进来 —— 它现在转调 vb6_TwipsToHimetric，所以从名单上退掉；
#  vb6forms.c 因此从 1 行长成 3 行：像素档 1 枚 + 缇档 2 枚，三枚都是权威本人）。
# 其中按真实 DPI 的只有 vb6_HimetricToPxX 那一枚，其余各按各自口径 ——
# §B129 要并表，但本刀不动它们（各有各的判据面），所以这里把它们钉住防"再长一份"。
$HmFiles = [ordered]@{
    'src/rtl/core/vb6forms/vb6forms.c'                   = 3
    'src/rtl/core/vb6forms/vb6forms_olecon.c'            = 7
    'src/rtl/core/vb6forms/vb6forms_picture_prop.c'      = 3
    'src/rtl/core/vb6forms/axsite/ax_site_ext.c'         = 1
}
$HmWantTotal = 14
$HmAuthority = 'vb6_HimetricToPxX'

# ---------- R1: 产物形状 ----------
$C3 = Join-Path $Root '.build\C3.exe'
if (-not (Test-Path $C3)) {
    Write-Host "FAIL statusbar_panel_hm: C3.exe not found ($C3)"; exit 1
}
$proj = Join-Path $Root 'tests\sbhm\SbHm.vbp'
if (-not (Test-Path $proj)) {
    Write-Host "FAIL statusbar_panel_hm: fixture project missing ($proj)"; exit 1
}
$prev = $ErrorActionPreference
$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $C3
$psi.Arguments = '"' + $proj + '" --emit-c'
$psi.UseShellExecute = $false
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError = $true
$p = [System.Diagnostics.Process]::Start($psi)
$ms = New-Object System.IO.MemoryStream
$p.StandardOutput.BaseStream.CopyTo($ms)
$null = $p.StandardError.ReadToEnd()
$p.WaitForExit()
$ErrorActionPreference = $prev
$bytes = $ms.ToArray()
$emit = [System.Text.Encoding]::GetEncoding(28591).GetString($bytes)

function Count-Of([string]$needle) {
    if ($needle -eq '') { return 0 }
    return ([regex]::Matches($emit, [regex]::Escape($needle))).Count
}

$hm = Count-Of 'vb6_StatusBar_SetPanelWidthHm('
if ($hm -ne 2) { $viol += "R1-HM-CALL want exactly 2, got $hm" }
if ((Count-Of ', 1, 1500);') -ne 1) { $viol += 'R1-ARG-1500 want exactly 1 (panel1 Object.Width verbatim)' }
if ((Count-Of ', 2, 2646);') -ne 1) { $viol += 'R1-ARG-2646 want exactly 1 (panel2 Object.Width verbatim)' }
if ((Count-Of 'vb6_StatusBar_AddPanel(') -ne 3) { $viol += 'R1-ADD-PANEL want exactly 3' }
if ((Count-Of 'vb6_StatusBar_SetPanelAutoSize(') -ne 1) { $viol += 'R1-AUTOSIZE want exactly 1 (spring panel only)' }
if ((Count-Of 'vb6_StatusBar_SetPanelWidth((void*)') -ne 0) { $viol += 'R1-NO-PLAIN-SET want 0 (design path goes through the *Hm exit)' }
if ($bytes.Length -lt 4000) { $viol += ('R4-EMIT-BYTES floor 4000, got ' + $bytes.Length) }

# 账 #300 (§B131): Panels(<数字>).Index 与 Panels("键").Index 必须各走各的出口。
# 改前那一格发的是 *ByKey(hwnd, 1) —— 整数进 wchar_t* 槽, 真跑 0xC0000005。
if ((Count-Of 'vb6_StatusBar_GetPanelIndex((void*)') -ne 2) {
    $viol += 'R1-INDEX-BYIDX want exactly 2 (the two numeric subscripts, Panels(2)/Panels(9))'
}
if ((Count-Of 'vb6_StatusBar_GetPanelIndexByKey((void*)') -ne 1) {
    $viol += 'R1-INDEX-BYKEY want exactly 1 (the key subscript only, Panels("fx2"))'
}

# 第二十二刀新增的两枚发码点：设计块那行 MinWidth 走 *Hm 出口（夹具三枚面板都写了它，读数 3），
# 而 Panel.Left 现在有自己的出口（HM14 那三处读，读数 3）。这两条计数是"接线在产物里真落了地"的
# 存在性证人 —— 缺发码点 = 设计值整格不见（第十八刀那一格的形状），缺 recognizer = 读回空。
$hmMin = Count-Of 'vb6_StatusBar_SetPanelMinWidthHm('
if ($hmMin -ne 3) { $viol += ('R1-HM-MINWIDTH want exactly 3 (design block writes MinWidth on all three panels), got ' + $hmMin) }
$plEmit = Count-Of 'vb6_StatusBar_GetPanelLeft('
if ($plEmit -ne 3) { $viol += ('R1-LEFT-EMIT want exactly 3 (the three Panels(i).Left reads in HM14), got ' + $plEmit) }

# ---------- R2 + R3: 源码面 ----------
$rtlRoot = Join-Path $Root 'src\rtl'
$perFile = @{}
$rtlFiles = 0
Get-ChildItem -Path $rtlRoot -Recurse -File -Include *.c, *.h, *.inc | ForEach-Object {
    $rtlFiles++
    $txt = [System.IO.File]::ReadAllText($_.FullName)
    $n = 0
    foreach ($ln in ($txt -replace "`r`n", "`n").Split("`n")) {
        if ($ln.IndexOf('2540') -lt 0) { continue }
        $code = ($ln -replace '/\*.*?\*/', '') -replace '//.*$', ''
        if ($code.IndexOf('2540') -ge 0) { $n++ }
    }
    if ($n -gt 0) {
        $rel = ($_.FullName.Substring($Root.Length).Replace('\', '/').TrimStart('/'))
        $perFile[$rel] = $n
    }
}
foreach ($k in $HmFiles.Keys) {
    $want = $HmFiles[$k]
    $got = 0
    if ($perFile.ContainsKey($k)) { $got = $perFile[$k] }
    if ($got -ne $want) { $viol += "R2-HM-SITES $k want exactly $want, got $got" }
}
foreach ($k in $perFile.Keys) {
    if (-not $HmFiles.Contains($k)) { $viol += "R2-HM-NEW-SITE $k carries an extra himetric conversion (list has 4 files)" }
}
$sum = 0
foreach ($v in $perFile.Values) { $sum += $v }
if ($sum -ne $HmWantTotal) { $viol += "R2-HM-TOTAL want $HmWantTotal, got $sum" }

$authPath = Join-Path $rtlRoot 'core\vb6forms\vb6forms.c'
$authTxt = [System.IO.File]::ReadAllText($authPath)
$authDef = ([regex]::Matches($authTxt, [regex]::Escape('int vb6_HimetricToPxX(int hm) { return MulDiv(hm, vb6_DpiX(), 2540); }'))).Count
if ($authDef -ne 1) { $viol += "R2-AUTHORITY $HmAuthority definition want exactly 1 real-DPI line, got $authDef" }
# 缇档那一 pair 也必须是权威本人（第二十二刀新增：存储换缇之后，设计期那一档与 getter 那一档
# 都转调它们，本族不再自己乘除）。少了其中一枚 = 某一侧改回本地写法 = 第二份答案。
$twipDef = ([regex]::Matches($authTxt, [regex]::Escape('long vb6_TwipsToHimetric(long tw)'))).Count
$hm2twDef = ([regex]::Matches($authTxt, [regex]::Escape('long vb6_HimetricToTwips(long hm)'))).Count
if ($twipDef -ne 1) { $viol += "R2-AUTHORITY vb6_TwipsToHimetric definition want exactly 1, got $twipDef" }
if ($hm2twDef -ne 1) { $viol += "R2-AUTHORITY vb6_HimetricToTwips definition want exactly 1, got $hm2twDef" }
# axsite/ax_load.c 从名单上退掉的条件就是"它只转调" —— 这里钉住那一格，防它哪天又自己写一遍
$axTxt = [System.IO.File]::ReadAllText((Join-Path $rtlRoot 'core\vb6forms\axsite\ax_load.c'))
$axDeleg = ([regex]::Matches($axTxt, 'vb6_TwipsToHimetric\(')).Count
if ($axDeleg -ne 1) { $viol += ('R2-MERGED axsite/ax_load.c must delegate to the authority exactly once, got ' + $axDeleg) }

$sbPath = Join-Path $rtlRoot 'core\vb6forms\vb6forms_statusbar.c'
$sbTxt = [System.IO.File]::ReadAllText($sbPath)
# 状态条这一族问权威的三道折，各恰好 2 处（固定档 + 弹簧下限 / Width + Left / 两枚 *Hm 设计出口）。
# 注：这些数含注释里出现的次数吗？不含 —— 一律带左括号，写成 `(` 才算一次调用。
$sbTwip = ([regex]::Matches($sbTxt, 'vb6_TwipToX\(')).Count
$sbBack = ([regex]::Matches($sbTxt, 'vb6_XToTwipX\(')).Count
$sbHm = ([regex]::Matches($sbTxt, 'vb6_HimetricToTwips\(')).Count
if ($sbTwip -ne 2) { $viol += ('R2-CALLER statusbar must ask vb6_TwipToX exactly twice (fixed tile + spring floor), got ' + $sbTwip) }
if ($sbBack -ne 2) { $viol += ('R2-CALLER statusbar must ask vb6_XToTwipX exactly twice (Width + Left getter), got ' + $sbBack) }
if ($sbHm -ne 2) { $viol += ('R2-CALLER statusbar must ask vb6_HimetricToTwips exactly twice (the two *Hm design exits), got ' + $sbHm) }
# 本地一次乘除都不许出现；而第十八刀那一版"把像素直接存进缇档"的形状必须归零 —— 这条是它的墓碑。
$sbMul = 0
$sbPx = ([regex]::Matches($sbTxt, 'vb6_HimetricToPxX\(')).Count
foreach ($ln in (($sbTxt -replace "`r`n", "`n").Split("`n"))) {
    $code = ($ln -replace '/\*.*?\*/', '') -replace '//.*$', ''
    $sbMul += ([regex]::Matches($code, 'MulDiv')).Count
}
if ($sbMul -ne 0) { $viol += ('R2-LOCAL-MATH statusbar re-does the conversion ' + $sbMul + ' time(s) instead of asking the authority') }
if ($sbPx -ne 0) { $viol += ('R2-OLD-SHAPE vb6_HimetricToPxX( is called ' + $sbPx + ' time(s) in statusbar - 存储档已换成缇，那一版形状回来了') }
$hdrPath = Join-Path $rtlRoot 'core\vb6forms\vb6forms_window.h'
if (([regex]::Matches(([System.IO.File]::ReadAllText($hdrPath)), 'vb6_HimetricToPxX')).Count -ne 1) {
    $viol += 'R2-DECL header must declare the authority exactly once'
}

$sbLines = ($sbTxt -replace "`r`n", "`n").Split("`n")
$seed = 0
$flo = 0
$floAuth = 0
foreach ($ln in $sbLines) {
    if ($ln -match 'want\[i\]\s*=\s*e->minWidth') { $seed++ }
    if ($ln -match 'take\s*<\s*floorPx') { $flo++ }
    if ($ln.Contains('int floorPx = e->minWidth > 0 ? vb6_TwipToX(e->minWidth) : 0;')) { $floAuth++ }
}
if ($seed -ne 0) { $viol += "R3-SPRING-SEED want[i]=e->minWidth must be 0 (MinWidth is a floor, not an addend), got $seed" }
if ($flo -ne 1) { $viol += "R3-SPRING-FLOOR want exactly 1 floor line (take < floorPx), got $flo" }
if ($floAuth -ne 1) { $viol += ('R3-SPRING-FLOOR-AUTH the floor must fold its 缇 into pixels through vb6_TwipToX, exactly once, got ' + $floAuth) }

# ---------- R5: 数字/键两条出路不许同名（账 #300 的结构性面） ----------
# `sbFinishByKey($byIdx, $byKey)` 的两个参数一旦写成同一个名字，数字下标那一形就会把
# 整数递进 `const wchar_t*` 槽（#300 的真实形状：Index 两格都填 *ByKey ⇒ 启动期 AV，
# 而键下标那一形是对的 ⇒ 症状按"下标写数字还是键"分家，存量针永远看不见）。
$comPath = Join-Path $Root 'src\backend\cgen_util_com.cpp'
$comTxt = [System.IO.File]::ReadAllText($comPath)
$pm = [regex]::Matches($comTxt, 'sbFinishByKey\(\s*"([A-Za-z0-9_]+)"\s*,\s*"([A-Za-z0-9_]+)"')
if ($pm.Count -ne 3) { $viol += ('R5-PAIRS want exactly 3 sbFinishByKey call sites, got ' + $pm.Count) }
foreach ($m in $pm) {
    if ($m.Groups[1].Value -eq $m.Groups[2].Value) {
        $viol += ('R5-SAME-EXIT ' + $m.Groups[1].Value + ' answers both the numeric and the key form')
    }
}
# 新出口三处必须同步（header 声明 / RTL 定义 / 发码点）—— 少一处就是 LNK 或 C2065 那一族
$tri = @(
    (Join-Path $rtlRoot 'core\vb6forms\vb6forms_prop_ctrl.h'),
    (Join-Path $rtlRoot 'core\vb6forms\vb6forms_statusbar.c'),
    $comPath
)
foreach ($f in $tri) {
    $c = ([regex]::Matches([System.IO.File]::ReadAllText($f), 'vb6_StatusBar_GetPanelIndex\b')).Count
    if ($c -ne 1) { $viol += ('R5-TRIPLE ' + (Split-Path -Leaf $f) + ' must carry vb6_StatusBar_GetPanelIndex exactly 1 time, got ' + $c) }
}

# ---------- R6: 账 #299 (§B130) —— Panels 的集合对象只许 memberobj 那一头答 ----------
# 立这一格的原委（全部实测）：改前 `Set po =` / 模块级 `As Object` / `For Each` / `With`
# 四种"把成员当对象用"的写法，头一律发成 vb6_ComGetObjectProp(<HWND>, L"Panels")
# = 拿 HWND 当 IDispatch 用（C29-8b 当年给 TreeView Nodes 写下的同一句症状），
# `For Each` 连循环都没进 —— BASE 那台跑同一份夹具是 fe=0 idx=0 txt=0，
# 第二十刀之后是 fe=3 idx=6 txt=5。
# 第二十一刀把四条发射路（值语境 / 对象语境 / 默认成员下标的 binder）收成同一处出口，所以这里钉的是**读数**不是愿望。
$gaPath = Join-Path $Root 'src\backend\detail\expr\cgen_expr_member_generic_access.inc'
$stPath = Join-Path $Root 'src\backend\detail\util\cgen_state.inc'
$moPath = Join-Path $rtlRoot 'core\vb6forms\vb6forms_memberobj.c'
$hdPath = Join-Path $rtlRoot 'core\vb6forms\vb6forms_prop_ctrl.h'
$gaTxt = [System.IO.File]::ReadAllText($gaPath)
$stTxt = [System.IO.File]::ReadAllText($stPath)
$moTxt = [System.IO.File]::ReadAllText($moPath)
$hdTxt = [System.IO.File]::ReadAllText($hdPath)
# com_bind 那条默认成员下标的发射路（第二十一刀起也问同一处出口）
$cbTxt = [System.IO.File]::ReadAllText((Join-Path $Root 'src\backend\detail\expr\cgen_expr_call_com_bind.inc'))

# R6-A 产物：四形全部由 memberobj 那枚入口造出来（第二十一刀之后 Set / 模块级 Object /
# With / For Each 走的是同一处出口），而"拿 HWND 当 IDispatch"那一形必须归零 ——
# 改前三形各有 1 处、实参那一路还有 4 处（合计 8 处 stale head），这一条就是它的墓碑。
$coll = Count-Of 'vb6_StatusBar_Panels((void*)vb6_hwnd_StatusBar1)'
$collItem = Count-Of 'vb6_ComCallObject(vb6_StatusBar_Panels('
$collEnum = Count-Of 'vb6_ForEach_Init(vb6_StatusBar_Panels('
$stale = Count-Of 'vb6_ComGetObjectProp(vb6_hwnd_StatusBar1, L"Panels")'
if ($coll -ne 11) { $viol += ('R6-CREATOR-EMIT want exactly 11 (10 走 Item + 1 走枚举), got ' + $coll) }
if ($collItem -ne 10) { $viol += ('R6-ITEM-VIA-CREATOR want exactly 10, got ' + $collItem) }
if ($collEnum -ne 1) { $viol += ('R6-FOREACH-VIA-CREATOR want exactly 1, got ' + $collEnum) }
if ($stale -ne 0) {
    $viol += ('R6-OVERLAP ' + $stale + ' head(s) still use the HWND as an IDispatch - ' +
              '成员集合的创建式只许 memberCollectionObjectExpr 一处答')
}

# R6-B 认"这串表达式是集合对象"的三张名单必须同时认这一枚前缀。
# 少一处的后果是**实测过的**：只补拦子不补名单，`Panels.Count = 3` 发成
# `vb6_StatusBar_Panels((void*)vb6_hwnd_SB1).Count` ⇒ cl C2224（void* 上点成员）。
$gaN = ([regex]::Matches($gaTxt, 'find\("vb6_StatusBar_Panels\("\)')).Count
if ($gaN -ne 2) { $viol += ('R6-LIST generic_access must carry the prefix at both sites, got ' + $gaN) }
$stK = ([regex]::Matches($stTxt, '"vb6_StatusBar_Panels\(\(void\*\)vb6_"')).Count
if ($stK -ne 1) { $viol += ('R6-LIST statusBarNameOfExpr kHeads want exactly 1, got ' + $stK) }
$stC = ([regex]::Matches($stTxt, '"vb6_StatusBar_Panels\("')).Count
if ($stC -ne 1) { $viol += ('R6-LIST isControlCollectionExpr want exactly 1, got ' + $stC) }

# R6-C 出口本身只许有一处**定义**，并且三条发射路都问它（值语境 / 对象语境 / 默认成员下标）。
$cgN = ([regex]::Matches($comTxt, 'memberCollectionObjectExpr\(')).Count
$cbN = ([regex]::Matches($cbTxt, 'memberCollectionObjectExpr\(')).Count
$defN = ([regex]::Matches($stTxt, 'std::string memberCollectionObjectExpr\(')).Count
if ($defN -ne 1) { $viol += ('R6-EXIT definition must exist exactly once in cgen_state.inc, got ' + $defN) }
if ($cgN -ne 2) { $viol += ('R6-EXIT-CALLS cgen_util_com must ask the exit twice (value+object), got ' + $cgN) }
if ($cbN -ne 1) { $viol += ('R6-EXIT-CALLS com-bind indexer must ask the exit once, got ' + $cbN) }

# R6-D 一表到底只许一处写：**创建式字面量**在非注释行里只许出现在 cgen_state.inc。
# 这六枚是仓里"成员集合由谁造"这张表的全部行；再有一处在别的文件里出现，就是第二份答案
# (#234 拿 DC 抄两遍、#235 画笔色两份存储、#229/#88 同一族的复发)。
$creatorLits = @(
    'vb6_TreeView_Nodes((void*)', 'vb6_Toolbar_Buttons((void*)', 'vb6_StatusBar_Panels((void*)',
    'vb6_ImageList_ListImages((void*)', 'vb6_ListView_ListItems((void*)', 'vb6_ListView_ColumnHeaders((void*)')
$creatorWant = @{
    'vb6_TreeView_Nodes((void*)' = 2; 'vb6_Toolbar_Buttons((void*)' = 1
    'vb6_StatusBar_Panels((void*)' = 2; 'vb6_ImageList_ListImages((void*)' = 2
    'vb6_ListView_ListItems((void*)' = 2; 'vb6_ListView_ColumnHeaders((void*)' = 2 }
$creatorFiles = @{}
$creatorTot = @{}
foreach ($L in $creatorLits) { $creatorTot[$L] = 0 }
Get-ChildItem -Path (Join-Path $Root 'src\backend') -Recurse -File -Include *.cpp, *.inc, *.h | ForEach-Object {
    $rel = ($_.FullName.Substring($Root.Length).Replace('\', '/').TrimStart('/'))
    foreach ($ln in ([System.IO.File]::ReadAllText($_.FullName) -replace "`r`n", "`n").Split("`n")) {
        $s = $ln.Trim()
        if ($s.StartsWith('//')) { continue }
        foreach ($L in $creatorLits) {
            $n = ([regex]::Matches($s, [regex]::Escape($L))).Count
            if ($n -gt 0) {
                $creatorTot[$L] += $n
                if (-not $creatorFiles.ContainsKey($L)) { $creatorFiles[$L] = @{} }
                $creatorFiles[$L][$rel] = 1
            }
        }
    }
}
foreach ($L in $creatorLits) {
    $got = $creatorTot[$L]
    $want = $creatorWant[$L]
    if ($got -ne $want) { $viol += ("R6-ONE-MAP $L code-line count want exactly $want, got $got") }
    foreach ($f in $creatorFiles[$L].Keys) {
        if ($f -ne 'src/backend/detail/util/cgen_state.inc') {
            $viol += ("R6-ONE-MAP-SITE $L is also written in $f - the table must live in one place")
        }
    }
}

# R6-E RTL 那一头：入口定义 / 声明 / Item 按 Key 那一档，各恰好一次
if (([regex]::Matches($moTxt, 'vb6_StatusBar_Panels\(void\* hwnd\)')).Count -ne 1) {
    $viol += 'R6-CREATOR vb6forms_memberobj.c must define the collection entry exactly once'
}
if (([regex]::Matches($hdTxt, 'vb6_StatusBar_Panels\(void\* hwnd\)')).Count -ne 1) {
    $viol += 'R6-DECL vb6forms_prop_ctrl.h must declare it exactly once'
}
if (([regex]::Matches($moTxt, 'VB6_MEMCK_PANELS\)\s+idx = vb6_StatusBar_GetPanelIndexByKey')).Count -ne 1) {
    $viol += 'R6-ITEM-BY-KEY memColl Item must answer the PANELS key form (else Panels("k") = Nothing)'
}
# #300 那格留下的第三份答案不许回到面板这一族：inline `p->index` 全仓现存 6 处（别的族），
# 面板那一处已改成问 getter。这个数只许降不许升。
$inline = ([regex]::Matches($moTxt, 'memSetI4\(out, p->index\);')).Count
if ($inline -gt 6) {
    $viol += ('R6-THIRD-ANSWER inline p->index answers grew beyond the 6 that exist: ' + $inline)
}
if (([regex]::Matches($moTxt, 'memSetI4\(out, vb6_StatusBar_GetPanelIndex\(p->owner, p->index\)\);')).Count -ne 1) {
    $viol += 'R6-INDEX-AUTHORITY Panel.Index must ask the one getter exactly once'
}

# ---------- R7: 账 #206 第二格 —— 新登记的 Panel.Left 与 *Hm 设计出口，落点一处不许缺 ----------
# 一枚成员从"名字认得、读数交空"到"真做得动"要过四道登记（memberobj 名字表 / DISPID / 派发
# case / 发码 recognizer），加 RTL 定义与 header 声明两格 —— 缺哪一格的症状都不一样，而
# **实测过的最阴那一格是 header**：第二十二刀发码点与定义都写齐了、声明漏了，产物照跑
# （调用返回 void、实参按 ABI 到位），C3 的构建日志里根本没有 cl 警告这一族 ⇒ 只有数落点能发现。
# 账 #174 就是同一形状（SelectedControls 发成隐式声明）。
$plNames = ([regex]::Matches($moTxt, 'L"Left"')).Count
$plDisp = ([regex]::Matches($moTxt, '#define VB6_MEMD_PANEL_LEFT 9')).Count
$plCase = ([regex]::Matches($moTxt, 'case VB6_MEMD_PANEL_LEFT:')).Count
$plMoCall = ([regex]::Matches($moTxt, 'vb6_StatusBar_GetPanelLeft\(p->owner, p->index\)')).Count
$plDef = ([regex]::Matches($sbTxt, 'int32_t vb6_StatusBar_GetPanelLeft\(void\* hwnd, int32_t index\)')).Count
$plDecl = ([regex]::Matches($hdTxt, 'vb6_StatusBar_GetPanelLeft\(void\* hwnd, int32_t index\);')).Count
$plRecog = ([regex]::Matches($comTxt, 'sbGet\("vb6_StatusBar_GetPanelLeft"\)')).Count
if ($plNames -ne 1) { $viol += ('R7-NAMES kPanelNames must register L"Left" exactly once, got ' + $plNames) }
if ($plDisp -ne 1) { $viol += ('R7-DISPID VB6_MEMD_PANEL_LEFT must be defined exactly once, got ' + $plDisp) }
if ($plCase -ne 1) { $viol += ('R7-DISPATCH memInvokePanel must answer VB6_MEMD_PANEL_LEFT exactly once, got ' + $plCase) }
if ($plMoCall -ne 1) { $viol += ('R7-MO-CALL memberobj must call the Left getter exactly once, got ' + $plMoCall) }
if ($plDef -ne 1) { $viol += ('R7-DEF vb6forms_statusbar.c must define vb6_StatusBar_GetPanelLeft exactly once, got ' + $plDef) }
if ($plDecl -ne 1) { $viol += ('R7-DECL vb6forms_prop_ctrl.h must declare vb6_StatusBar_GetPanelLeft exactly once, got ' + $plDecl) }
if ($plRecog -ne 1) { $viol += ('R7-RECOGNIZER cgen_util_com must map Panel.Left to that exit exactly once, got ' + $plRecog) }
# 设计块那行 MinWidth 的三处（发码点 / RTL 定义 / header 声明）—— 同上，缺第三处是静默的
$saTxt = [System.IO.File]::ReadAllText((Join-Path $Root 'src\backend\detail\module\cgen_form_ctrl_style_apply.inc'))
$minEmit = ([regex]::Matches($saTxt, 'vb6_StatusBar_SetPanelMinWidthHm\(\(void\*\)"')).Count
$minDef = ([regex]::Matches($sbTxt, 'void vb6_StatusBar_SetPanelMinWidthHm\(void\* hwnd, int32_t index, int32_t hm\)')).Count
$minDecl = ([regex]::Matches($hdTxt, 'vb6_StatusBar_SetPanelMinWidthHm\(void\* hwnd, int32_t index, int32_t hm\);')).Count
if ($minEmit -ne 1) { $viol += ('R7-MINW-EMIT the design block must emit SetPanelMinWidthHm exactly once, got ' + $minEmit) }
if ($minDef -ne 1) { $viol += ('R7-MINW-DEF RTL must define SetPanelMinWidthHm exactly once, got ' + $minDef) }
if ($minDecl -ne 1) { $viol += ('R7-MINW-DECL header must declare SetPanelMinWidthHm exactly once, got ' + $minDecl) }
# 单位这一格用代码形状钉（不靠注释）：排版那一趟必须把 e->width **折**一次才递给 SB_SETPARTS
# —— 这一行存在就说明存储档不是像素（像素档是第十八刀那一版，它这里直接用 e->width）。
$unitFold = ([regex]::Matches($sbTxt, 'want\[i\] = e->width > 0 \? vb6_TwipToX\(e->width\)')).Count
if ($unitFold -ne 1) { $viol += ('R7-UNIT the layout must fold e->width through vb6_TwipToX exactly once (storage is not pixels), got ' + $unitFold) }

# ---------- R8: 账 #303 (§B132) —— 要占位的那两档各从剩余空间里扣一次，不多不少 ----------
# 病名（第二十二刀把 getter 改成交排版结果才露面的存量）：`SbLayout` 第一遍把内容档的实测宽写进
# want[i] 却**没累加进 fixedTotal** ⇒ 弹簧把同一格宽吃两次 ⇒ 末格右边界越过客户区
# （夹具实测 sum=6630 而 bar=6000，差的 630 缇 = 42 像素 = 那枚 sbrTime 面板的文本宽）。
# 钉成"恰好 2 次"的两个方向都是真形状：0 次 = 本账；3 次（把弹簧档也扣上）= 第十八刀撤掉的
# 那个"MinWidth 当加项"复发。判据面 = tests/ctrlstatusbar 的 SB36-TILES-BAR（BASE 上 False）。
$tileAcc = ([regex]::Matches($sbTxt, 'fixedTotal \+= want\[i\];')).Count
if ($tileAcc -ne 2) { $viol += ('R8-TILE-RESERVE want exactly 2 reserved tiles (fixed + contents), got ' + $tileAcc) }

$oneMapTot = 0
foreach ($L in $creatorLits) { $oneMapTot += $creatorTot[$L] }
if ($oneMapTot -lt 11) { $viol += ("R4-R6-ONEMAP the one-map census found $oneMapTot sites, floor 11 = it is idling") }

# ---------- R4: 防空转 ----------
if ($rtlFiles -lt 100) { $viol += "R4-RTL-FILES floor 100, got $rtlFiles" }
if ($HmFiles.Count -ne 4) { $viol += 'R4-LIST the allowlist itself changed size (must stay 4 files)' }
if ($HmWantTotal -ne 14) { $viol += 'R4-TOTAL the pinned total changed (must stay 14)' }
if ($pm.Count -lt 1) { $viol += 'R4-R5-CENSUS the pair scan found nothing = the sentinel is idling' }
if (($gaN + $stK + $stC + $cgN) -lt 5) { $viol += 'R4-R6-CENSUS the panels-list scan found nothing = the sentinel is idling' }
if (($coll + $collItem + $collEnum + $defN + $cbN) -lt 15) { $viol += 'R4-R6-CENSUS2 the panels emit/exit scan found nothing = the sentinel is idling' }
if (($plNames + $plDisp + $plCase + $plMoCall + $plDef + $plDecl + $plRecog + $minEmit + $minDef + $minDecl + $unitFold) -lt 6) {
    $viol += 'R4-R7-CENSUS the Left/design-MinWidth registration scan found nothing = the sentinel is idling'
}
if (($sbTwip + $sbBack + $sbHm + $axDeleg + $twipDef + $hm2twDef) -lt 3) {
    $viol += 'R4-R2-CENSUS the authority-call scan found nothing = the sentinel is idling'
}

if ($viol.Count -eq 0) {
    Write-Host ("PASS static_sentinel_statusbar_panel_hm: emit Hm=" + $hm + "/" + $hmMin +
                " add=3 autosz=1 plain=0 left=$plEmit / himetric sites $sum in $($perFile.Count) files" +
                " (statusbar asks $sbTwip+$sbBack+$sbHm, local math $sbMul, old shape $sbPx, tiles reserved $tileAcc)" +
                " / rtl_files=$rtlFiles / panels coll=$coll item=$collItem " +
                "enum=$collEnum stale=$stale exit=$defN+$cgN+$cbN lists=$gaN+$stK+$stC " +
                "onemap=$oneMapTot inline=$inline")
    exit 0
}
Write-Host 'FAIL statusbar_panel_hm:'
foreach ($v in $viol) { Write-Host "  - $v" }
exit 1
