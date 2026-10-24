# check_iface_view_authority.ps1 - 账 #305 §B221/#449（§B222）：「这一枚表达式交出来的是视图还是实例」
# 这一问只许有一位主人。七条钉：
#   ① 视图的成员调用发射形状 `->vt-><slot>(x, args)` 在全 src/backend 恰好 **1** 处，
#      且必须住在 module/cgen_iface_vtbl.cpp 的 ivrefVtableCall 里（com_bind 与「接收者是
#      视图值的调用」那一站都调它）。长出第 2 处 = 同一形状两位作者。
#   ② 问句 ifaceViewOfExpr 的**消费文件**恰好 **4** 份：Set 目标侧 / 实参侧 /
#      成员访问接收者 / 带实参调用的接收者。少一份 = 有一站回到各写一遍；多一份 = 判据被复制。
#   ③ 直接拿字面量 "vb6_ivref_" 拼类型/问档的文件恰好 **6** 份（今天在册的名单）。
#      第 7 份长出来就意味着「是不是接口」又有了第二位主人。
#   ④ 「Set 的槽位交给哪一位主人」那一问 (第二百零三刀) 两站同问:
#      cgen_setlet_set_prop.inc (置空腿③) 与 cgen_setlet_set_rhs.inc (覆写腿②) **各自**至少
#      问一次 ivrefCType(<声明档>)。少一站 = 那一站照旧铸 `vb6_cls_<接口名>_AddRef/_Release`
#      = 链接期 LNK2019 (第二百零三刀实测: 只补覆写那一站时, 夹具红在 `Set me->m_iv = Nothing`)。
#   ⑤ 视图计数主人 emitIvrefRefSet 的**调用点**恰好 **4** 处, 且只在那两份 Set 站点文件里
#      (按名 B05 / 置空 / 字段覆写 / 字段置空兜底)。长出第 5 处或换文件 = 同一发计数两位作者。
#   ⑥ 「一枚工程类实例怎么交进视图槽」那一发形状 (`&(<实例>)->__iv_<I>`, 第二百零四刀) 有**一位**
#      作者 = ivrefInstanceViewExpr；它的**消费点恰好 3 处**、分居三份文件：
#      Set 覆写腿② / Set 按名 B04 / 实参交付侧 deliverToClassSlot。少一处 = 那一站回到自己拼；
#      多一处 = 同一形状第二位作者。
#   ⑦ 字面量 `"__iv_" + cIdent(` 那份拼法只许住在两份文件里 = 主人自己 + ByVal 实参适配器
#      (那一发带 `(vb6_ivref_I*)` 强转、形状不同，刻意留在册)。第 3 份长出来 = 又一处现拼视图槽字段。
# 退出码：0 = 七条都过；1 = 任一条红。
param(
    [string]$Root = ""
)
$ErrorActionPreference = 'Continue'
if (-not $Root) { $Root = (Split-Path -Parent $PSScriptRoot) }
$backend = Join-Path $Root "src\backend"
if (-not (Test-Path $backend)) {
    Write-Host ("MISSING " + $backend)
    exit 2
}
$files = Get-ChildItem -Path $backend -Recurse -File |
    Where-Object { $_.Extension -match '^\.(cpp|inc|h|hpp)$' }

$fail = @()

# ① vtable 发射形状唯一作者
$vt = @($files | Select-String -Pattern '"->vt->"')
if ($vt.Count -ne 1) {
    $fail += ("vt-call sites = " + $vt.Count + " (want exactly 1)")
} elseif ($vt[0].Path -notmatch 'cgen_iface_vtbl\.cpp$') {
    $fail += ("vt-call formula moved into " + $vt[0].Path)
}

# ② ifaceViewOfExpr 的消费文件（去掉它自己的定义与声明）
$ivAll = @($files | Select-String -Pattern 'ifaceViewOfExpr')
$ivFiles = @($ivAll | ForEach-Object { Split-Path -Leaf $_.Path } | Sort-Object -Unique |
    Where-Object { $_ -ne 'cgen_util_classtype.cpp' -and $_ -ne 'cgen_helpers.inc' })
if ($ivFiles.Count -ne 4) {
    $fail += ("ifaceViewOfExpr consumer files = " + $ivFiles.Count +
              " (want exactly 4): " + ($ivFiles -join ', '))
}

# ③ 字面量 "vb6_ivref_" 的主人名单
$litFiles = @($files | Select-String -Pattern '"vb6_ivref_"' |
    ForEach-Object { Split-Path -Leaf $_.Path } | Sort-Object -Unique)
if ($litFiles.Count -ne 6) {
    $fail += ("vb6_ivref_ literal files = " + $litFiles.Count +
              " (want exactly 6): " + ($litFiles -join ', '))
}

# ④ Set 两站同问: 置空腿③ 与 覆写腿② 各问一次「这格槽的声明档是不是新式接口视图」
$nullStation = Join-Path $backend "detail\stmt\cgen_setlet_set_prop.inc"
$overStation = Join-Path $backend "detail\stmt\cgen_setlet_set_rhs.inc"
foreach ($st in @(@{ F = $nullStation; P = 'ivrefCType\(targetCls101\)' },
                  @{ F = $overStation; P = 'ivrefCType\(clsOfSlot\)' })) {
    if (-not (Test-Path $st.F)) { $fail += ("station missing: " + $st.F); continue }
    $asked = @(Select-String -Path $st.F -Pattern $st.P)
    if ($asked.Count -lt 1) {
        $fail += ("station " + (Split-Path -Leaf $st.F) + " no longer asks " + $st.P +
                  " (= 那一格会再铸一枚 vb6_cls_<接口名>_AddRef/_Release, LNK2019)")
    }
}

# ⑤ 视图计数主人的调用点名单
$refSetCalls = @($files | Select-String -Pattern 'emitIvrefRefSet\(target')
if ($refSetCalls.Count -ne 4) {
    $fail += ("emitIvrefRefSet call sites = " + $refSetCalls.Count + " (want exactly 4)")
}
$refSetFiles = @($refSetCalls | ForEach-Object { Split-Path -Leaf $_.Path } | Sort-Object -Unique)
if (($refSetFiles -join ',') -ne 'cgen_setlet_set_prop.inc,cgen_setlet_set_rhs.inc') {
    $fail += ("emitIvrefRefSet consumers = " + ($refSetFiles -join ', ') +
              " (want only the two Set station files)")
}

# ⑥ 视图形状主人 ivrefInstanceViewExpr: 一位作者 + 三处消费 (Set 覆写 / Set 按名 / 实参交付侧)
$viewAll = @($files | Select-String -Pattern 'ivrefInstanceViewExpr\(')
$viewDef = @($viewAll | Where-Object { (Split-Path -Leaf $_.Path) -eq 'cgen_iface_vtbl.cpp' })
$viewDecl = @($viewAll | Where-Object { (Split-Path -Leaf $_.Path) -eq 'cgen_helpers.inc' })
$viewCalls = @($viewAll | Where-Object { $_ -notin $viewDef -and $_ -notin $viewDecl })
if ($viewDef.Count -ne 1) {
    $fail += ("ivrefInstanceViewExpr definition sites = " + $viewDef.Count + " (want exactly 1)")
}
if ($viewDecl.Count -ne 1) {
    $fail += ("ivrefInstanceViewExpr declarations = " + $viewDecl.Count + " (want exactly 1)")
}
if ($viewCalls.Count -ne 3) {
    $fail += ("ivrefInstanceViewExpr call sites = " + $viewCalls.Count + " (want exactly 3)")
}
$viewCallFiles = @($viewCalls | ForEach-Object { Split-Path -Leaf $_.Path } | Sort-Object -Unique)
if (($viewCallFiles -join ',') -ne 'cgen_base_type.cpp,cgen_setlet_set_prop.inc,cgen_setlet_set_rhs.inc') {
    $fail += ("view-shape consumers = " + ($viewCallFiles -join ', ') +
              " (want the two Set stations + the arg delivery side)")
}
# 交付侧那一站必须**经主人**拿这一发 (自己内联拼 = 这一条掉下来)
$deliverVia = @(Select-String -Path (Join-Path $backend "cgen_base_type.cpp") `
                              -Pattern 'ivrefInstanceViewExpr\(')
if ($deliverVia.Count -lt 1) {
    $fail += ("deliverToClassSlot no longer routes the view shape through ivrefInstanceViewExpr" +
              " (= 实参站回到把实例基址当视图交出去, 实测 rc=0xC0000005)")
}

# ⑦ `"__iv_" + cIdent(` 那份拼法的住家名单
$shapeFiles = @($files | Select-String -Pattern '"__iv_" \+ cIdent' |
    ForEach-Object { Split-Path -Leaf $_.Path } | Sort-Object -Unique)
if (($shapeFiles -join ',') -ne 'cgen_expr_call_arg_emit.inc,cgen_iface_vtbl.cpp') {
    $fail += ("inline __iv_ spellings live in " + ($shapeFiles -join ', ') +
              " (want only the owner + the ByVal arg adapter)")
}

if ($fail.Count -gt 0) {
    Write-Host "IFACE-VIEW-AUTHORITY FAIL"
    foreach ($f in $fail) { Write-Host ("  - " + $f) }
    exit 1
}
Write-Host "IFACE-VIEW-AUTHORITY OK (1 formula site / 4 consumers / 6 literal files / 2 Set stations ask / 4 refSet calls / 3 view-shape consumers / 2 inline __iv_ files)"
exit 0
