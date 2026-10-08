# check_com_prop_type_authority.ps1 - 账 #245 的结构性哨兵 (只扫源码, 不起 cl)
#
# 为什么单开这一道: 这一格的缺陷形状是"答案的来源没被守住"。类型库里的属性是 VARDESC,
# parseVarDesc 以前只在 VAR_PERINSTANCE 那一支给 member.returnType 赋值, 而 dual 接口的
# 属性 (stdole 的 StdFont.Name/Size/Bold/Charset ...) 是 VAR_PROPERTY(3) ⇒ 交出去的是
# **没赋过值的 16 位字段**。实测同一枚二进制连跑三次读到 29620 / 6971 / 50156, 换一档
# MSVC 编出来的二进制就稳定落在另一个值上 ⇒ 下游 resolveComValue 把这份垃圾当
# "这枚 COM 属性是什么型"的答案用, 于是 BSTR 属性被发成 vb6_ComGetIntProp, 而 CI 与本机
# 对同一笔提交交出两种发码 (门 #410 的 emit-samples 对差实测 13 行)。编译不响、链接不响、
# 语料 A/B 也只响一半, 所以要结构钉。
#
# 规则 (改坏了会红, 不是装饰):
#   C1  VARDESC 的类型无条件取: parseVarDesc 体内必须有 `member.returnType = mapTypeDesc(&pVD->elemdescVar.tdesc, pTI);`,
#       且该函数里**不许**再用 `if (pVD->varkind == VAR_PERINSTANCE)` 把它包回去 (那正是本次的现场)。
#   C2  交出去之前就有初值: typelib_parser.hpp 里 ComParamInfo / ComMemberInfo 两个 struct 的每个
#       **标量数据成员**声明行都必须带 `=` (std::string / std::vector 豁免)。没有初值的枚举字段
#       一旦被复制进 Symbol::ComMethodSig, 就是"每进程自洽、跨构建不同"的那一格。
#   C3  消费点口径一致: cgen_util_com.cpp 的 resolveComValue 里, 早期绑定那一支必须
#       ①先看 `it->second.isPropertyGet` (put/method 那份签名不许当属性读来解包),
#       ②类型未知时退回 `vb6_VariantFromComResult(vb6_ComGetProp(` 而不是按 unpackType 猜一档
#       (猜档就是 `L"Name"` 被发成 IntProp 的那条路; 同族另一处 cgen_expr_call_arg_emit.inc 本来就是退 VARIANT)。
#   C4   census: 全仓直接 `comMethods.find(` 的后端文件名单钉死为四份 (cgen_util_com.cpp /
#       cgen_expr_call_arg_emit.inc / cgen_expr_call_com_bind.inc / cgen_expr_call_prelude.inc)。
#       这一格教的是"同一个问题被抄成几份"的代价, 所以这里不假装已经合一 ——
#       名单外再开第五处读法 (即第五种问法) 必须先回来把口径并进来再登记。
#
# 用法: powershell -File scripts/check_com_prop_type_authority.ps1   (PASS ⇒ exit 0)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

$descPath = Join-Path $root 'src\com\typelib_parser_desc.cpp'
$hdrPath  = Join-Path $root 'src\com\typelib_parser.hpp'
$comPath  = Join-Path $root 'src\backend\cgen_util_com.cpp'

foreach ($p in @($descPath, $hdrPath, $comPath)) {
    if (-not (Test-Path $p)) { Write-Host "FAIL missing file: $p"; exit 1 }
}

$desc = Get-Content -Raw -Encoding UTF8 $descPath
$hdr  = Get-Content -Raw -Encoding UTF8 $hdrPath
$com  = Get-Content -Raw -Encoding UTF8 $comPath

$fails = @()

# ---- C1: parseVarDesc 无条件给 returnType 赋值 ----
$vIdx = $desc.IndexOf('ComMemberInfo TypeLibParser::parseVarDesc')
if ($vIdx -lt 0) {
    $fails += 'C1 parseVarDesc not found in typelib_parser_desc.cpp'
} else {
    $nIdx = $desc.IndexOf('ComMemberInfo TypeLibParser::parseFuncDesc', $vIdx)
    $endIdx = if ($nIdx -gt 0) { $nIdx } else { $desc.Length }
    $body = $desc.Substring($vIdx, $endIdx - $vIdx)
    if ($body -notmatch 'member\.returnType\s*=\s*mapTypeDesc\(\s*&pVD->elemdescVar\.tdesc') {
        $fails += 'C1 parseVarDesc no longer assigns member.returnType from elemdescVar.tdesc'
    }
    if ($body -match 'if\s*\(\s*pVD->varkind\s*==\s*VAR_PERINSTANCE\s*\)') {
        $fails += 'C1 parseVarDesc guards returnType on VAR_PERINSTANCE again (VAR_PROPERTY/VAR_CONST would emit an uninitialized field)'
    }
}

# ---- C2: 两个 struct 的标量成员都要有初值 ----
foreach ($st in @('ComParamInfo', 'ComMemberInfo')) {
    $sIdx = $hdr.IndexOf("struct $st {")
    if ($sIdx -lt 0) { $fails += "C2 struct $st not found"; continue }
    $eIdx = $hdr.IndexOf('};', $sIdx)
    $blk = $hdr.Substring($sIdx, $eIdx - $sIdx)
    foreach ($line in ($blk -split "`r?`n")) {
        $t = ($line -replace '//.*$', '').Trim()      # 先脱行尾注释: 声明行都带中文注释, 不脱就永远看不见它们
        if ($t -eq '' -or -not $t.EndsWith(';')) { continue }
        if ($t -match '^std::(string|vector)') { continue }
        if ($t -notmatch '=') {
            $fails += "C2 struct $st has an uninitialized scalar member: $t"
        }
    }
}

# ---- C3: resolveComValue 的早期绑定那一支 ----
$ebIdx = $com.IndexOf('if (isEarlyBoundCom_ && earlyBoundSym_)')
if ($ebIdx -lt 0) {
    $fails += 'C3 resolveComValue early-bound branch not found'
} else {
    # 支路自己第一行就是 8 空格缩进的 isEarlyBoundCom_ = false; —— 取"四空格缩进"那一行当支路尾
    $tailIdx = $com.IndexOf("`n    isEarlyBoundCom_ = false;", $ebIdx)
    $endIdx = if ($tailIdx -gt 0) { $tailIdx } else { $com.Length }
    $blk = $com.Substring($ebIdx, $endIdx - $ebIdx)
    if ($blk -notmatch 'it->second\.isPropertyGet') {
        $fails += 'C3 early-bound branch stopped checking isPropertyGet (a put/method signature must not be unpacked as a property read)'
    }
    if ($blk -notmatch 'vb6_VariantFromComResult\(vb6_ComGetProp\(') {
        $fails += 'C3 unknown return type no longer falls back to the generic VARIANT read'
    }
    if ($blk -match 'unpackType == "LongPtr"') {
        $fails += 'C3 the early-bound branch guesses from unpackType again (that is how BSTR props became ComGetIntProp)'
    }
}

# ---- C4: 消费者名单 ----
$backendDir = Join-Path $root 'src\backend'
$hits = Get-ChildItem -Path $backendDir -Recurse -File -Include *.cpp,*.inc,*.hpp |
        Where-Object { (Get-Content -Raw -Encoding UTF8 $_.FullName) -match 'comMethods\.find\(' } |
        ForEach-Object { $_.Name } | Sort-Object -Unique
$want = @('cgen_expr_call_arg_emit.inc', 'cgen_expr_call_com_bind.inc',
          'cgen_expr_call_prelude.inc', 'cgen_util_com.cpp')
if ($hits.Count -ne $want.Count -or (Compare-Object $hits $want)) {
    $fails += ('C4 comMethods.find consumers changed: expected ' + ($want -join ', ') +
               '; got ' + ($hits -join ', '))
}

if ($fails.Count -gt 0) {
    $fails | ForEach-Object { Write-Host "FAIL $_" }
    exit 1
}
Write-Host 'OK check_com_prop_type_authority: C1..C4'
exit 0
