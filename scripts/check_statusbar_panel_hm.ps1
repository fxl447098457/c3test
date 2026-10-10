# check_statusbar_panel_hm.ps1 —— 状态条面板宽那两处答案只许有一套（账 #206 §B41 第一格 / §B129）
#
# 立账的原委：`Panels(i).Width` 在**真 VB6 设计器**手里有两个名字、两种单位 ——
# 设计块里那一行是 `Object.Width`，值是 himetric(0.01mm)；VB 代码里读写的是缇。
# 本仓从前只认无前缀的 `Width`（`frm_parser.cpp:81` 保留点号全名 ⇒ 那一行永远查不到），
# 所以别人工程的面板几何整格丢掉：实测语料 5623 份 .frm 里 356 份 / 1634 处这么写，
# 而本仓三份状态条夹具用的都是自己造的点号形 ⇒ 全语料 A/B 对这一族永远沉默（零覆盖）。
# 这个哨兵钉住修法之后的形状，防的是：
#   ① 那条前缀认读被改掉（发码又看不见设计值了）；
#   ② himetric→像素被抄第二份（本族已有 13 行、5 份文件各写各的，见 §B129 —— 再加一份就是
#      第 14 行/第 6 份文件，正是 #234「拿 DC 抄两遍」、#235「画笔色两份存储」那个形状）；
#   ③ 弹簧档的 MinWidth 又变成"加项"（夹具实测 e3=493 而 cw=467，多出来正好一枚 MinWidth）。
#
# 判据面：
#   R1 产物：tests/sbhm/SbHm.vbp 的 --emit-c 里 —— SetPanelWidthHm 恰好 2 处（两枚固定档面板）、
#      实参字面量 `1500` / `2646` 各 1 处、AddPanel 恰好 3、SetPanelAutoSize 恰好 1（弹簧那枚不给
#      Width ⇒ 它必须**不**出现在 Hm 里）、且无前缀那枚 SetPanelWidth( 在产物里恰好 0 处
#      （设计期这一趟只走 Hm 出口；运行期赋值走的是另一张表，不归这条判据管）。
#   R2 源码：src/rtl 里含 2540 的**非注释行**必须逐文件对上名单（5 份 / 13 行，见下），
#      多出一份文件即红；且 vb6forms.c 那一行必须是按真实 DPI 的那枚权威 vb6_HimetricToPxX。
#   R3 源码：vb6forms_statusbar.c 里 `want[i] = e->minWidth` 这一形必须 0 处（下限只答一次），
#      而兜底那一行恰好 1 处。
#   R4 防空转：扫到的 RTL 份数 >= 100、产物字节数 >= 4000、两张名单自己的长度也钉死。
#   R5（账 #300 同轮加）：`sbFinishByKey` 三处的两个出口名不许相同，且新出口 `vb6_StatusBar_GetPanelIndex`
#      必须在 header / RTL 定义 / 发码点三处各出现恰好一次（少一处就是 LNK2019 或 C2065 那一族）。
#
# 出口只走 --emit-c（只到发码，不起 cl），所以这条判据是秒级的；负控用 -Root 把整棵树指到副本上跑。

param(
    [string]$Root = ''
)
$ErrorActionPreference = 'Stop'
if (-not $Root) { $Root = Split-Path -Parent $PSScriptRoot }

$viol = @()

# R2 名单：himetric 与 像素/缇 之间的折算在 RTL 里的现有住处（账 #226 同族：一份决定抄几遍）。
# 这份名单是**读数**不是愿望：2026-10-10 实测 5 份文件 / 13 个非注释行。
# 其中只有 vb6forms.c 那一枚按真实 DPI（vb6_HimetricToPxX），其余四处各按各自口径 ——
# §B129 要并表，但本刀不顺手改它们（各有各的判据面），所以这里把它们钉住防"再长一份"。
$HmFiles = [ordered]@{
    'src/rtl/core/vb6forms/vb6forms.c'                   = 1
    'src/rtl/core/vb6forms/vb6forms_olecon.c'            = 7
    'src/rtl/core/vb6forms/vb6forms_picture_prop.c'      = 3
    'src/rtl/core/vb6forms/axsite/ax_load.c'             = 1
    'src/rtl/core/vb6forms/axsite/ax_site_ext.c'         = 1
}
$HmWantTotal = 13
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
    if (-not $HmFiles.Contains($k)) { $viol += "R2-HM-NEW-SITE $k carries a 4th/5th himetric conversion (list has 5 files)" }
}
$sum = 0
foreach ($v in $perFile.Values) { $sum += $v }
if ($sum -ne $HmWantTotal) { $viol += "R2-HM-TOTAL want $HmWantTotal, got $sum" }

$authPath = Join-Path $rtlRoot 'core\vb6forms\vb6forms.c'
$authTxt = [System.IO.File]::ReadAllText($authPath)
$authDef = ([regex]::Matches($authTxt, [regex]::Escape('int vb6_HimetricToPxX(int hm) { return MulDiv(hm, vb6_DpiX(), 2540); }'))).Count
if ($authDef -ne 1) { $viol += "R2-AUTHORITY $HmAuthority definition want exactly 1 real-DPI line, got $authDef" }
$sbPath = Join-Path $rtlRoot 'core\vb6forms\vb6forms_statusbar.c'
$sbTxt = [System.IO.File]::ReadAllText($sbPath)
if (([regex]::Matches($sbTxt, [regex]::Escape('int px = vb6_HimetricToPxX(hm);'))).Count -ne 1) {
    $viol += 'R2-CALLER statusbar must call the authority exactly once (not re-do the math)'
}
$hdrPath = Join-Path $rtlRoot 'core\vb6forms\vb6forms_window.h'
if (([regex]::Matches(([System.IO.File]::ReadAllText($hdrPath)), 'vb6_HimetricToPxX')).Count -ne 1) {
    $viol += 'R2-DECL header must declare the authority exactly once'
}

$sbLines = ($sbTxt -replace "`r`n", "`n").Split("`n")
$seed = 0
$flo = 0
foreach ($ln in $sbLines) {
    if ($ln -match 'want\[i\]\s*=\s*e->minWidth') { $seed++ }
    if ($ln -match 'take\s*<\s*\(e->minWidth') { $flo++ }
}
if ($seed -ne 0) { $viol += "R3-SPRING-SEED want[i]=e->minWidth must be 0 (MinWidth is a floor, not an addend), got $seed" }
if ($flo -ne 1) { $viol += "R3-SPRING-FLOOR want exactly 1 floor line, got $flo" }

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

# ---------- R4: 防空转 ----------
if ($rtlFiles -lt 100) { $viol += "R4-RTL-FILES floor 100, got $rtlFiles" }
if ($HmFiles.Count -ne 5) { $viol += 'R4-LIST the allowlist itself changed size (must stay 5 files)' }
if ($HmWantTotal -ne 13) { $viol += 'R4-TOTAL the pinned total changed (must stay 13)' }
if ($pm.Count -lt 1) { $viol += 'R4-R5-CENSUS the pair scan found nothing = the sentinel is idling' }

if ($viol.Count -eq 0) {
    Write-Host ("PASS static_sentinel_statusbar_panel_hm: emit Hm=2 add=3 autosz=1 plain=0 / himetric sites " +
                "$sum in $($perFile.Count) files / rtl_files=$rtlFiles")
    exit 0
}
Write-Host 'FAIL statusbar_panel_hm:'
foreach ($v in $viol) { Write-Host "  - $v" }
exit 1
