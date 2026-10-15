# check_object_slot_boxing.ps1 - 账 #305 §B187 装箱腿的结构性哨兵 (只扫源码, 不起 cl)
#
# 为什么单开这一道: 「Set <泛对象槽> = <工程内实例> 该交什么」这一问改前的答案是"什么都不交,
# 直接发裸指针赋值" —— 于是槽里是一枚 `vb6_cls_X*`, 晚绑定那一族 (vb6_ComCall / 外部 COM 实参)
# 各自读它: 前者静默空答, 后者 0xC0000005。现在这一问只有一处回答 (cgen_setlet_set_rhs.inc
# 那一站的装箱臂), 而它要问两句「这枚类在不在运行期 coclass 表里」和「这枚变量此刻装的是不是
# 包装器」—— 那两句也各只有一处定义。哨兵钉的是这四件事不许再长出第二份。
#
# 规则 (改坏了会红, 不是装饰):
#   B1  vb6_ComObject_FromBorrowedInstance 在 src/backend 的代码行里恰好 3 处 = 装箱腿 1 (Set
#       站) + 字段访问器 2 (cgen_com.cpp 的 As New / typed 字段那两条 getter)。装箱腿只许一处。
#   B2  「哪一档的类在 coclass 表里占一行」这条判据只许住在 Symbol::isCoClassTableEntry:
#       全 src 里 `instancing == VBInstancing::MultiUse|SingleUse` 必须恰好 2 处且都在
#       symbol_table.hpp 的定义体内 (cgen_com_events.cpp / driver_codegen_dll_typelib.inc
#       那两处问的是 `== Private` 另一件事, 不归本条管)。parser 那唯一的写者
#       (mod.instancing = VBInstancing::MultiUse) 必须还在 1 处; 消费点 (->isCoClassTableEntry())
#       至少 3 处: 建表收集 / As New 那一支 / driver 那份工程级名单 —— 少一处就是有人又开始
#       手抄判据。
#   B3  「这枚 C 表达式此刻装着包装器吗」只有 varHoldsComWrapper 一处回答 (cgen_api.inc)。
#       直查 wrappedComVars_.count 的地方只许剩那条定义本身 (cgen_api.inc 里 1 处),
#       src/backend 别处必须 0 处 —— 179b 改问它之前那里是一处手抄。
#   B4  登记表只有一处写: `wrappedComVars_.insert` 恰好 1 处 —— 那一处同时负责注销
#       ("最后一次 Set 交的是不是包装器"), 别处再 insert 就会留下过期答案。
#   B5  装箱腿那一站的闸门齐件: lateBoundSlotTarget + classHasCoClassDesc +
#       句柄闸 (vb6_hwnd_ / vb6_form_ctrl_ 那一问) 必须都在 cgen_setlet_set_rhs.inc 里,
#       少一件就是漏了一路负条件。
#   B6  夹具与两架构的真跑格都在: tests/b187_box/B187d.vbp 存在, run_tests.ps1 里
#       b187_boxed_object_slot 恰好 2 处 (x64 + x86)。
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

# B1
$nBorrow = Count-InFiles "src\backend" $srcExts "vb6_ComObject_FromBorrowedInstance"
$stationRel = "src\backend\detail\stmt\cgen_setlet_set_rhs.inc"
$nStation = @([regex]::Matches((Text-Of $stationRel), "vb6_ComObject_FromBorrowedInstance")).Count
# 上面那一句数的是**全文** (含注释), 所以站点里是 2; 代码行单独再数一次才与 B1 同口径。
$nStationCode = Count-InFiles "src\backend\detail\stmt" $srcExts "vb6_ComObject_FromBorrowedInstance"
if ($nBorrow -ne 3) { $script:bad += ("B1 FromBorrowedInstance 在 src/backend 代码行 = $nBorrow (要 3)") }
if ($nStationCode -ne 1) { $script:bad += ("B1 装箱腿那一站 = $nStationCode 处 (要 1; 多一处就是又开了一站)") }

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

# B3 / B4
$nRawCount = Count-InFiles "src\backend" $srcExts "wrappedComVars_\.count" "src\backend\detail\util\cgen_api.inc"
if ($nRawCount -ne 0) { $script:bad += ("B3 别处直查 wrappedComVars_.count = $nRawCount (要 0, 一律问 varHoldsComWrapper)") }
$nDefCount = @([regex]::Matches((Text-Of "src\backend\detail\util\cgen_api.inc"), "wrappedComVars_\.count")).Count
if ($nDefCount -ne 1) { $script:bad += ("B3 varHoldsComWrapper 定义体内的 count = $nDefCount (要 1)") }
$nInsert = Count-InFiles "src\backend" $srcExts "wrappedComVars_\.insert"
if ($nInsert -ne 1) { $script:bad += ("B4 wrappedComVars_.insert = $nInsert (要 1, 登记表只许一处写)") }

# B5
$st = Text-Of $stationRel
foreach ($need in @("lateBoundSlotTarget", "classHasCoClassDesc", "vb6_hwnd_", "vb6_form_ctrl_")) {
    $c = @([regex]::Matches($st, [regex]::Escape($need))).Count
    if ($c -lt 1) { $script:bad += ("B5 装箱那一站缺负条件件 " + $need + " (= " + $c + ")") }
}

# B6
if (-not (Test-Path -LiteralPath (Join-Path $root "tests\b187_box\B187d.vbp"))) {
    $script:bad += "B6 缺夹具 tests/b187_box/B187d.vbp"
}
$nCells = @([regex]::Matches((Text-Of "tests\run_tests.ps1"), "b187_boxed_object_slot")).Count
if ($nCells -ne 2) { $script:bad += ("B6 b187_boxed_object_slot 在 run_tests.ps1 里 = $nCells (要 2: x64 + x86)") }

if ($script:bad.Count -eq 0) {
    Write-Host ("PASS boxing-station=1 borrowed-wrappers=3 coclass-predicate-sites=" + $nUse `
        + " registry-writes=" + $nInsert + " cells=" + $nCells) -ForegroundColor Green
    exit 0
}
foreach ($b in $script:bad) { Write-Host ("FAIL " + $b) -ForegroundColor Red }
exit 1
