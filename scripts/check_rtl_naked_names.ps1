# check_rtl_naked_names.ps1 - 账 #220 的结构性哨兵 (只扫源码, 不起 cl)
#
# 为什么单开这一道: RTL 里曾写着 `const int32_t B = 1; const int32_t BF = 2;` (外加头文件里的
# 两行 extern), 用途是让 parser 原样发射的 Picture.Line 语法旗标 `, B` / `, BF` 有个落脚处。
# 生成的模块 C 会 #include 那批 RTL 头, 而用户模块级变量在 C 里也是**裸名** —— 于是
# `Public B As Long` 直接撞成 C2373 重定义 + C2166 给 const 赋值, 连 exe 都出不来
# (改前探针实测: BUILD-RC=1 / 5 条诊断 / no exe)。这类名字与用户名字空间是**共享**的:
# 头里 extern 的在编译期撞, .c 里非 static 定义的在链接期撞 (LNK2005), 只有 static 的不撞。
#
# 口径: RTL 不许导出「裸名 = VB6 合法标识符」的文件作用域数据全局。旗标改由 parser 在
# Line 的 style 位置折成字面量 (parser_expr_postfix.cpp 一处), RTL 不再需要名字。
#
# 规则 (改坏了会红, 不是装饰):
#   N1  B / BF 这两枚裸名全局在 RTL 里必须彻底没有 (定义 0 + extern 0)
#   N2  非 static 的裸名文件作用域数据全局 = 一份钉死的名单 (多一枚就红; 名单要缩小
#       必须在同一次提交里改这里 —— 例如账 #219 把 Changed 归到 vb6_PropertyPage_Changed)
#   N3  旗标折成字面量这件事只许有一个地方: style 位守卫 1 / 两个词各 1 / 交出的字面量 1
#   N4  值口径不许飘: B -> 1, BF -> 2 (沿用删除前两枚全局的值, 本刀不改语义)
#
# 扫的是标量/指针档 (intN_t / long / char / double / BSTR / VARIANT / VB6_* 等)。
#
# 用法:  pwsh -File scripts\check_rtl_naked_names.ps1
# 退出码: 0 = 全绿; 1 = 红

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$bad = @()

$rtlDir = Join-Path $root 'src\rtl'
$parserRel = Join-Path $root 'src\parser\parser_expr_postfix.cpp'
if (-not (Test-Path -LiteralPath $rtlDir)) { Write-Host 'FAIL src\rtl missing' -ForegroundColor Red; exit 1 }
if (-not (Test-Path -LiteralPath $parserRel)) { Write-Host 'FAIL parser_expr_postfix.cpp missing' -ForegroundColor Red; exit 1 }

$types = '(?:int8_t|int16_t|int32_t|int64_t|uint8_t|uint16_t|uint32_t|uint64_t|long|short|unsigned\s+long|char|double|float|void\s*\*|BSTR|VARIANT|HRESULT|BOOL|BYTE|WORD|DWORD|LPARAM|WPARAM|size_t|VB6_[A-Za-z_]\w*)'
$globalPat = '^(?:extern\s+)?(?:static\s+)?(?:const\s+)?' + $types + '\s*(?:\*+\s*)?([A-Za-z_]\w*)\s*(?:\[[^\]]*\])?\s*(?:=[^;]*)?;'
$allowPat = '^(vb6_|VB6_|c3_|C3_)'

$rtlText = @{}
Get-ChildItem -Path $rtlDir -Recurse -Include *.c, *.h | ForEach-Object {
    $rtlText[$_.FullName] = [System.IO.File]::ReadAllText($_.FullName)
}

# N1: B / BF 不许以任何形式回到 RTL
$n1 = 0
foreach ($t in $rtlText.Values) {
    $n1 += @([regex]::Matches($t, '^(?:extern\s+)?(?:static\s+)?(?:const\s+)?(?:int32_t|uint32_t|int16_t|int)\s+\*?\s*(?:B|BF)\s*(?:\[[^\]]*\])?\s*(?:=[^;]*)?;', 'Multiline')).Count
}
if ($n1 -ne 0) { $bad += ('N1 naked B/BF globals back in RTL ' + $n1 + ' times (want 0 - the flag is folded by the parser now)') }

# N2: 非 static 的裸名文件作用域数据全局, 名单钉死
$found = @{}
foreach ($kv in $rtlText.GetEnumerator()) {
    foreach ($m in [regex]::Matches($kv.Value, $globalPat, 'Multiline')) {
        $decl = $m.Groups[0].Value
        $name = $m.Groups[1].Value
        if ($decl -match '^static\b') { continue }
        if ($name -match $allowPat) { continue }
        if (-not $found.ContainsKey($name)) { $found[$name] = 0 }
        $found[$name] += 1
    }
}
# 名单: Changed 归账 #219 (它和 vb6_PropertyPage_Changed 是同一件事的两枚全局);
#       g_uc_* / g_hoCount 是 UC 宿主的内部计数, 撞名概率低但同样该带前缀。
$pin = @('Changed', 'g_hoCount', 'g_uc_descCount', 'g_uc_recCount', 'g_uc_dumpSeq')
$extra = @($found.Keys | Where-Object { $pin -notcontains $_ })
$missing = @($pin | Where-Object { -not $found.ContainsKey($_) })
if ($extra.Count -gt 0) { $bad += ('N2 new naked RTL global: ' + ($extra -join ', ') + ' - VB lets a module name a variable that, so this collides') }
if ($missing.Count -gt 0) { $bad += ('N2 pinned census no longer matches: ' + ($missing -join ', ') + ' disappeared - update the pin in the same commit that removes it') }

# N3 + N4: 折旗标那一处
$ptxt = [System.IO.File]::ReadAllText($parserRel)
$guard = @([regex]::Matches($ptxt, 'if\s*\(\s*trailingIdx\s*==\s*1')).Count
$wordB = @([regex]::Matches($ptxt, 'optWord\s*==\s*"b"')).Count
$wordBF = @([regex]::Matches($ptxt, 'optWord\s*==\s*"bf"')).Count
$litInt = @([regex]::Matches($ptxt, 'LiteralKind::Integer,\s*optWord')).Count
$idName = @([regex]::Matches($ptxt, 'IdentifierExpr[^;]*optWord')).Count
if ($guard -ne 1) { $bad += ('N3 style-slot guard appears ' + $guard + ' times (want exactly 1 - one fold point)') }
if ($wordB -lt 1) { $bad += 'N3 the B flag is no longer folded here' }
if ($wordBF -lt 1) { $bad += 'N3 the BF flag is no longer folded here' }
if ($litInt -ne 1) { $bad += ('N3 literal handoff ' + $litInt + ' times (want 1)') }
if ($idName -ne 0) { $bad += ('N3 the flag name is back in the AST ' + $idName + ' times (want 0) - that is what created the collision') }
$pairText = @([regex]::Matches($ptxt, 'optWord\s*==\s*"b"\s*\?\s*"1"\s*:\s*"2"')).Count
$pairInt = @([regex]::Matches($ptxt, 'intValue\s*=\s*optWord\s*==\s*"b"\s*\?\s*1\s*:\s*2')).Count
if ($pairText -ne 1) { $bad += ('N4 flag text pair ' + $pairText + ' times (want 1)') }
if ($pairInt -ne 1) { $bad += ('N4 flag intValue pair ' + $pairInt + ' times (want 1) - B=1 / BF=2 came from the deleted globals, do not renumber') }

if ($bad.Count -gt 0) {
    foreach ($b in $bad) { Write-Host ('FAIL ' + $b) -ForegroundColor Red }
    exit 1
}
Write-Host ('PASS RTL naked-name guard: B/BF ' + $n1 + ', census ' + ($found.Keys.Count) +
    ' pinned names, fold point ' + $guard + '+' + $litInt + ', flag names in AST ' + $idName + ', value pair ' + $pairInt)
exit 0
