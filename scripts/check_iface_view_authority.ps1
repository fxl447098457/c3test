# check_iface_view_authority.ps1 - 账 #305 §B221/#449（§B222）：「这一枚表达式交出来的是视图还是实例」
# 这一问只许有一位主人。九条钉：
#   ① 视图的成员调用发射形状 `->vt-><slot>(x, args)` 在全 src/backend 恰好 **1** 处，
#      且必须住在 module/cgen_iface_vtbl.cpp 的 ivrefVtableCall 里（com_bind 与「接收者是
#      视图值的调用」那一站都调它）。长出第 2 处 = 同一形状两位作者。
#   ② 问句 ifaceViewOfExpr 的**消费文件**恰好 **5** 份：Set 目标侧 / 实参侧 /
#      成员访问接收者（字段兜底臂）/ 成员访问接收者（类模块臂，第二百一十刀 S2）/
#      带实参调用的接收者。少一份 = 有一站回到各写一遍；多一份 = 判据被复制。
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
#      多一处 = 同一形状第二位作者。第二百零五刀给交付侧再接一档（借来的实例走 nullable 那一档）
#      ⇒ 消费点 3 → **4**，仍分居那三份文件。
#   ⑦ 字面量 `"__iv_" + cIdent(` 那份拼法只许住在两份文件里 = 主人自己 + ByVal 实参适配器
#      (那一发带 `(vb6_ivref_I*)` 强转、形状不同，刻意留在册)。第 3 份长出来 = 又一处现拼视图槽字段。
#   ⑧ 视图槽「谁付钱」那一发 AddRef (第二百零五刀给 copy-in 补上视图档) 的**发射点**在全 src/backend
#      恰好 **5** 处、只居这 3 份文件 = cgen_com.cpp (registerClassArgRefCopyIn 的视图臂，恰 1 处) +
#      cgen_iface_vtbl.cpp (emitIvrefRefSet) + cgen_setlet_set_prop.inc (仍在册的两处内联形状 + 置空前退旧)。
#      站点自己再拼一发 = 「谁付钱」长出第二位主人；这一条掉到 4 处 = 视图档又没人付（实测修前全模块 0 条
#      copy-in ⇒ 交回腿净 −1 ⇒ 过程出口拆槽时 0xC0000374）。
#   ⑨ 「接收者是视图 ⇒ 属性读走 vtable 槽」那一臂有一位主人 ivrefViewGetterReadExpr：
#      定义恰 1 (cgen_iface_vtbl.cpp) / 声明恰 1 (cgen_helpers.inc) / 调用点恰 1 且只住在
#      cgen_util_com.cpp (站S1)。它问名册那一头 slotIvrefViewName 只许主人自己用：带 `(` 的
#      出现恰 3 处 (定义 + 声明 + 主人内那一次)、文件只许那两份。长出第 4 处 = 有站点绕过主人
#      直接问名册 = 「是不是视图」有了第二位判据的读者。
# 退出码：0 = 九条都过；1 = 任一条红。
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
if ($ivFiles.Count -ne 5) {
    $fail += ("ifaceViewOfExpr consumer files = " + $ivFiles.Count +
              " (want exactly 5): " + ($ivFiles -join ', '))
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
if ($viewCalls.Count -ne 4) {
    $fail += ("ivrefInstanceViewExpr call sites = " + $viewCalls.Count + " (want exactly 4)")
}
$viewCallFiles = @($viewCalls | ForEach-Object { Split-Path -Leaf $_.Path } | Sort-Object -Unique)
if (($viewCallFiles -join ',') -ne 'cgen_base_type.cpp,cgen_setlet_set_prop.inc,cgen_setlet_set_rhs.inc') {
    $fail += ("view-shape consumers = " + ($viewCallFiles -join ', ') +
              " (want the two Set stations + the arg delivery side)")
}
# 交付侧那一站必须**经主人**拿这一发 (自己内联拼 = 这一条掉下来)
$deliverVia = @(Select-String -Path (Join-Path $backend "cgen_base_type.cpp") `
                              -Pattern 'ivrefInstanceViewExpr\(')
if ($deliverVia.Count -ne 2) {
    $fail += ("deliverToClassSlot routes the view shape through ivrefInstanceViewExpr " +
              ("" + $deliverVia.Count) + " times (want exactly 2 = New 那一档 + 借来的实例那一档)" +
              " (少一档 = 实参站回到把实例基址当视图交出去, 实测 rc=0xC0000005)")
}

# ⑦ `"__iv_" + cIdent(` 那份拼法的住家名单
$shapeFiles = @($files | Select-String -Pattern '"__iv_" \+ cIdent' |
    ForEach-Object { Split-Path -Leaf $_.Path } | Sort-Object -Unique)
if (($shapeFiles -join ',') -ne 'cgen_expr_call_arg_emit.inc,cgen_iface_vtbl.cpp') {
    $fail += ("inline __iv_ spellings live in " + ($shapeFiles -join ', ') +
              " (want only the owner + the ByVal arg adapter)")
}

# ⑧ 视图槽「谁付钱」那一发 AddRef 的发射点名单 (第二百零五刀)
$addRefs = @($files | Select-String -Pattern '"->vt->AddRef\("' -Encoding UTF8)
if ($addRefs.Count -ne 5) {
    $fail += ("view AddRef emission sites = " + $addRefs.Count + " (want exactly 5)")
}
$addRefFiles = @($addRefs | ForEach-Object { Split-Path -Leaf $_.Path } | Sort-Object -Unique)
if (($addRefFiles -join ',') -ne 'cgen_com.cpp,cgen_iface_vtbl.cpp,cgen_setlet_set_prop.inc') {
    $fail += ("view AddRef owners = " + ($addRefFiles -join ', ') +
              " (want copy-in + emitIvrefRefSet + the two recorded inline Set shapes)")
}
$copyInAddRef = @(Select-String -Path (Join-Path $backend "module\cgen_com.cpp") `
                              -Pattern '"->vt->AddRef\("' -Encoding UTF8)
if ($copyInAddRef.Count -ne 1) {
    $fail += ("registerClassArgRefCopyIn view arm = " + $copyInAddRef.Count +
              " (want exactly 1; 0 = 视图槽又没人付钱, 实测净 −1 → 出口 0xC0000374)")
}


# ⑨ 新式视图属性读那一臂的主人 (第二百一十刀 S1)
$rdAll = @($files | Select-String -Pattern 'ivrefViewGetterReadExpr\(')
$rdDef = @($rdAll | Where-Object { (Split-Path -Leaf $_.Path) -eq 'cgen_iface_vtbl.cpp' })
$rdDecl = @($rdAll | Where-Object { (Split-Path -Leaf $_.Path) -eq 'cgen_helpers.inc' })
$rdCalls = @($rdAll | Where-Object { $_ -notin $rdDef -and $_ -notin $rdDecl })
if ($rdDef.Count -ne 1) {
    $fail += ("ivrefViewGetterReadExpr definition sites = " + $rdDef.Count + " (want exactly 1)")
}
if ($rdDecl.Count -ne 1) {
    $fail += ("ivrefViewGetterReadExpr declarations = " + $rdDecl.Count + " (want exactly 1)")
}
if ($rdCalls.Count -ne 1) {
    $fail += ("ivrefViewGetterReadExpr call sites = " + $rdCalls.Count + " (want exactly 1)")
}
$rdCallFiles = @($rdCalls | ForEach-Object { Split-Path -Leaf $_.Path } | Sort-Object -Unique)
if (($rdCallFiles -join ',') -ne 'cgen_util_com.cpp') {
    $fail += ("view property-read consumer = " + ($rdCallFiles -join ', ') +
              " (want only station S1 in cgen_util_com.cpp)")
}
$nameAll = @($files | Select-String -Pattern 'slotIvrefViewName\(')
if ($nameAll.Count -ne 3) {
    $fail += ("slotIvrefViewName sites = " + $nameAll.Count +
              " (want exactly 3 = definition + declaration + the owner's single ask)")
}
$nameFiles = @($nameAll | ForEach-Object { Split-Path -Leaf $_.Path } | Sort-Object -Unique)
if (($nameFiles -join ',') -ne 'cgen_helpers.inc,cgen_iface_vtbl.cpp') {
    $fail += ("roster ask lives in " + ($nameFiles -join ', ') +
              " (want the owner + its declaration; a 3rd file = a station bypasses the owner)")
}


if ($fail.Count -gt 0) {
    Write-Host "IFACE-VIEW-AUTHORITY FAIL"
    foreach ($f in $fail) { Write-Host ("  - " + $f) }
    exit 1
}
Write-Host "IFACE-VIEW-AUTHORITY OK (1 formula site / 5 consumers / 6 literal files / 2 Set stations ask / 4 refSet calls / 4 view-shape consumers / 2 inline __iv_ files / 5 view AddRef sites in 3 files / 1 view property-read owner in 1 station + 3 roster ask sites in 2 files)"
exit 0
