# check_object_slot_boxing.ps1 - 账 #305 §B187 装箱腿的结构性哨兵 (只扫源码, 不起 cl)
#
# 为什么单开这一道: 「一枚工程内实例交给泛对象槽 (void* 里该装 IDispatch*) 时该发什么」这一问
# 改前的答案是"直接发裸指针" —— 于是槽里/形参里是一枚 `vb6_cls_X*`, 晚绑定那一族 (vb6_ComCall /
# 外部 COM 实参) 各自读它: 前者静默空答, 后者 0xC0000005。现在这一问只有一处回答
# (boxIntoLateSlot 那一枚唯一发码处, 三处站点都问它), 而它要问两句「这枚类在不在运行期 coclass
# 表里」和「这枚变量此刻装的是不是包装器」—— 那两句也各只有一处定义。哨兵钉的是这四件事不许再
# 长出第二份。
#
# 规则 (改坏了会红, 不是装饰):
#   B1  装箱的发码处只许一枚: src/backend 的代码行里 `vb6_ComObject_FromBorrowedInstance` 恰好
#       4 处 = 唯一发码处 1 + 站点那条 boxedHere 前缀判据 1 + cgen_com.cpp 的 getter 2; 其中
#       **发出**包装器的那一句 (`return "(void*)vb6_ComObject_FromBorrowedInstance`) 恰好 1 处。
#       `boxIntoLateSlot(` 恰好 5 处 = 声明 1 + 定义 1 + 站点 3 (Set / 位置实参 / 命名实参);
#       `cParamLateSlotType(` 恰好 4 处 = 声明 1 + 定义 1 + 实参站点 2。
#   B2  「哪一档的类在 coclass 表里占一行」这条判据只许住在 Symbol::isCoClassTableEntry:
#       全 src 里 `instancing == VBInstancing::MultiUse|SingleUse` 必须恰好 2 处且都在
#       symbol_table.hpp 的定义体内 (cgen_com_events.cpp / driver_codegen_dll_typelib.inc
#       那两处问的是 `== Private` 另一件事, 不归本条管)。parser 那唯一的写者
#       (mod.instancing = VBInstancing::MultiUse) 必须还在 1 处; 消费点 (->isCoClassTableEntry())
#       至少 3 处: 建表收集 / As New 那一支 / driver 那份工程级名单 —— 少一处就是有人又开始
#       手抄判据。
#   B3  「这枚变量此刻装着包装器吗」只有 varHoldsComWrapper 一处回答 (cgen_api.inc): src 代码行
#       恰好 8 处 = 定义 1 + 消费 7 (读者① typed 目标 / 读者② 类槽实参 / 读者③ Is 两臂 /
#       登记块 / 别名复制 / 唯一发码处的槽→槽负条件); 直查 `wrappedComVars_.count` 只许剩定义
#       体内那 1 处, 别处必须 0 —— 179b 改问它之前那里就是一处手抄。
#   B4  登记表只有一处写、两处销: `wrappedComVars_.insert` 恰好 1 处, `wrappedComVars_.erase`
#       恰好 2 处 (非包装器那一支 + Variant 槽那一支)。多一处 insert 就会留下过期答案, 少一处
#       erase 就是「最后一次交的不是包装器」那条口径断了 (台账 §B187 那条 CreateObject 的读法)。
#   B5  负条件齐件: 唯一发码处那一份 (cgen_base_type.cpp) 必须同时含句柄闸 (vb6_hwnd_ /
#       vb6_form_ctrl_) + desc 闸 (classHasCoClassDesc) + 登记表闸 (varHoldsComWrapper) +
#       Variant 闸 (cExprIsVariant); 两枚实参站点各自必须含 isByVal 与 cParamLateSlotType
#       —— ByRef 那一档刻意不装箱 (要落临时就丢写回), 闸门掉了就会一路吃掉 ByRef;
#       Set 那一站必须还问 lateBoundSlotTarget 并给登记表写 boxedHere。
#   B6  夹具与两架构的真跑格都在: tests/b187_box/B187d.vbp 与 B187Arg.cls (第四臂 = 实参那一站)
#       存在, run_tests.ps1 里 b187_boxed_object_slot 恰好 2 处 (x64 + x86)、CREATE4MEM 恰好 4 处
#       (每臂各一条正针 + 一条反面证人)。
#
# 用法:  pwsh -File scripts\check_object_slot_boxing.ps1
# 退出码: 0 = 全绿; 1 = 红

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$script:bad = @()
$srcExts = @(".c", ".h", ".cpp", ".inc", ".hpp")

function Text-Of([string]$rel) {
    $p = Join-Path $root $rel
    if (-not (Test-Path -LiteralPath $p)) { return $null }
    return [System.IO.File]::ReadAllText($p)
}

# 只数**代码行**上的出现: 整行注释 (以 // 起头) 与块注释续行 (以 * 起头) 不算 ——
# 这些注释里必然提到被钉的符号名 (账 #297 那条"被行尾块注释挡住"的教训是反方向的:
# 认漏了会假绿, 认多了会假红, 所以先把注释放掉再数)。
# $skipRel 非空 = 这个文件整份跳过 (用它把"定义本身"从"别处不许有"那一问里摘出来)。
function Count-InFiles([string]$dirRel, [string[]]$exts, [string]$pattern, [string]$skipRel = "") {
    $dir = Join-Path $root $dirRel
    if (-not (Test-Path -LiteralPath $dir)) { $script:bad += ("missing dir " + $dirRel); return 0 }
    $skipFull = ""
    if ($skipRel) { $skipFull = (Join-Path $root $skipRel) }
    $n = 0
    foreach ($f in Get-ChildItem -LiteralPath $dir -Recurse -File |
                   Where-Object { $exts -contains $_.Extension }) {
        if ($skipFull -and ($f.FullName -ieq $skipFull)) { continue }
        foreach ($line in [System.IO.File]::ReadAllText($f.FullName) -split "`r?`n") {
            $t = $line.TrimStart()
            if ($t.StartsWith("//") -or $t.StartsWith("*")) { continue }
            $n += @([regex]::Matches($line, $pattern)).Count
        }
    }
    return $n
}

$boxRel = "src\backend\cgen_base_type.cpp"
$stationRel = "src\backend\detail\stmt\cgen_setlet_set_rhs.inc"

# B1
$nBorrow = Count-InFiles "src\backend" $srcExts "vb6_ComObject_FromBorrowedInstance"
$nEmit = @([regex]::Matches((Text-Of $boxRel), 'return "\(void\*\)vb6_ComObject_FromBorrowedInstance')).Count
$nBoxFn = Count-InFiles "src\backend" $srcExts "boxIntoLateSlot\s*\("
$nSlotParam = Count-InFiles "src\backend" $srcExts "cParamLateSlotType\s*\("
if ($nBorrow -ne 4) { $script:bad += ("B1 FromBorrowedInstance 在 src/backend 代码行 = $nBorrow (要 4: 发码处 + 站点前缀判据 + getter 2)") }
if ($nEmit -ne 1) { $script:bad += ("B1 发出包装器那一句 = $nEmit (要 1, 只许住在唯一发码处)") }
if ($nBoxFn -ne 5) { $script:bad += ("B1 boxIntoLateSlot( = $nBoxFn (要 5: 声明 1 + 定义 1 + 站点 3)") }
if ($nSlotParam -ne 4) { $script:bad += ("B1 cParamLateSlotType( = $nSlotParam (要 4: 声明 1 + 定义 1 + 实参站点 2)") }

# B2
$symRel = "src\semantics\symbol_table.hpp"
$nRuleAll = Count-InFiles "src" $srcExts "instancing\s*==\s*VBInstancing::(MultiUse|SingleUse)"
$nRuleDef = @([regex]::Matches((Text-Of $symRel), "instancing\s*==\s*VBInstancing::(MultiUse|SingleUse)")).Count
if ($nRuleAll -ne 2 -or $nRuleDef -ne 2) {
    $script:bad += ("B2 手抄 coclass 判据: 全 src=$nRuleAll, 定义内=$nRuleDef (要 2/2)")
}
$nParserWrite = Count-InFiles "src\parser" $srcExts "mod\.instancing = VBInstancing::MultiUse"
if ($nParserWrite -ne 1) { $script:bad += ("B2 parser 那个唯一写者 = $nParserWrite (要 1)") }
$nUse = Count-InFiles "src" $srcExts "->isCoClassTableEntry\(\)"
if ($nUse -lt 3) { $script:bad += ("B2 isCoClassTableEntry 消费点 = $nUse (要 >=3)") }

# B3
$apiRel = "src\backend\detail\util\cgen_api.inc"
$nAsk = Count-InFiles "src" $srcExts "varHoldsComWrapper\s*\("
$nAskDef = @([regex]::Matches((Text-Of $apiRel), "varHoldsComWrapper\s*\(")).Count
$nRawElse = Count-InFiles "src\backend" $srcExts "wrappedComVars_\.count" $apiRel
$nRawDef = @([regex]::Matches((Text-Of $apiRel), "wrappedComVars_\.count")).Count
if ($nAsk -ne 8 -or $nAskDef -ne 1) {
    $script:bad += ("B3 varHoldsComWrapper 全 src/定义内 = $nAsk/$nAskDef (要 8/1: 定义 1 + 消费 7)")
}
if ($nRawElse -ne 0 -or $nRawDef -ne 1) {
    $script:bad += ("B3 直查 wrappedComVars_.count 别处/定义内 = $nRawElse/$nRawDef (要 0/1)")
}
$nUnwrap = Count-InFiles "src\backend" $srcExts "unwrapComVarExpr\s*\("
if ($nUnwrap -ne 5) { $script:bad += ("B3 unwrapComVarExpr 代码行 = $nUnwrap (要 5: 定义 1 + 三处读者四臂)") }

# B4
$nIns = Count-InFiles "src" $srcExts "wrappedComVars_\.insert"
$nErase = Count-InFiles "src" $srcExts "wrappedComVars_\.erase"
if ($nIns -ne 1) { $script:bad += ("B4 wrappedComVars_.insert = $nIns (要 1, 登记表只许一处写)") }
if ($nErase -ne 2) { $script:bad += ("B4 wrappedComVars_.erase = $nErase (要 2: 非包装器那一支 + Variant 槽那一支)") }

# B5
$boxTxt = Text-Of $boxRel
foreach ($need in @("vb6_hwnd_", "vb6_form_ctrl_", "classHasCoClassDesc", "varHoldsComWrapper", "cExprIsVariant")) {
    $c = @([regex]::Matches($boxTxt, [regex]::Escape($need))).Count
    if ($c -lt 1) { $script:bad += ("B5 唯一发码处缺负条件件 " + $need + " (= " + $c + ")") }
}
foreach ($stn in @("src\backend\detail\expr\cgen_expr_call_arg_emit.inc",
                   "src\backend\detail\expr\cgen_expr_call_named_args.inc")) {
    $txt = Text-Of $stn
    foreach ($need in @("isByVal", "cParamLateSlotType")) {
        $c = @([regex]::Matches($txt, [regex]::Escape($need))).Count
        if ($c -lt 1) { $script:bad += ("B5 实参站点 " + $stn + " 缺闸门件 " + $need) }
    }
}
$stTxt = Text-Of $stationRel
foreach ($need in @("lateBoundSlotTarget", "boxedHere")) {
    $c = @([regex]::Matches($stTxt, [regex]::Escape($need))).Count
    if ($c -lt 1) { $script:bad += ("B5 Set 那一站缺 " + $need) }
}

# B6
if (-not (Test-Path -LiteralPath (Join-Path $root "tests\b187_box\B187d.vbp"))) {
    $script:bad += "B6 缺夹具 tests/b187_box/B187d.vbp"
}
if (-not (Test-Path -LiteralPath (Join-Path $root "tests\b187_box\B187Arg.cls"))) {
    $script:bad += "B6 缺第四臂夹具 tests/b187_box/B187Arg.cls (实参那一站)"
}
$nCells = @([regex]::Matches((Text-Of "tests\run_tests.ps1"), "b187_boxed_object_slot")).Count
if ($nCells -ne 2) { $script:bad += ("B6 b187_boxed_object_slot 在 run_tests.ps1 里 = $nCells (要 2: x64 + x86)") }
$nArm4 = @([regex]::Matches((Text-Of "tests\run_tests.ps1"), "CREATE4MEM")).Count
if ($nArm4 -ne 4) { $script:bad += ("B6 CREATE4MEM 在 run_tests.ps1 里 = $nArm4 (要 4: 两架构各一正针 + 一反面证人)") }

if ($script:bad.Count -eq 0) {
    Write-Host ("PASS boxing-emits=1 stations=3 borrowed-refs=4 coclass-predicate-sites=" + $nUse `
        + " wrapper-ask-sites=" + $nAsk + " registry-ins=" + $nIns + " erase=" + $nErase `
        + " cells=" + $nCells + " arm4=" + $nArm4) -ForegroundColor Green
    exit 0
}
foreach ($b in $script:bad) { Write-Host ("FAIL " + $b) -ForegroundColor Red }
exit 1
