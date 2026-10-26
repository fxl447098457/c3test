# check_condition_truth_authority.ps1 - 「把一条 C 表达式折成 VB6 的真值测试」只有一位主人
#   (账 #305 第二百四十四刀)
#
# 起因 (读数在第二百四十三刀那一行): 晚绑定调用 vb6_ComCall(...) 交回的是**堆上一枚 VARIANT 的
#   指针** (C 侧 void*)。条件那一族只有一处包装站 (cgen_stmt.cpp 里补 vb6_VariantToBool 那段)，
#   而它问的是「这串 C 是不是槽」(cExprIsVariant) —— 指针不是槽 ⇒ 答否 ⇒ 一行都不包 ⇒
#   if (ptr) 恒真。真语料按产物数到 82 处 / 27 份文件，最响的一族是 `Do While Not rs.EOF`
#   (恒真 = 死循环)；`Loop While <假条件>` 实测跑满保险丝 (n=4 / VB6 该 1)。
#
# 规则（改坏了会红）：
#   R1  RTL 出口 vb6_ComResultToBool: 定义 1 + 声明 1；体内三问各 1，且**先读值、后收回**
#       (顺序倒了 = 读一枚已清空的槽)。
#   R2  唯一主人 conditionTruthExpr: 定义 1 (cgen_util_type.cpp) + 声明 1 (cgen_helpers.inc)
#       + 站点 7 (If / ElseIf / Do While / Do Until / Loop While / Loop Until / While Wend)。
#   R3  「这发是不是裸晚绑定调用的结果」= cExprIsBareComResult: 定义 1 + 声明 1 + 只在主人里问 1 次；
#       名单恰好四条 (ComCall / ComCallByDispid / ComGetProp / ComGetObjectProp)，且**不许**混进
#       带类型的出口 (ComCallInt/BSTR/Double/Object、ComGetStringProp) —— 那些交回的已是具体 C 档。
#   R4  站点不许自己再问「是不是槽」: cgen_stmt.cpp 与 cgen_loop.cpp 里 cExprIsVariant(lastExpr_) 各 0。
#   R5  判据面: 夹具存在 + runner 里 7 枚正针 (带 ComResultToBool) + 14 枚键位 (正/反各 7) + 登记两枚针。
#
# 用法: powershell -File scripts\check_condition_truth_authority.ps1
# 退出码: 0 = 全绿; 1 = 红

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
if ($env:C3_CHECK_ROOT) { $root = $env:C3_CHECK_ROOT }

$rtlC  = Join-Path $root 'src\rtl\core\vb6rtl\vb6rtl_com.c'
$rtlH  = Join-Path $root 'src\rtl\core\vb6rtl\vb6rtl_class_com.h'
$typeC = Join-Path $root 'src\backend\cgen_util_type.cpp'
$helpers = Join-Path $root 'src\backend\detail\util\cgen_helpers.inc'
$ifC   = Join-Path $root 'src\backend\stmt\cgen_stmt.cpp'
$loopC = Join-Path $root 'src\backend\stmt\cgen_loop.cpp'
$harness = Join-Path $root 'tests\run_tests.ps1'
$fix     = Join-Path $root 'tests\test_late_cond.bas'

foreach ($p in @($rtlC, $rtlH, $typeC, $helpers, $ifC, $loopC, $harness, $fix)) {
    if (-not (Test-Path -LiteralPath $p)) { Write-Host ('FAIL missing ' + $p) -ForegroundColor Red; exit 1 }
}

function Count-Pattern($path, $pattern) {
    $t = [System.IO.File]::ReadAllText($path)
    return ([regex]::Matches($t, $pattern)).Count
}

$fails = @()
function Expect($tag, $got, $want, $note) {
    if ($got -ne $want) {
        $script:fails += ('{0} = {1} (want {2}) -- {3}' -f $tag, $got, $want, $note)
        Write-Host ('FAIL {0} = {1}, want {2}  ({3})' -f $tag, $got, $want, $note) -ForegroundColor Red
    } else {
        Write-Host ('ok   {0} = {1}' -f $tag, $got) -ForegroundColor Green
    }
}

# R1 —— RTL 出口
Expect 'R1 vb6_ComResultToBool defined'   (Count-Pattern $rtlC '(?m)^int16_t vb6_ComResultToBool\(void\* variant_ptr\) \{') 1 '定义只许一枚, 在 vb6rtl_com.c'
Expect 'R1 vb6_ComResultToBool declared'  (Count-Pattern $rtlH 'int16_t vb6_ComResultToBool\(void\* variant_ptr\);') 1 '声明只许一枚, 在 vb6rtl_class_com.h'
Expect 'R1 consumes the heap VARIANT'     (Count-Pattern $rtlC 'vb6_VariantFromComResult\(variant_ptr\)') 1 '那一枚堆块的所有权必须交出去'
Expect 'R1 reads via VariantToBool'       (Count-Pattern $rtlC 'vb6_VariantToBool\(v\)') 1 '真值口径问既有的那位主人'
Expect 'R1 clears the copy'               (Count-Pattern $rtlC 'vb6_VariantClear\(&v\)') 1 '深拷出来的槽要收回 (BSTR/对象/数组那几档有载荷)'
$rtlT = [System.IO.File]::ReadAllText($rtlC)
Expect 'R1 read-before-clear' ([int]($rtlT.IndexOf('vb6_VariantToBool(v)') -lt $rtlT.IndexOf('vb6_VariantClear(&v)'))) 1 '顺序倒了 = 读一枚已清空的槽'

# R2 —— 唯一主人与七站
Expect 'R2 conditionTruthExpr defined'   (Count-Pattern $typeC '(?m)^std::string CCodeGen::conditionTruthExpr\(') 1 '定义只许一枚, 在 cgen_util_type.cpp'
Expect 'R2 conditionTruthExpr declared'  (Count-Pattern $helpers 'std::string conditionTruthExpr\(') 1 '声明在 cgen_helpers.inc'
Expect 'R2 If 那一站'                    (Count-Pattern $ifC   'conditionTruthExpr\(') 2 'If 顶层 + ElseIf (ElseIf 从前连这一问都没问)'
Expect 'R2 循环那五站'                   (Count-Pattern $loopC 'conditionTruthExpr\(') 5 'Do While / Do Until / Loop While / Loop Until / While Wend'

# R3 —— 「裸晚绑定调用」那一问
Expect 'R3 cExprIsBareComResult defined'   (Count-Pattern $typeC '(?m)^bool CCodeGen::cExprIsBareComResult\(') 1 '定义只许一枚'
Expect 'R3 cExprIsBareComResult declared'  (Count-Pattern $helpers 'bool cExprIsBareComResult\(') 1 '声明在 cgen_helpers.inc'
Expect 'R3 asked only by the owner'        (Count-Pattern $typeC 'cExprIsBareComResult\(cExpr\)') 1 '只许主人在条件那一站问它'
foreach ($n in @('vb6_ComCall\(', 'vb6_ComCallByDispid\(', 'vb6_ComGetProp\(', 'vb6_ComGetObjectProp\(')) {
    Expect ('R3 prefix ' + $n) (Count-Pattern $typeC ('"' + $n + '"')) 1 '名单里恰好一条'
}
foreach ($bad in @('vb6_ComCallInt', 'vb6_ComCallBSTR', 'vb6_ComCallDouble', 'vb6_ComCallObject', 'vb6_ComGetStringProp')) {
    Expect ('R3 forbidden typed exit ' + $bad) (Count-Pattern $typeC $bad) 0 '带类型的出口交回的已是具体 C 档, 混进来就是二次解包'
}

# R4 —— 站点不许自己再问「是不是槽」
Expect 'R4 If 站不再自问'   (Count-Pattern $ifC   'cExprIsVariant\(lastExpr_\)') 0 '那一问归主人'
Expect 'R4 循环站不再自问' (Count-Pattern $loopC 'cExprIsVariant\(lastExpr_\)') 0 '那一问归主人'

# R5 —— 判据面
$h = [System.IO.File]::ReadAllText($harness)
Expect 'R5 shape needles'  ([regex]::Matches($h, 'vb6_ComResultToBool')).Count 7 '七站各一枚正针'
Expect 'R5 keyed needles'  ([regex]::Matches($h, 'L"K-')).Count 14 '七枚键位 = 正针 7 + 反面针 7 (数 L" 那一层, 别撞上别的夹具)'
Expect 'R5 bas-test registered' ([regex]::Matches($h, 'Add-BasTest "test_late_cond"')).Count 1 '真跑夹具登记一处'
Expect 'R5 runtime needles' ([regex]::Matches($h, 'COND:12/12')).Count 1 '十二臂全对的总结针'

if ($fails.Count -gt 0) {
    Write-Host ('RED: ' + $fails.Count + ' guard(s) broken') -ForegroundColor Red
    exit 1
}
Write-Host 'PASS: 条件那一读只有一位主人 (七站同问, 裸晚绑定调用走 vb6_ComResultToBool)' -ForegroundColor Green
exit 0
