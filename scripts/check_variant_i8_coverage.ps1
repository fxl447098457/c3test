# check_variant_i8_coverage.ps1 - 「指针宽度那一档属于 Long 同族」这条决定的覆盖面哨兵 (账 #305 / §B138 尾巴)
#
# 为什么单开这一道：同一个问句（「这枚 Variant 装的算不算数值 / 该报什么类型名 / 打包成什么档」）在
# src\rtl 里以 `switch (v.vt)` 的形状实现了**十一遍**。§B138 把 VarPtr/StrPtr/ObjPtr 的类型答案换成
# LongPtr 之后，x64 第一次真的往这些表里喂 VT_I8 —— 于是四处当场答错：vb6_TypeName 落 default 报
# "Variant"、vb6_IsNumeric 报假、vb6_ComPackVariant 落 default 把地址**静默打包成 VT_EMPTY**、
# UC 宿主模型两枚取数读回 0（后两处按不同读数只算一处）。补 case 只是止血；这一道钉的是**闭合 census**：
# 以后任何人新写一张 vt 表，要么进 must 名单并答这一档，要么进 exempt 并写明理由，否则门红。
#
# 规则（改坏了会红，不是装饰）：
#   V1  src\rtl 里发现的 vt switch 站点数 >= 13 —— 突然变少或数出 0 = glob/正则坏了，那比红更糟
#   V2  闭合性：每一处站点的宿主函数必须**要么在 must、要么在 exempt**；两边都没有 ⇒ 红
#   V3  must 名单里每个函数体必须真有 VT_I8 档（挂了名字没写档 = 静默漏答，正是本账的实物）
#   V4  名单（must + exempt + decMust + decExempt）里每个名字都必须在 src\rtl 里真的存在（拼错的名字会让规则断电）
#   V5  must 名单不许空着或缩水到 < 11；decMust 不许缩水到 < 7
#   D1  decMust 里每个函数体必须真有 VT_DECIMAL 档 —— 与 V3 同形，只是问的是另一档
#   D2  闭合性按**档**算：每一处站点的宿主函数在 Decimal 这一档也必须二选一（decMust 或 decExempt）
#   D3  decExempt 的每条理由不许是空话（少于 12 字 ⇒ 红），豁免必须说清「为什么不答这一档」
#
# 口径从「一档 × 每表」换成「每档 × 每表」的来历（账 #305 §B143）：VT_I8 那一撞之后 Decimal 是第二次，
# 而两次都只钉了一档 —— 补了 I8 的表照样可以漏 Decimal。现在同一张表要答两档，名单各列各的。
# ⚠ 站点正则今天只认接收者叫 `v` 的写法（可带强转）。**换成别的变量名就看不见**，实测这类还有 7 处：
#   ax_propbag.c(p->vt) / uc_hostmodel.c(in->vt ×2) / vb6rtl_com.c(varElem.vt, pv->vt ×2) /
#   vb6rtl_paramarray.c(retVal.vt) —— 那 2 处 pv->vt 正是 §B143 下一格的 x86 COM 导入两臂。
#   把它们收进来之前别把 V1 的下限调高，否则会红在一堆没归因的站点上。
#
# 用法:  powershell -File scripts\check_variant_i8_coverage.ps1
# 退出码: 0 = 全绿; 1 = 红

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$rtl  = Join-Path $root "src\rtl"
if (-not (Test-Path -LiteralPath $rtl)) {
    Write-Host ("FAIL rtl dir missing: " + $rtl) -ForegroundColor Red
    exit 1
}
$bad = @()

$must = @(
    "vb6_VariantToLong", "vb6_VariantToLongPtr", "vb6_VariantToBool", "vb6_VariantToDouble",
    "vb6_TypeName", "vb6_IsNumeric", "vb6_ComPackVariant", "memVariantToI4",
    "vb6_ho_variantToLong", "vb6_ho_variantToDouble", "vb6_VarToDouble_internal"
)
$exempt = @{
    "vb6_CDec" = "两架构读数本来就相同（CStr(CDec(整数)) 今天就是空串），缺的不是这一档 -> 另立 §B141"
    "vb6_ResIdOf" = "资源 id 的实参解包：只认 BSTR 与数值档，而 default 交 lVal(不是 0)，与本账「漏档静默交 0」不同形；Decimal 档全仓 0 个调用点"
}

# Decimal 这一档（§B143）：同一张表要答两档，所以名单也列两份。
$decMust = @(
    "vb6_VariantToLong", "vb6_VariantToLongPtr", "vb6_VariantToBool", "vb6_VariantToDouble",
    "vb6_TypeName", "vb6_IsNumeric", "vb6_VarToDouble_internal"
)
$decExempt = @{
    "vb6_ComPackVariant" = "往 COM 交参数，Decimal 那一档要按 x86 内联/x64 指针两套写法走，问的不是「VB 侧怎么取数」"
    "memVariantToI4" = "UC 宿主模型的取数口，全仓 0 个 Decimal 调用点（§B143 量过），要补先证明可达"
    "vb6_ho_variantToLong" = "UC 宿主模型的取数口，全仓 0 个 Decimal 调用点（§B143 量过），要补先证明可达"
    "vb6_ho_variantToDouble" = "UC 宿主模型的取数口，全仓 0 个 Decimal 调用点（§B143 量过），要补先证明可达"
    "vb6_CDec" = "它是造 Decimal 的那一档，不是读表；补它等于把同一个问题抄第二遍"
    "vb6_ResIdOf" = "资源 id 的实参解包：LoadRes*(CDec(...)) 全仓 0 处，而 default 交的是 lVal 不是 0"
}

$fnRe     = [regex]'^[A-Za-z_][A-Za-z0-9_ \*]*?([A-Za-z_][A-Za-z0-9_]*)\s*\([^;]*\)\s*\{\s*$'
# 接收者叫 v 的 vt switch，**允许带强转**：`switch ((vb6_vartype)v->vt)` 与 `switch (v.vt)` 是同一族表。
# 以前这条正则不认强转写法，于是「比较走哪个数值」那一口(vb6_VarToDouble_internal)整个不在 census 里。
$switchRe = [regex]'switch\s*\(\s*(?:\([^)]*\)\s*)?(?:v\.vt|v->vt)\s*\)'
# 档的写法有两种拼法：SDK 名与 vb6_ 前缀名，且可能带强转 ⇒ 按同一个模式认。
$armRe = @{
    I8  = [regex]'case\s+(?:\([^)]*\)\s*)?(?:VT_I8|vb6_vtI8)\s*:'
    DEC = [regex]'case\s+(?:\([^)]*\)\s*)?(?:VT_DECIMAL|vb6_vtDecimal)\s*:'
}

# ---------- 扫一遍：站点 + 每个函数的体 ----------
$files = @(Get-ChildItem -LiteralPath $rtl -Recurse -File -Filter *.c)
$sites = @()
$bodies = @{}
$allNames = @($must + $decMust + @($exempt.Keys) + @($decExempt.Keys)) | Select-Object -Unique
foreach ($f in $files) {
    $text  = [System.IO.File]::ReadAllText($f.FullName)
    $lines = $text -split "`r?`n"
    $cur   = ""
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $m = $fnRe.Match($lines[$i])
        if ($m.Success) { $cur = $m.Groups[1].Value }
        if ($switchRe.IsMatch($lines[$i])) {
            $sites += [pscustomobject]@{ File = $f.FullName.Substring($root.Length + 1)
                                         Line = $i + 1; Fn = $cur }
        }
    }
    foreach ($name in ($allNames)) {
        if ($bodies.ContainsKey($name)) { continue }
        $sig = [regex]::Match($text, '(?m)^[\w \*]*' + [regex]::Escape($name) + '\s*\([^\r\n;]*\)\s*\{')
        if (-not $sig.Success) { continue }
        $depth = 0; $from = $sig.Index + $sig.Value.Length - 1; $to = -1
        for ($k = $from; $k -lt $text.Length; $k++) {
            $c = $text[$k]
            if ($c -eq '{') { $depth++ }
            elseif ($c -eq '}') { $depth--; if ($depth -eq 0) { $to = $k; break } }
        }
        if ($to -gt $from) { $bodies[$name] = $text.Substring($from, $to - $from + 1) }
    }
}

# ---------- V1: 覆盖面 ----------
if ($sites.Count -lt 13) {
    $bad += ("V1 only " + $sites.Count + " vt switches found under src\rtl (floor 13) -> the glob or the " +
             "regex broke; the closure checks below would be checking almost nothing")
}

# ---------- 每一档各问一遍：闭合 census + 名单自己 ----------
$arms = @(
    [pscustomobject]@{ Tag = "V2/V3"; Arm = "VT_I8";      Must = $must;    Exempt = $exempt;    Floor = 11; Re = $armRe.I8 },
    [pscustomobject]@{ Tag = "D1/D2"; Arm = "VT_DECIMAL"; Must = $decMust; Exempt = $decExempt; Floor = 7;  Re = $armRe.DEC }
)
foreach ($a in $arms) {
    # 闭合性：每张 vt 表都要在这一档二选一(答它 / 写明为什么不答)
    foreach ($s in $sites) {
        if ($s.Fn -eq "") {
            $bad += ($a.Tag + " " + $s.File + ":" + $s.Line + " switch could not be attributed to a function " +
                     "-> the census can't tell whether it answers the " + $a.Arm + " 档; check the signature shape")
            continue
        }
        if (($a.Must -notcontains $s.Fn) -and (-not $a.Exempt.ContainsKey($s.Fn))) {
            $bad += ($a.Tag + " " + $s.File + ":" + $s.Line + " in " + $s.Fn + ": a vt switch that is in " +
                     "neither list for the " + $a.Arm + " 档 -> it drops that 档 into default. Either add it " +
                     "to the must list AND write the case, or to the exempt list with the reason it doesn't need one")
        }
    }
    # must 名单：名字要在，档也要在
    foreach ($name in $a.Must) {
        if (-not $bodies.ContainsKey($name)) {
            $bad += ("V4 must-list entry " + $name + " has no function body in src\rtl -> renamed or deleted; " +
                     "this rule is now dead power")
            continue
        }
        if (-not $a.Re.IsMatch($bodies[$name])) {
            $bad += ($a.Tag + " " + $name + ": in the must-list for the " + $a.Arm + " 档 but its body has no " +
                     "case for it -> this table silently answers that 档 through default")
        }
    }
    # exempt 名单：名字还要在(删了函数就撤豁免)，理由还要写得住
    foreach ($name in @($a.Exempt.Keys)) {
        if (-not $bodies.ContainsKey($name)) {
            $bad += ("V4 exempt entry " + $name + " is not a function in src\rtl anymore -> drop the exemption " +
                     "together with the function")
        }
        if (([string]$a.Exempt[$name]).Length -lt 12) {
            $bad += ("D3 exempt entry " + $name + " (" + $a.Arm + " 档) has a reason shorter than 12 chars -> an " +
                     "empty reason turns the exemption into a silent hole in the census")
        }
    }
    # 针面非空
    if ($a.Must.Count -lt $a.Floor) {
        $bad += ("V5 " + $a.Arm + " must-list holds " + $a.Must.Count + " names (floor " + $a.Floor +
                 ") -> the needle surface collapsed; if these tables really merged into one authority, " +
                 "delete this sentinel together with them")
    }
}

if ($bad.Count -eq 0) {
    Write-Host ("PASS variant_i8_coverage: switches " + $sites.Count + " I8 must " + $must.Count +
                " exempt " + $exempt.Count + " | DEC must " + $decMust.Count + " exempt " +
                $decExempt.Count + " | violations 0") -ForegroundColor Green
    exit 0
}
foreach ($b in $bad) { Write-Host ("FAIL " + $b) -ForegroundColor Red }
exit 1
