# check_object_slot_boxing.ps1 - 账 #305 §B187 装箱一族的结构性哨兵 (只扫源码, 不起 cl)
#
# 为什么单开这一道: 「一枚工程内实例交给泛对象槽 (void* 里该装 IDispatch*) 时该发什么」这一问
# 改前的答案是"直接发裸指针" —— 于是槽里/形参里是一枚 `vb6_cls_X*`, 晚绑定那一族 (vb6_ComCall /
# 外部 COM 实参) 各自读它: 前者静默空答, 后者 0xC0000005。现在这一问只有一处回答
# (boxIntoLateSlot 那一枚唯一发码处, 三处站点都问它), 而它要问三句:「这枚类在不在运行期 coclass
# 表里」、「这枚变量此刻装的是不是包装器」、「这枚**值**自己是不是包装器」—— 三句各只有一处定义。
# ByRef 那一座桥 (第五腿) 的两端也一律问第三句, 且两句发码文本只许从 bridgeByRefLateSlotArg 出。
# 哨兵钉的是这五件事不许再长出第二份。
#
# 规则 (改坏了会红, 不是装饰):
#   B1  装箱的发码处只许一枚: src/backend 的代码行里 `vb6_ComObject_FromBorrowedInstance` 恰好
#       5 处 = 唯一发码处 1 + ByRef 桥的进场那一句 1 + 站点那条 boxedHere 前缀判据 1 +
#       cgen_com.cpp 的 getter 2; 其中 **按值发出**包装器的那一句
#       (`return "(void*)vb6_ComObject_FromBorrowedInstance`) 恰好 1 处。
#       `boxIntoLateSlot(` 恰好 5 处 = 声明 1 + 定义 1 + 站点 3 (Set / 位置实参 / 命名实参);
#       `cParamLateSlotType(` 恰好 5 处 = 声明 1 + 定义 1 + ByVal 实参站点 2 + ByRef 桥那一问 1
#       (桥与 ByVal 那一档**共用同一把形参声明尺**, 不许另立)。
#   B2  「哪一档的类在 coclass 表里占一行」这条判据只许住在 Symbol::isCoClassTableEntry:
#       全 src 里 `instancing == VBInstancing::MultiUse|SingleUse` 必须恰好 2 处且都在
#       symbol_table.hpp 的定义体内 (cgen_com_events.cpp / driver_codegen_dll_typelib.inc
#       那两处问的是 `== Private` 另一件事, 不归本条管)。parser 那唯一的写者
#       (mod.instancing = VBInstancing::MultiUse) 必须还在 1 处; 消费点 (->isCoClassTableEntry())
#       至少 3 处: 建表收集 / As New 那一支 / driver 那份工程级名单 —— 少一处就是有人又开始
#       手抄判据。
#   B3  「这枚**变量**此刻装着包装器吗」只有 varHoldsComWrapper 一处回答 (cgen_api.inc): src 代码行
#       恰好 8 处 = 定义 1 + 消费 7 (读者① typed 目标 / 读者② 类槽实参 / 读者③ Is 两臂 /
#       登记块 / 别名复制 / 唯一发码处的槽→槽负条件); 直查 `wrappedComVars_.count` 只许剩定义
#       体内那 1 处, 别处必须 0 —— 179b 改问它之前那里就是一处手抄。
#   B4  登记表只有一处写、两处销: `wrappedComVars_.insert` 恰好 1 处, `wrappedComVars_.erase`
#       恰好 2 处 (非包装器那一支 + Variant 槽那一支)。多一处 insert 就会留下过期答案, 少一处
#       erase 就是「最后一次交的不是包装器」那条口径断了 (台账 §B187 那条 CreateObject 的读法)。
#   B5  负条件齐件: 唯一发码处那一份 (cgen_base_type.cpp) 必须同时含句柄闸 (vb6_hwnd_ /
#       vb6_form_ctrl_) + desc 闸 (classHasCoClassDesc) + 登记表闸 (varHoldsComWrapper) +
#       Variant 闸 (cExprIsVariant); 两枚实参站点各自必须含 isByVal 与 cParamLateSlotType
#       (ByVal 那一档) 与 bridgeByRefLateSlotArg (ByRef 那一档) —— 两边少一个就会互相吃进来;
#       Set 那一站必须还问 lateBoundSlotTarget 并给登记表写 boxedHere。
#   B6  夹具与两架构的真跑格都在: tests/b187_box/B187d.vbp 与 B187Arg.cls (第四臂 = 实参那一站)、
#       B187Ref.cls (第五臂 = ByRef 那座桥)、B187Name.cls (第六臂 = TypeName 认包装器) 存在,
#       run_tests.ps1 里 b187_boxed_object_slot 恰好 2 处 (x64 + x86)、CREATE4MEM / CREATE5MEM
#       / CREATE6MEM 各恰好 4 处 (每臂各一条正针 + 一条反面证人)。
#   B7  ByRef 那座桥的**两句发码只许住在一处** (第五腿): `bridgeByRefLateSlotArg(` 恰好 4 处 =
#       声明 1 + 定义 1 + 站点 2; 进场临时量名 `_vb6_latebox` 与剥回临时量名 `_vb6_lateunbox`
#       各恰好 1 处且都在定义体内; 剥回那一腿的 `c_.addPostLine` 在本文件里恰好 1 处 (别处再挂
#       一条就是第二条退路)。「这枚**值**是不是包装器」= vb6_ComObject_IsBoxed: src/backend 恰好
#       2 处 (桥的两端各一, 都在定义体内) 且 src/rtl 恰好 3 处 (声明 1 + 定义 1 + 类名那一问 1,
#       见 B8) —— 多一处就是有人绕过 deref 三闸自己去读首字段 (GetInstance 那条 0x20404 崩记录的同族)。
#   B9  「一枚对象变量不指向任何对象」那一问只许答一处 (§B189 第二格, 第八十七刀):
#       vb6_BSTR_FromStr(L"Nothing") 在 src/rtl 恰好 1 处, 且它那一行必须排在 vb6_Host_TypeNameOf
#       之前 (NULL 不许递给宿主那一路); 标表里 case vb6_vtEmpty 仍必须恰好答 L"Empty" 一次
#       —— 从没赋值的 Variant 那一形本刀不许顺手动。
#   B8  「这枚包装器是哪一枚工程类」(§B189 第一档, 第八十六刀) 只许一处定义、一处消费:
#       vb6_ComObject_ClassNameBStr 在 src/rtl 恰好 4 处 = 出口声明 1 + 定义 1 + 消费方 extern 1
#       + 调用 1; 在 src/backend 必须 0 处 —— 类名是运行期的值, 发码侧不许自己再拼这一问 (那等于把
#       那张 512 格反查表重新拉来当运行期读者)。且 vb6rtl_conv.c 里它那一行必须排在
#       vb6_Host_TypeNameOf **之后**: 宿主 (窗体 / 控件) 那一问在前是硬口径, 倒了就把 tests/setobj
#       钉着的 TextBox / CheckBox 两枚针改错。
#   B10 「这枚声明档的类名在本工程里被当接口用过没有」(§B189 第二档, 第八十八刀) 一处定义、一处
#       消费、一处发射, 且闸门必须排在发射之前: classUsedAsInterface 在 src/backend 恰好 2 处
#       (出口定义 1 + 折叠那一问 1)、src/driver 与 src/rtl 必须 0 处 (这问的是"本工程里有没有人
#       Implements 它", 只许发码侧问一次); 那张名单 projImplementedNames_ 在 src/backend 恰好 4 处
#       (字段 1 + 注入 1 + 两道闸 2), setProjectImplementedNames 全 src 恰好 2 处 (定义 + driver 注入),
#       driver 侧 projectImplementedNames 恰好 3 处 (声明 + insert + 注入实参), 且取数腿只问语义层
#       那一份 implementsNames (本文件里恰好 2 处: 注释那一问 + 循环那一问 —— 重扫 AST 就是第二份权威)。
#       发射点 '!= NULL) ? vb6_BSTR_FromStr(L' 在 src/backend 恰好 1 处, 且它那一行必须排在闸门之后
#       (先问视图、后折名字, 倒了就是把接口名当对象名)。控件那一路的两枚文本闸 (vb6_hwnd_ /
#       vb6_form_ctrl_) 必须都还在: 控件的答案属于宿主那一问, 不属声明档。第四道闸 = 实参是整枚槽
#       ⇒ 那是运行期的问句, 只许问 §B147 那条权威 variantOperandAST (折叠那一问的文件里恰好 1 处),
#       不许自己抄 isDefinitelyVariantExpr (0 处) —— 实物是 A/B 上唯一一处真语料移动:
#       VBFlexGrid.ctl 的 `ByVal Value As Variant` 被另一过程同名的 `As IVBFlexDataSource` 漏过来折成了
#       常量, 把 `TypeName(Value) = "ImageList"` 那一支变成恒假 (行为改动, 不是外观改动)。
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
$argStn = "src\backend\detail\expr\cgen_expr_call_arg_emit.inc"
$namedStn = "src\backend\detail\expr\cgen_expr_call_named_args.inc"

# B1
$nBorrow = Count-InFiles "src\backend" $srcExts "vb6_ComObject_FromBorrowedInstance"
$nEmit = @([regex]::Matches((Text-Of $boxRel), 'return "\(void\*\)vb6_ComObject_FromBorrowedInstance')).Count
$nBoxFn = Count-InFiles "src\backend" $srcExts "boxIntoLateSlot\s*\("
$nSlotParam = Count-InFiles "src\backend" $srcExts "cParamLateSlotType\s*\("
if ($nBorrow -ne 5) { $script:bad += ("B1 FromBorrowedInstance 在 src/backend 代码行 = $nBorrow (要 5: 发码处 + ByRef 桥进场 + 站点前缀判据 + getter 2)") }
if ($nEmit -ne 1) { $script:bad += ("B1 发出包装器那一句 = $nEmit (要 1, 只许住在唯一发码处)") }
if ($nBoxFn -ne 5) { $script:bad += ("B1 boxIntoLateSlot( = $nBoxFn (要 5: 声明 1 + 定义 1 + 站点 3)") }
if ($nSlotParam -ne 5) { $script:bad += ("B1 cParamLateSlotType( = $nSlotParam (要 5: 声明 1 + 定义 1 + ByVal 站点 2 + ByRef 桥 1)") }

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
foreach ($stn in @($argStn, $namedStn)) {
    $txt = Text-Of $stn
    foreach ($need in @("isByVal", "cParamLateSlotType", "bridgeByRefLateSlotArg")) {
        $c = @([regex]::Matches($txt, [regex]::Escape($need))).Count
        if ($c -lt 1) { $script:bad += ("B5 实参站点 " + $stn + " 缺闸门件 " + $need) }
    }
}
$stTxt = Text-Of $stationRel
foreach ($need in @("lateBoundSlotTarget", "boxedHere")) {
    $c = @([regex]::Matches($stTxt, [regex]::Escape($need))).Count
    if ($c -lt 1) { $script:bad += ("B5 Set 那一站缺 " + $need) }
}

# B7 — ByRef 那座桥
$nBridge = Count-InFiles "src\backend" $srcExts "bridgeByRefLateSlotArg\s*\("
if ($nBridge -ne 4) { $script:bad += ("B7 bridgeByRefLateSlotArg( = $nBridge (要 4: 声明 1 + 定义 1 + 站点 2)") }
$nBoxTmp = Count-InFiles "src" $srcExts "_vb6_latebox"
$nUnbTmp = Count-InFiles "src" $srcExts "_vb6_lateunbox"
$nBoxTmpDef = @([regex]::Matches($boxTxt, "_vb6_latebox")).Count
$nUnbTmpDef = @([regex]::Matches($boxTxt, "_vb6_lateunbox")).Count
if ($nBoxTmp -ne 1 -or $nBoxTmpDef -ne 1) {
    $script:bad += ("B7 进场临时量名 全 src/定义内 = $nBoxTmp/$nBoxTmpDef (要 1/1: 两句发码只许住在桥里)")
}
if ($nUnbTmp -ne 1 -or $nUnbTmpDef -ne 1) {
    $script:bad += ("B7 剥回临时量名 全 src/定义内 = $nUnbTmp/$nUnbTmpDef (要 1/1)")
}
$nPost = @([regex]::Matches($boxTxt, "c_\.addPostLine")).Count
if ($nPost -ne 1) { $script:bad += ("B7 桥里的 addPostLine = $nPost (要 1: 剥回那一腿只许一条退路)") }
$nIsBoxed = Count-InFiles "src\backend" $srcExts "vb6_ComObject_IsBoxed"
$nIsBoxedRtl = Count-InFiles "src\rtl" $srcExts "vb6_ComObject_IsBoxed"
if ($nIsBoxed -ne 2) { $script:bad += ("B7 src/backend 问 vb6_ComObject_IsBoxed = $nIsBoxed (要 2: 桥的两端各一)") }
if ($nIsBoxedRtl -ne 3) { $script:bad += ("B7 src/rtl 的 vb6_ComObject_IsBoxed = $nIsBoxedRtl (要 3: 声明 1 + 定义 1 + 类名那一问 1)") }

# B6
if (-not (Test-Path -LiteralPath (Join-Path $root "tests\b187_box\B187d.vbp"))) {
    $script:bad += "B6 缺夹具 tests/b187_box/B187d.vbp"
}
if (-not (Test-Path -LiteralPath (Join-Path $root "tests\b187_box\B187Arg.cls"))) {
    $script:bad += "B6 缺第四臂夹具 tests/b187_box/B187Arg.cls (实参那一站)"
}
if (-not (Test-Path -LiteralPath (Join-Path $root "tests\b187_box\B187Ref.cls"))) {
    $script:bad += "B6 缺第五臂夹具 tests/b187_box/B187Ref.cls (ByRef 那座桥)"
}
if (-not (Test-Path -LiteralPath (Join-Path $root "tests\b187_box\B187Name.cls"))) {
    $script:bad += "B6 缺第六臂夹具 tests/b187_box/B187Name.cls (TypeName 认包装器)"
}
if (-not (Test-Path -LiteralPath (Join-Path $root "tests\b187_box\B187Nil.cls"))) {
    $script:bad += "B6 缺第七臂夹具 tests/b187_box/B187Nil.cls (TypeName 对 Nothing 那一形)"
}
$rtTxt = Text-Of "tests\run_tests.ps1"
$nCells = @([regex]::Matches($rtTxt, "b187_boxed_object_slot")).Count
if ($nCells -ne 2) { $script:bad += ("B6 b187_boxed_object_slot 在 run_tests.ps1 里 = $nCells (要 2: x64 + x86)") }
$nArm4 = @([regex]::Matches($rtTxt, "CREATE4MEM")).Count
if ($nArm4 -ne 4) { $script:bad += ("B6 CREATE4MEM 在 run_tests.ps1 里 = $nArm4 (要 4: 两架构各一正针 + 一反面证人)") }
$nArm5 = @([regex]::Matches($rtTxt, "CREATE5MEM")).Count
if ($nArm5 -ne 4) { $script:bad += ("B6 CREATE5MEM 在 run_tests.ps1 里 = $nArm5 (要 4: 两架构各一正针 + 一反面证人)") }
$nArm6 = @([regex]::Matches($rtTxt, "CREATE6MEM")).Count
if ($nArm6 -ne 4) { $script:bad += ("B6 CREATE6MEM 在 run_tests.ps1 里 = $nArm6 (要 4: 两架构各一正针 + 一反面证人)") }
$nArm7 = @([regex]::Matches($rtTxt, "CREATE7MEM")).Count
if ($nArm7 -ne 4) { $script:bad += ("B6 CREATE7MEM 在 run_tests.ps1 里 = $nArm7 (要 4: 两架构各一正针 + 一反面证人)") }
$nArm8 = @([regex]::Matches($rtTxt, "CREATE8MEM")).Count
if ($nArm8 -ne 4) { $script:bad += ("B6 CREATE8MEM 在 run_tests.ps1 里 = $nArm8 (要 4: 两架构各一正针 + 一反面证人)") }
if (-not (Test-Path -LiteralPath (Join-Path $root "tests\b187_box\B187Decl.cls"))) {
    $script:bad += "B6 缺第八臂夹具 tests/b187_box/B187Decl.cls (TypeName 按声明档折叠)"
}
if (-not (Test-Path -LiteralPath (Join-Path $root "tests\b189_gate\B189g.vbp"))) {
    $script:bad += "B6 缺闸门夹具 tests/b189_gate/B189g.vbp (折叠那问的接口视图那一头, 只发码不建)"
}
$nGateCells = @([regex]::Matches($rtTxt, "b189_gate_")).Count
if ($nGateCells -ne 3) { $script:bad += ("B6 b189_gate_ 那三枚发码判据 = $nGateCells (要 3: 视图反针 + 实体正针 + 同名槽那一双)") }

# B8 — TypeName 认自家包装器 (§B189 第一档): 一处出口、一处消费, 且排在宿主那一问之后
$convRel = "src\rtl\core\vb6rtl\vb6rtl_conv.c"
$nClsRtl = Count-InFiles "src\rtl" $srcExts "vb6_ComObject_ClassNameBStr"
$nClsBackend = Count-InFiles "src\backend" $srcExts "vb6_ComObject_ClassNameBStr"
if ($nClsRtl -ne 4) {
    $script:bad += ("B8 vb6_ComObject_ClassNameBStr 在 src/rtl = $nClsRtl (要 4: 出口声明 1 + 定义 1 + extern 1 + 调用 1)")
}
if ($nClsBackend -ne 0) { $script:bad += ("B8 发码侧自己拼了类名那一问 = $nClsBackend (要 0: 类名是运行期的值)") }
$convLines = (Text-Of $convRel) -split "`r?`n"
$iHost = -1
$iCls = -1
for ($i = 0; $i -lt $convLines.Count; $i++) {
    $t = $convLines[$i].TrimStart()
    if ($t.StartsWith("//") -or $t.StartsWith("*")) { continue }
    if ($iHost -lt 0 -and $convLines[$i].Contains("vb6_Host_TypeNameOf")) { $iHost = $i }
    if ($iCls -lt 0 -and $convLines[$i].Contains("vb6_ComObject_ClassNameBStr(v.pdispVal)")) { $iCls = $i }
}
if ($iHost -lt 0) { $script:bad += "B8 找不到宿主那一问 vb6_Host_TypeNameOf (消费点被搬走了)" }
if ($iCls -lt 0) { $script:bad += "B8 找不到类名那一问的调用点 (消费点被搬走了)" }
if ($iHost -ge 0 -and $iCls -ge 0 -and $iHost -gt $iCls) {
    $script:bad += ("B8 类名那一问排到了宿主那一问之前 (类名行 $iCls < 宿主行 $iHost) —— 控件那一档会被改错")
}

# B9 — TypeName 对「一枚不指向对象的对象变量」只答一处，且 NULL 不进宿主那一路
$nilRel = "src\rtl\core\vb6rtl\vb6rtl_conv.c"
$nNothing = Count-InFiles "src\rtl" $srcExts 'vb6_BSTR_FromStr\(L"Nothing"'
$nilTxt = Text-Of $nilRel
$nEmptyArm = @([regex]::Matches($nilTxt, 'case vb6_vtEmpty:\s+return L"Empty";')).Count
$iNil = -1
$iHost9 = -1
$nilLines = $nilTxt -split "`r?`n"
for ($i = 0; $i -lt $nilLines.Count; $i++) {
    $t = $nilLines[$i].TrimStart()
    if ($t.StartsWith("//") -or $t.StartsWith("*")) { continue }
    if ($iNil -lt 0 -and $nilLines[$i].Contains('vb6_BSTR_FromStr(L"Nothing")')) { $iNil = $i }
    if ($iHost9 -lt 0 -and $nilLines[$i].Contains("vb6_Host_TypeNameOf")) { $iHost9 = $i }
}
if ($nNothing -ne 1) { $script:bad += ("B9 src/rtl 答 Nothing 的地方 = $nNothing 处 (要 1: 那一问只许住在 vb6_TypeName 这一支)") }
if ($iNil -lt 0) { $script:bad += "B9 找不到 Nothing 那一支 (被搬走了还是改了字面量?)" }
if ($iHost9 -ge 0 -and $iNil -ge 0 -and $iNil -gt $iHost9) {
    $script:bad += ("B9 Nothing 那一支排到了宿主那一问之后 (NULL 行 $iNil > 宿主行 $iHost9) —— 会把 NULL 递给宿主那一路")
}
if ($nEmptyArm -ne 1) { $script:bad += ("B9 标表里 case vb6_vtEmpty 那一行 = $nEmptyArm (要 1: 从没赋值的 Variant 照旧答 Empty)") }

# B10 — 「这枚声明档的类名在本工程里被当接口用过没有」(§B189 第二档, 第八十八刀):
#        一处定义、一处消费、一处发射, 且闸门必须排在发射之前。
#        折叠那一支是**编译期按声明折**的, 所以它有两个天生要钉死的地方: ① 答案的名字来自声明档
#        (不是写死的一枚) —— 靠夹具 tx As B187Name 那一形钉行为, 这里钉"发码点只许一处";
#        ② 接口视图那一头不许折 (VB6 答实体真身, 按声明折就是把接口名当对象名) —— 靠那张
#        工程级 implements 名单, 名单只许语义层那一份做权威。
$foldRel = "src\backend\detail\expr\cgen_expr_call_builtin_pre.inc"
$nGateBackend = Count-InFiles "src\backend" $srcExts "classUsedAsInterface"
$nGateOther = (Count-InFiles "src\driver" $srcExts "classUsedAsInterface") + (Count-InFiles "src\rtl" $srcExts "classUsedAsInterface")
if ($nGateBackend -ne 2) { $script:bad += ("B10 classUsedAsInterface 在 src/backend = $nGateBackend (要 2: 出口定义 1 + 折叠那一问 1)") }
if ($nGateOther -ne 0) { $script:bad += ("B10 发码侧之外有人自己抄了接口视图那一问 = $nGateOther (要 0)") }
$nGateState = Count-InFiles "src\backend" $srcExts "projImplementedNames_"
if ($nGateState -ne 4) { $script:bad += ("B10 projImplementedNames_ 在 src/backend = $nGateState (要 4: 字段 1 + 注入 1 + 两道闸 2)") }
$nGateInject = Count-InFiles "src" $srcExts "setProjectImplementedNames"
if ($nGateInject -ne 2) { $script:bad += ("B10 setProjectImplementedNames 全 src = $nGateInject (要 2: 出口定义 1 + driver 注入 1)") }
$nGateScan = Count-InFiles "src\driver" $srcExts "projectImplementedNames"
if ($nGateScan -ne 3) { $script:bad += ("B10 driver 侧 projectImplementedNames = $nGateScan (要 3: 名单声明 1 + insert 1 + 注入实参 1)") }
$nGateSemantic = @([regex]::Matches((Text-Of "src\driver\detail\driver_codegen_voidfield_scan.inc"), "implementsNames")).Count
if ($nGateSemantic -ne 2) { $script:bad += ("B10 那份名单的取数腿问语义层 implementsNames = $nGateSemantic 处 (要 2: 注释里那一问 + 循环那一问; 只认这一份权威, 不许重扫 AST)") }
$nTernary = Count-InFiles "src\backend" $srcExts '!= NULL\) \? vb6_BSTR_FromStr\(L'
if ($nTernary -ne 1) { $script:bad += ("B10 折叠那支的发射点 '!= NULL) ? vb6_BSTR_FromStr(L' 在 src/backend = $nTernary (要 1: 一处发射, 别处再拼一遍就是第二份答案源)") }
$foldLines = (Text-Of $foldRel) -split "`r?`n"
$iGate10 = -1
$iTern10 = -1
for ($i = 0; $i -lt $foldLines.Count; $i++) {
    $t = $foldLines[$i].TrimStart()
    if ($t.StartsWith("//") -or $t.StartsWith("*")) { continue }
    if ($iGate10 -lt 0 -and $foldLines[$i].Contains("classUsedAsInterface(")) { $iGate10 = $i }
    if ($iTern10 -lt 0 -and $foldLines[$i].Contains('!= NULL) ? vb6_BSTR_FromStr(L')) { $iTern10 = $i }
}
if ($iGate10 -lt 0) { $script:bad += "B10 折叠那一问找不到闸门 (被撤了?)" }
if ($iGate10 -ge 0 -and $iTern10 -le $iGate10) {
    $script:bad += ("B10 闸门排到了发射之后/同处 (闸门行 $iGate10, 发射行 $iTern10) —— 视图那一头会被折")
}
foreach ($guard in @("vb6_hwnd_", "vb6_form_ctrl_")) {
    $c = @([regex]::Matches((Text-Of $foldRel), [regex]::Escape($guard))).Count
    if ($c -lt 1) { $script:bad += ("B10 折叠那一问缺控件那一路的文本闸 " + $guard + " (控件的答案属于宿主那一问)") }
}
# 第四道闸: 实参是整枚槽 ⇒ 运行期的问句, 不许按声明档折。这一问只许问那一条权威 (§B147 收成的
# variantOperandAST), 在本文件里恰好 1 处; 直接抄 isDefinitelyVariantExpr 就是第二份判据, 0 处。
$foldText = Text-Of $foldRel
$nSlotAsk = @([regex]::Matches($foldText, "variantOperandAST")).Count
if ($nSlotAsk -ne 1) { $script:bad += ("B10 折叠那一问问槽的权威 variantOperandAST = $nSlotAsk 处 (要 1: 那一问只许走 §B147 那一条)") }
$nSlotCopy = @([regex]::Matches($foldText, "isDefinitelyVariantExpr")).Count
if ($nSlotCopy -ne 0) { $script:bad += ("B10 折叠那一问自己抄了槽判据 isDefinitelyVariantExpr = $nSlotCopy 处 (要 0: 权威是 variantOperandAST)") }

if ($script:bad.Count -eq 0) {
    Write-Host ("PASS boxing-emits=1 stations=3 borrowed-refs=5 coclass-predicate-sites=" + $nUse `
        + " wrapper-ask-sites=" + $nAsk + " registry-ins=" + $nIns + " erase=" + $nErase `
        + " bridge-sites=" + $nBridge + " boxed-value-asks=" + $nIsBoxed + "/" + $nIsBoxedRtl `
        + " cells=" + $nCells + " arm4=" + $nArm4 + " arm5=" + $nArm5 + " arm6=" + $nArm6 `
        + " classname-rtl=" + $nClsRtl + " classname-backend=" + $nClsBackend + " order=" + $iHost + "<" + $iCls `
        + " arm7=" + $nArm7 + " nothing-site=" + $nNothing + "@line" + $iNil + " empty-arm=" + $nEmptyArm `
        + " gate-sites=" + $nGateBackend + "/" + $nGateOther + " gate-table=" + $nGateState + "/" + $nGateInject + "/" + $nGateScan `
        + " fold-emit=" + $nTernary + " order=" + $iGate10 + "<" + $iTern10 `
        + " arm8=" + $nArm8 + " gate-cells=" + $nGateCells) -ForegroundColor Green
    exit 0
}
foreach ($b in $script:bad) { Write-Host ("FAIL " + $b) -ForegroundColor Red }
exit 1
