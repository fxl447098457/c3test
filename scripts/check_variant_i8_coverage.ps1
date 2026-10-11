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
#   V1  src\rtl 里发现的 vt switch 站点数 >= 11 —— 突然变少或数出 0 = glob/正则坏了，那比红更糟
#   V2  闭合性：每一处站点的宿主函数必须**要么在 must、要么在 exempt**；两边都没有 ⇒ 红
#   V3  must 名单里每个函数体必须真有 `case VT_I8:`（挂了名字没写档 = 静默漏答，正是本账的实物）
#   V4  名单（must + exempt）里每个名字都必须在 src\rtl 里真的存在（拼错的名字会让规则断电）
#   V5  must 名单不许空着或缩水到 < 10（针面哪天全没了，这道哨兵等于没电）
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
    "vb6_ho_variantToLong", "vb6_ho_variantToDouble"
)
$exempt = @{
    "vb6_CDec" = "两架构读数本来就相同（CStr(CDec(整数)) 今天就是空串），缺的不是这一档 -> 另立 §B141"
}

$fnRe     = [regex]'^[A-Za-z_][A-Za-z0-9_ \*]*?([A-Za-z_][A-Za-z0-9_]*)\s*\([^;]*\)\s*\{\s*$'
$switchRe = [regex]'switch\s*\(\s*(?:v\.vt|v->vt)\s*\)'

# ---------- 扫一遍：站点 + 每个函数的体 ----------
$files = @(Get-ChildItem -LiteralPath $rtl -Recurse -File -Filter *.c)
$sites = @()
$bodies = @{}
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
    foreach ($name in ($must + @($exempt.Keys))) {
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
if ($sites.Count -lt 11) {
    $bad += ("V1 only " + $sites.Count + " vt switches found under src\rtl (floor 11) -> the glob or the " +
             "regex broke; V2/V3 below would be checking almost nothing")
}

# ---------- V2: 闭合 census ----------
foreach ($s in $sites) {
    if ($s.Fn -eq "") {
        $bad += ("V2 " + $s.File + ":" + $s.Line + " switch could not be attributed to a function -> " +
                 "the census can't tell whether it answers the I8 档; check the signature shape")
        continue
    }
    if (($must -notcontains $s.Fn) -and (-not $exempt.ContainsKey($s.Fn))) {
        $bad += ("V2 " + $s.File + ":" + $s.Line + " in " + $s.Fn + ": a vt switch that is in neither " +
                 "must nor exempt -> it silently answers the Long family without VT_I8. Either add it to " +
                 "$must AND the case, or to $exempt with the reason it doesn't need one")
    }
}

# ---------- V3 + V4: 名单自己 ----------
foreach ($name in $must) {
    if (-not $bodies.ContainsKey($name)) {
        $bad += ("V4 must-list entry " + $name + " has no function body in src\rtl -> renamed or deleted; " +
                 "this rule is now dead power")
        continue
    }
    if ($bodies[$name] -notlike "*case VT_I8:*") {
        $bad += ("V3 " + $name + ": in the must-list but its body has no 'case VT_I8:' -> x64 hands this " +
                 "table pointer-width values (VarPtr/StrPtr/ObjPtr answer LongPtr since §B138) and the " +
                 "switch drops them into default")
    }
}
foreach ($name in @($exempt.Keys)) {
    if (-not $bodies.ContainsKey($name)) {
        $bad += ("V4 exempt entry " + $name + " is not a function in src\rtl anymore -> drop the exemption " +
                 "together with the function")
    }
}

# ---------- V5: 针面非空 ----------
if ($must.Count -lt 10) {
    $bad += ("V5 must-list holds " + $must.Count + " names (floor 10) -> the needle surface collapsed; " +
             "if these tables really merged into one authority, delete this sentinel together with them")
}

if ($bad.Count -eq 0) {
    Write-Host ("PASS variant_i8_coverage: switches " + $sites.Count + " must " + $must.Count +
                " exempt " + $exempt.Count + " violations 0") -ForegroundColor Green
    exit 0
}
foreach ($b in $bad) { Write-Host ("FAIL " + $b) -ForegroundColor Red }
exit 1
