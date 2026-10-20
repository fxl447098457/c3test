# check_owned_temp_keeps.ps1 - 账 #454 §B164 形①（第一百三十五刀 = 「谁存住了传进去的带权值」那张 KEEPS 表）
#
# 这一刀**只做表，不动行为**：把「一条语句里现造的带权临时量传进去之后，谁把它存住了」
# 这件事做成一张**有出口权威的表**，并把「出口面」从 grep 名单改成**权威签名表**推出来的。
#
# 为什么必须先有表：上一版（9546dbfe / 6bf5989 / be5d6abf）把 §B164 里那条
# 「KEEPS = 0」的问法当成了「一切实参位上的现造串都没人存住」—— 那一句只覆盖
# 「vb6_VariantToString 第一个实参位上那 3578 处 mint 的消费者」，判的是 RTL 那一侧。
# 命题换错 ⇒ 门把 5 份既有夹具的运行期读数改了（注册表那一族整条断掉等）⇒ 那一版已撤回。
# 教训：**先证明「知道谁不能回收」，再谈「能回收」**。
#
# 三分类（这一版的口径）：
#   KEEPS=0  确定只在调用内读，不把指针存出去 —— 允许回收（**但是**回收那一刀还没开）
#   KEEPS=1  确定会把指针存到结构体/堆/全局，或原样返回出去，或经由它写出去 —— 禁止回收
#   U       静态分析或权威签名表答不出它到底存不存
# 逃逸通道三条（KEEPS=1 的「为什么」那一栏，缺一条就漏一族）：
#   store   存进结构体/堆/全局（指针活着越过调用）
#   ret     原样返回（借出去 ⇒ 调用方拿到的是同一枚指针，不是副本）
#   write   经由它写出去（形参是 BSTR* / VARIANT* 那种可写槽）
#
# 出口面（**不许用 grep 名单当权威**）：权威 = RTL 的那 18 个头
# （src/rtl/core/vb6rtl/*.h + src/rtl/core/vb6forms/*.h，也就是生成代码 include 的
# 那两张伞头 vb6rtl.h / vb6forms.h 拉进来的全部子头）。表里每一行都必须能从那 18 个头
# 推出来；推不出来 = unclassified ≠ 0 ⇒ 停机。
#
# 分类的**可证性**（这一版最要紧的一条）：只**证得出**的才给 0/1，证不出的给 U。
#   证得出 KEEPS=1：形参被**整枚**赋给一个结构体成员/文件级全局，或被**原样**返回，
#                   或取了它的地址
#   证得出 KEEPS=0：形参从不被整枚赋出去、不被原样返回、不取地址，且它流进去的每一枚
#                   vb6_* 出口自己就是 KEEPS=0（转手 ⇒ 下游说了算；成环 ⇒ U）
#   其余（赋给局部但那个局部的去向答不出、流进答不出的出口、找不到体）⇒ U
# ⚠ 「赋给局部」**不许**直接判 store：`BSTR copy = source;` 是**复制**，不是存指针。
#   那一族（复制）要靠「局部自己有没有再逃出去」来判，判不出就 U。
#
# 用法:  pwsh -File scripts/check_owned_temp_keeps.ps1            # 判
#        pwsh -File scripts/check_owned_temp_keeps.ps1 -Bless    # 重新登记快照
[CmdletBinding()]
param(
    [string]$Root = "",
    [switch]$Bless,
    [string]$TableFile = ""
)
$ErrorActionPreference = 'Continue'
if (-not $Root) { $Root = Split-Path -Parent $PSScriptRoot }
# ⚠ 参数**不许**叫 $Table：PowerShell 变量名大小写不敏感，而下面有一枚同名的小写 hashtable
# （$rows 那一枚）—— 两者是同一枚变量，[string] 形参会把 hashtable 折成字符串，
# 然后 `$rows[$k]` 报「Unable to index into an object of type System.String」。
if (-not $TableFile) { $TableFile = Join-Path $Root 'scripts\owned_temp_keeps_164.txt' }

$RtlHdrDir = Join-Path $Root 'src\rtl\core\vb6rtl'
$RtlFrmDir = Join-Path $Root 'src\rtl\core\vb6forms'
$bad = @()

# ---------------------------------------------------------------- 权威出口面
$hdrFiles = @()
foreach ($dir in @($RtlHdrDir, $RtlFrmDir)) {
    if (-not (Test-Path -LiteralPath $dir)) { continue }
    $hdrFiles += @(Get-ChildItem -LiteralPath $dir -Recurse -File -Filter *.h)
}
# ⚠ 枚举顺序**必须定死**（排序）：同名出口的原型取「第一次见到的那份」，而
# Get-ChildItem 的顺序在两次运行之间会变 ⇒ 表会变（踩过一次：快照漂 396 行，
# 而且两次漂的名单还不一样 ⇒ 表根本不可登记）。
$hdrFiles = @($hdrFiles | Sort-Object FullName)
$authorityHeaders = @($hdrFiles | ForEach-Object { $_.Name } | Sort-Object -Unique)

function Strip-Cmt([string]$S) {
    $t = ($S -replace '/\*.*?\*/', '') -replace '//.*$', ''
    $ix = $t.IndexOf('/*')
    if ($ix -ge 0) { $t = $t.Substring(0, $ix) }
    return $t
}
function Get-Balance([string]$S) {
    $n = 0
    foreach ($ch in $S.ToCharArray()) {
        if ($ch -eq '(' -or $ch -eq '[') { $n += 1 } elseif ($ch -eq ')' -or $ch -eq ']') { $n -= 1 }
    }
    return $n
}
function Split-Params([string]$Params) {
    $p = ($Params -replace '\s+', ' ').Trim()
    if ($p -eq '' -or $p -eq 'void') { return @() }
    $depth = 0; $cur = ''; $items = @()
    foreach ($ch in $p.ToCharArray()) {
        if ($ch -eq '(' -or $ch -eq '[') { $depth += 1 } elseif ($ch -eq ')' -or $ch -eq ']') { $depth -= 1 }
        if ($ch -eq ',' -and $depth -eq 0) { $items += $cur; $cur = '' } else { $cur += $ch }
    }
    $items += $cur
    $out = @()
    foreach ($it in $items) {
        $t = (($it -replace '\bconst\s+', ' ') -replace '\b(unsigned|signed)\s+', ' ').Trim()
        if ($t -eq '' -or $t -eq 'void' -or $t -eq '...') { continue }
        $out += $t
    }
    # ⚠ 逗号一元运算符：`return $out` 在**只有一个形参**时会被 PowerShell 展平成一条字符串
    # ⇒ 外面 `$params.Count` 变成**字符串长度**，形参位按字符扫过去，整枚出口掉出表外。
    return ,$out
}
# 形参「类型 + 名字」→ 类型记号。⚠ 先把**形参名**摘掉（"BSTR s" ⇒ "BSTR"），
# 摘不干净的那一格按「类型就是它自己」处理。
function Formal-Kind([string]$T) {
    $t = ($T -replace '\s+', ' ').Trim()
    $ty = ($t -replace '\b[A-Za-z_][A-Za-z0-9_]*\s*$', '').Trim()
    if ($ty -eq '') { $ty = $t }
    if ($ty -match '\bBSTR\s*\*$') { return 'BSTRPTR' }
    if ($ty -match '\bBSTR\s*$') { return 'BSTR' }
    if ($ty -match '\bvb6_VARIANT\s*\*$') { return 'VARIANTPTR' }
    if ($ty -match '\bvb6_VARIANT\s*$') { return 'VARIANT' }
    return ''
}

$protos = @{}          # 出口名 -> 形参类型数组
$surface = @{}         # "出口名|形参序号" -> 记号
foreach ($f in $hdrFiles) {
    $lines = [System.IO.File]::ReadAllLines($f.FullName)
    $i = 0
    while ($i -lt $lines.Count) {
        $ln = $lines[$i]
        if ($ln -eq '' -or $ln -match '^[ \t#/]') { $i += 1; continue }
        $buf = Strip-Cmt $ln
        $k = $i
        while ((Get-Balance $buf) -gt 0 -and ($k + 1 -lt $lines.Count)) {
            $k += 1
            $buf += ' ' + (Strip-Cmt $lines[$k]).Trim()
        }
        $open = $buf.IndexOf('(')
        $close = $buf.LastIndexOf(')')
        if ($open -lt 0 -or $close -le $open) { $i = $k + 1; continue }
        $head = $buf.Substring(0, $open).Trim()
        $m = [regex]::Match($head, '([A-Za-z_][A-Za-z0-9_]*)\s*$')
        if (-not $m.Success) { $i = $k + 1; continue }
        $name = $m.Groups[1].Value
        if ($name -notlike 'vb6_*') { $i = $k + 1; continue }
        $params = Split-Params $buf.Substring($open + 1, $close - $open - 1)
        if ($params.Count -eq 0) { $i = $k + 1; continue }
        if (-not $protos.ContainsKey($name)) {
            $protos[$name] = $params
            for ($pi = 0; $pi -lt $params.Count; $pi++) {
                $kind = Formal-Kind $params[$pi]
                if ($kind -eq 'BSTR' -or $kind -eq 'VARIANT') { $surface["$name|$pi"] = $kind }
            }
        }
        $i = $k + 1
    }
}
Write-Host ("KEEPS 权威出口面: " + $authorityHeaders.Count + " 个头 / " + $protos.Count +
            " 个原型 / " + $surface.Count + " 枚按值带权形参位")

# ---------------------------------------------------------------- RTL 体
$bodyOf = @{}
$globalsOf = @{}       # 文件级变量名（每枚出口记它所在文件的文件级变量）
foreach ($f in (Get-ChildItem -LiteralPath (Join-Path $Root 'src\rtl') -Recurse -File -Include *.c, *.h, *.inc | Sort-Object FullName)) {
    $txt = [System.IO.File]::ReadAllText($f.FullName)
    # 文件级变量：不在任何函数体内的 `static`/普通全局声明
    $gvars = @{}
    foreach ($gm in [regex]::Matches($txt, '(?m)^\s*(?:static\s+)?(?:const\s+)?[A-Za-z_][A-Za-z0-9_ \*]*\s+([A-Za-z_][A-Za-z0-9_]*)\s*(?:\[[^\]]*\])?\s*(?:=[^;]*)?;')) {
        $gvars[$gm.Groups[1].Value] = $true
    }
    foreach ($mm in [regex]::Matches($txt, '(?s)(?:^|\n)\s*(?:static\s+)?[A-Za-z_][A-Za-z0-9_ \*]*\b(vb6_[A-Za-z0-9_]+)\s*\(([^;{]*)\)\s*\{')) {
        $nm = $mm.Groups[1].Value
        if ($bodyOf.ContainsKey($nm)) { continue }
        $start = $mm.Index + $mm.Length
        $depth = 1
        $j = $start
        while ($j -lt $txt.Length -and $depth -gt 0) {
            if ($txt[$j] -eq '{') { $depth += 1 } elseif ($txt[$j] -eq '}') { $depth -= 1 }
            $j += 1
        }
        $bodyOf[$nm] = $txt.Substring($start, $j - $start)
        $globalsOf[$nm] = $gvars
        # **内部静态助手的形参表**（不在公开头里 ⇒ 不进出口面，但**转手判定**要用：
        # 下游是谁、它那一格是带权位不是，答不出就把本枚降成 U）
        if (-not $protos.ContainsKey($nm)) {
            $dp = Split-Params $mm.Groups[2].Value
            if ($dp.Count -gt 0) { $protos[$nm] = $dp }
        }
    }
}

# ---------------------------------------------------------------- 分类
$script:clsClass = ''
$script:clsEscape = ''
$script:clsMemo = @{}
$script:clsStack = @{}

function Get-LocalNames([string]$body) {
    $names = @{}
    foreach ($lm in [regex]::Matches($body, '(?m)^\s*(?:const\s+)?[A-Za-z_][A-Za-z0-9_]*(?:\s*\*)?(?:\s+[A-Za-z_][A-Za-z0-9_]*)*\s+([A-Za-z_][A-Za-z0-9_]*)\s*(?:[=;\[])' )) {
        $names[$lm.Groups[1].Value] = $true
    }
    return $names
}

function Classify([string]$name, [int]$idx) {
    $key = "$name|$idx"
    if ($script:clsMemo.ContainsKey($key)) {
        $m = $script:clsMemo[$key]
        $script:clsClass = $m[0]; $script:clsEscape = $m[1]
        return
    }
    if ($script:clsStack.ContainsKey($key)) {   # 成环 ⇒ 答不出（**并把它记成 U**：
        # 不记的话，这枚的答案就取决于「从环的哪一头进来」⇒ 同一份源码两次算两样）
        $script:clsClass = 'U'; $script:clsEscape = 'cycle'
        $script:clsMemo[$key] = @('U', 'cycle')
        return
    }
    $script:clsStack[$key] = $true
    $script:clsClass = 'U'; $script:clsEscape = 'unknown'

    if (-not $bodyOf.ContainsKey($name)) { $script:clsEscape = 'nobody' }
    elseif (-not $protos.ContainsKey($name)) { $script:clsEscape = 'noproto' }
    else {
        $params = $protos[$name]
        if ($idx -ge $params.Count) { $script:clsEscape = 'noproto' }
        else {
            $nm = [regex]::Match($params[$idx], '([A-Za-z_][A-Za-z0-9_]*)\s*$')
            if (-not $nm.Success) { $script:clsEscape = 'noname' }
            else {
                $arg = $nm.Groups[1].Value
                $body = $bodyOf[$name]
                $locals = Get-LocalNames $body
                $gvars = $globalsOf[$name]
                $esc = [System.Collections.Generic.List[string]]::new()
                $why = [System.Collections.Generic.List[string]]::new()

                # (1) 整枚赋给一个**非局部**的左值（结构体成员 / 文件级全局）⇒ store
                foreach ($mm in [regex]::Matches($body, '(?m)([A-Za-z_][A-Za-z0-9_]*(?:\s*(?:->|\.)\s*[A-Za-z_][A-Za-z0-9_]*)+)\s*=\s*\(?\s*' + [regex]::Escape($arg) + '\s*\)?\s*;')) {
                    $esc.Add('store') | Out-Null; $why.Add('field') | Out-Null; break
                }
                if ($esc.Count -eq 0) {
                    foreach ($mm in [regex]::Matches($body, '(?m)(?<![*\w])([A-Za-z_][A-Za-z0-9_]*)\s*=\s*\(?\s*' + [regex]::Escape($arg) + '\s*\)?\s*;')) {
                        $lhs = $mm.Groups[1].Value
                        if ($lhs -eq $arg) { continue }
                        if ($locals.ContainsKey($lhs)) {
                            # 赋给局部：那一枚局部自己逃不逃？（复制 vs 存指针）
                            if ([regex]::IsMatch($body, '(?m)return\s+\(?\s*' + [regex]::Escape($lhs) + '\s*\)?\s*;')) {
                                $esc.Add('ret') | Out-Null; $why.Add('local-returned') | Out-Null
                            } elseif ([regex]::IsMatch($body, '(?m)&' + [regex]::Escape($lhs) + '\b')) {
                                $esc.Add('write') | Out-Null; $why.Add('local-address') | Out-Null
                            } else {
                                $why.Add('local-copy') | Out-Null
                            }
                        } elseif ($gvars.ContainsKey($lhs)) {
                            $esc.Add('store') | Out-Null; $why.Add('global') | Out-Null
                        } else {
                            $esc.Add('U') | Out-Null; $why.Add('unknown-lvalue') | Out-Null
                        }
                        break
                    }
                }
                # (2) 原样返回（借出去，不是副本）
                if ([regex]::IsMatch($body, '(?m)return\s+\(?\s*' + [regex]::Escape($arg) + '\s*\)?\s*;')) {
                    if (-not $esc.Contains('ret')) { $esc.Add('ret') | Out-Null; $why.Add('returned') | Out-Null }
                }
                # (3) 取地址（经它写出去），或**经解引用写出去**（`*slot = arg` ⇒ 形参
                #     那一格是可写槽，调用方拿到的就是这枚指针）
                if ([regex]::IsMatch($body, '&' + [regex]::Escape($arg) + '\b')) {
                    if (-not $esc.Contains('write')) { $esc.Add('write') | Out-Null; $why.Add('address') | Out-Null }
                }
                if ([regex]::IsMatch($body, '\*\s*[A-Za-z_][A-Za-z0-9_]*\s*=\s*\(?\s*' + [regex]::Escape($arg) + '\s*\)?\s*;')) {
                    if (-not $esc.Contains('write')) { $esc.Add('write') | Out-Null; $why.Add('deref-write') | Out-Null }
                }
                # (4) 转手给另一枚 vb6_* 出口 ⇒ 下游说了算
                if ($esc.Count -eq 0 -or $esc.Contains('U')) {
                    foreach ($mm in [regex]::Matches($body, '\b(vb6_[A-Za-z0-9_]+)\s*\(([^;]*?)\)')) {
                        $callee = $mm.Groups[1].Value
                        if ($callee -eq $name) { continue }
                        $callArgs = Split-Params $mm.Groups[2].Value
                        # 这一枚形参在**调用点的第几格**（按位置，不按名字 —— 下游的形参名
                        # 与上游的形参名常常不是同一个字，按名字配会配不上）
                        $didx = -1
                        for ($q = 0; $q -lt $callArgs.Count; $q++) {
                            if ([regex]::IsMatch($callArgs[$q], '\b' + [regex]::Escape($arg) + '\b')) { $didx = $q; break }
                        }
                        if ($didx -lt 0) { continue }   # 这一枚形参没进这通调用
                        if (-not $bodyOf.ContainsKey($callee)) {
                            $esc.Add('U') | Out-Null; $why.Add("handoff-unknown:$callee") | Out-Null; break
                        }
                        if (-not $protos.ContainsKey($callee) -or $didx -ge $protos[$callee].Count) {
                            $esc.Add('U') | Out-Null; $why.Add("handoff-unknown-slot:$callee") | Out-Null; break
                        }
                        $downKind = Formal-Kind $protos[$callee][$didx]
                        if ($downKind -ne 'BSTR' -and $downKind -ne 'VARIANT') {
                            # 下游那一格不是带权位（转手过去的是别的类型）⇒ 本枚不必管
                            $why.Add("handoff-to-plain:$callee") | Out-Null; break
                        }
                        Classify $callee $didx
                        if ($script:clsClass -eq '1') { $esc.Add('store') | Out-Null; $why.Add("handoff-keeps:$callee") | Out-Null }
                        elseif ($script:clsClass -eq 'U') { $esc.Add('U') | Out-Null; $why.Add("handoff-unknown:$callee") | Out-Null }
                        else { $why.Add("handoff-reads:$callee") | Out-Null }
                        break
                    }
                }
                if ($esc.Contains('store') -or $esc.Contains('ret') -or $esc.Contains('write')) {
                    $script:clsClass = '1'
                    $script:clsEscape = (($esc | Sort-Object -Unique) -join '+')
                } elseif ($esc.Contains('U')) {
                    $script:clsClass = 'U'
                    $script:clsEscape = (($why | Sort-Object -Unique) -join ';')
                } else {
                    $script:clsClass = '0'
                    $script:clsEscape = (($why | Sort-Object -Unique) -join ';')
                }
            }
        }
    }
    $script:clsMemo[$key] = @($script:clsClass, $script:clsEscape)
    $script:clsStack.Remove($key)
}

$rows = @{}
# ⚠ 遍历顺序必须**定死**（排序）：分类里有成环判定，顺序不同会得出不同的表
# （踩过一次： hashtable 的 Keys 顺序变一次，快照就漂 396 行 ⇒ 表变成不可登记的）。
foreach ($key in ($surface.Keys | Sort-Object)) {
    $parts = $key -split '\|'
    Classify $parts[0] ([int]$parts[1])
    $rows[$key] = @($script:clsClass, $script:clsEscape, $surface[$key])
}
$k0 = @($rows.Values | Where-Object { $_[0] -eq '0' }).Count
$k1 = @($rows.Values | Where-Object { $_[0] -eq '1' }).Count
$ku = @($rows.Values | Where-Object { $_[0] -eq 'U' }).Count
Write-Host ("KEEPS 表: KEEPS=0 $k0 / KEEPS=1 $k1 / U $ku （共 " + $rows.Count + " 行）")

# ---------------------------------------------------------------- 三锚
# 正控 = 存进全局那一枚（vb6_ErrRaise 把 source/description 存进 vb6_err）⇒ KEEPS=1
# 负控 = 只读那一枚（vb6_Len 只读）⇒ KEEPS=0
# 灰控 = 用户过程 / 晚绑定 COM ⇒ U（不是表里的行，是**规则**上的那一类）
$anchors = @(
    @{ key = 'vb6_ErrRaise|1'; want = '1'; why = 'store: 存进全局 vb6_err' },
    @{ key = 'vb6_ErrRaise|2'; want = '1'; why = 'store: 存进全局 vb6_err' },
    @{ key = 'vb6_Len|0';      want = '0'; why = 'read-only: 只在调用内读' }
)
foreach ($a in $anchors) {
    if (-not $rows.ContainsKey($a.key)) {
        $bad += ("K1 锚点缺失（表里没有这一行）: " + $a.key + " —— " + $a.why)
        continue
    }
    if ($rows[$a.key][0] -ne $a.want) {
        $bad += ("K1 锚点 " + $a.key + " 分类 = " + $rows[$a.key][0] + "，期望 " + $a.want +
                 "（" + $a.why + "；实测逃逸通道 = " + $rows[$a.key][1] + "）")
    }
}
$grayRules = @('user-proc', 'late-bound-com')

# ---------------------------------------------------------------- 快照对账
$snap = @{}
if (Test-Path -LiteralPath $TableFile) {
    foreach ($ln in [System.IO.File]::ReadAllLines($TableFile)) {
        if ($ln -eq '' -or $ln.StartsWith('#')) { continue }
        # ⚠ 字段分隔**不许用 '|'**：键自己带一个 '|'（出口名|形参序号），用 '|' 分隔
        # 就把键切成两半 ⇒ 快照对账全表漂（踩过一次：漂 396 行，名单每跑一次还不一样）。
        # 用 TAB：键与逃逸通道里都不可能出现 TAB。
        $p = $ln -split "`t", 4
        if ($p.Count -lt 4) { continue }
        $snap[$p[0]] = @($p[1], $p[2], $p[3])
    }
}
if ($Bless) {
    $out = @()
    $out += '# owned_temp_keeps_164.txt —— 账 #454 §B164 形① 的 KEEPS 表（出口面出自 RTL 的权威头）'
    $out += '# 格式: 出口名|形参序号 <TAB> 分类(0/1/U) <TAB> 逃逸通道 <TAB> 形参档'
    $out += '# 分类口径: 0 = 只在调用内读 / 1 = 存|原样返回|经它写出去 / U = 证不出'
    $out += '# 用户过程与晚绑定 COM 不在表里（不是具名出口）⇒ 按规则归 U，见 check_owned_temp_keeps.ps1'
    foreach ($k in ($rows.Keys | Sort-Object)) {
        $v = $rows[$k]
        $out += ($k + "`t" + $v[0] + "`t" + $v[1] + "`t" + $v[2])
    }
    [System.IO.File]::WriteAllLines($TableFile, $out, (New-Object System.Text.UTF8Encoding($false)))
    Write-Host ("已登记快照: " + $TableFile + " （" + $rows.Count + " 行）")
    exit 0
}

# K2: unclassified == 0 —— 出口面里每一枚按值带权形参位都必须有行
$missing = @($surface.Keys | Where-Object { -not $rows.ContainsKey($_) })
if ($missing.Count -gt 0) {
    $bad += ("K2 出口面里有 " + $missing.Count + " 枚带权形参位在表里没有行（unclassified ≠ 0）: " +
             (($missing | Select-Object -First 6) -join ', '))
}
# K3: 表里不许有**出口面之外**的行（表比权威面大 = 表自己造了出口）
$extra = @($rows.Keys | Where-Object { -not $surface.ContainsKey($_) })
if ($extra.Count -gt 0) {
    $bad += ("K3 表里有 " + $extra.Count + " 行不在权威出口面里（表自己造了出口）: " +
             (($extra | Select-Object -First 6) -join ', '))
}
# K4: U 那一类必须**显式存在且非空**（不可判定不许被静默归 0）
if ($ku -eq 0) {
    $bad += 'K4 U（不可判定）那一类是空的 —— 答不出的出口必须显式归 U，不许静默变成 KEEPS=0'
}
# K5: 快照 == 现算（漂移即红）
if ($snap.Count -eq 0) {
    $bad += 'K5 快照 ' + $TableFile + ' 不存在或读不出 —— 先跑一次 -Bless 登记'
} else {
    $drift = @()
    foreach ($k in $rows.Keys) {
        if (-not $snap.ContainsKey($k)) { $drift += ('+' + $k) } else {
            $s = $snap[$k]; $v = $rows[$k]
            if ($s[0] -ne $v[0] -or $s[1] -ne $v[1] -or $s[2] -ne $v[2]) { $drift += ('~' + $k) }
        }
    }
    foreach ($k in $snap.Keys) { if (-not $rows.ContainsKey($k)) { $drift += ('-' + $k) } }
    if ($drift.Count -gt 0) {
        $bad += ("K5 表与快照漂移 " + $drift.Count + " 行（先归因，再 -Bless）: " +
                 (($drift | Select-Object -First 8) -join ', '))
    }
}
# K6: 灰控两条规则必须在案（用户过程 / 晚绑定 COM 归 U）
if ($grayRules.Count -ne 2) { $bad += 'K6 灰控规则条数不对（用户过程 + 晚绑定 COM，两条都要在案）' }
# K7: 负证人登记 —— 撤回那一版弄坏过 5 份既有夹具，它们从「受害者」升级成**预先登记的证人**
$witnesses = @(
    'tests\test_registry_settings.bas',
    'tests\test_bstr_borrow_slot.bas',
    'tests\test_host_decimal.bas'
)
$missingW = @($witnesses | Where-Object { -not (Test-Path -LiteralPath (Join-Path $Root $_)) })
if ($missingW.Count -gt 0) {
    $bad += ("K7 负证人不存在: " + ($missingW -join ', '))
}

if ($bad.Count -eq 0) {
    Write-Host ("PASS owned temp keeps: authority headers " + $authorityHeaders.Count +
                " / surface " + $surface.Count + " / rows " + $rows.Count +
                " | KEEPS=0 " + $k0 + " KEEPS=1 " + $k1 + " U " + $ku +
                " | unclassified 0 | 3 anchors OK | snapshot OK | gray rules 2 | witnesses 3")
    exit 0
}
foreach ($b in $bad) { Write-Host ("FAIL " + $b) -ForegroundColor Red }
exit 1
