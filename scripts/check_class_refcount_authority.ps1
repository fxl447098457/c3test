# check_class_refcount_authority.ps1 - 账 #305 §B218 (甲)：工程类实例的引用计数只许有一位主人。
# 九条钉（每条都能单独红，红了就说明同一问长出了第二位主人）：
#   ① 出入口那一对的 mint 各恰好 1 处，且住在 module/cgen_com.cpp（每条腿只许转调它）。
#   ② 计数头 `int32_t __refcount;` 的 mint 恰好 1 处，且住在 cgen_iface_vtbl.cpp 的
#      emitIfaceClassFields 里 —— 它必须紧跟 __comObj 那一句（ai/022 D19：偏移 0 恒是 __comObj）。
#   ③ 覆写/置空那一位主人 emitClassSlotRefSet 的消费文件恰好 2 份（Set 的 RHS 臂 + 置空臂）；
#      第 3 份只能由腿④/腿⑤ 那两刀带进来，届时这里跟着改数。
#   ④ 工程类形参登记收成一处出口 registerClassFormal：消费文件恰好 3 份（Sub / Function /
#      Property 三个入口），调用点恰好 9 处（每份三条：跨模块兜底 / Class 符号 / ComClass 同名）。
#   ⑤ procClassFormals_ 那张名册只许住在 7 份文件里（表本体 + 三个入口清表 + 出口本体 +
#      两条腿各问一次）。
#   ⑥ 字面量 __refcount 的主人名单 = 在册那 6 份；RTL 那一处必须是 vb6comserver_obj.c 的注释。
#   ⑦ 陈旧注释「没有引用计数」（具体类那三处）不许活着 —— 甲落地之后这句话是错的。
#   ⑧ 过程出口那张带档的登记表 slotsToRelease_ 只许住在 5 份文件里（表本体 + 三个过程入口
#      清表 + 主人本体），工程类那一档的注册口 trackClassSlotForRelease 恰好 2 处
#      （Dim As <工程类> 的两条注册路：Class 符号 / projectClassNameOf 兜底）。
# 退出码：0 = 九条都过；1 = 任一条红；2 = 找不到 src/backend。
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
$srcFiles = Get-ChildItem -Path (Join-Path $Root "src") -Recurse -File |
    Where-Object { $_.Extension -match '^\.(cpp|inc|c|h|hpp)$' }
$backendFiles = Get-ChildItem -Path $backend -Recurse -File |
    Where-Object { $_.Extension -match '^\.(cpp|inc|c|h|hpp)$' }

$fail = @()

function CountOf([string]$pattern) {
    $n = 0
    foreach ($f in $srcFiles) {
        $t = [IO.File]::ReadAllText($f.FullName)
        $n += ([regex]::Matches($t, [regex]::Escape($pattern))).Count
    }
    return $n
}
function FilesOf([string]$pattern) {
    $r = @()
    foreach ($f in $srcFiles) {
        $t = [IO.File]::ReadAllText($f.FullName)
        if (([regex]::Matches($t, [regex]::Escape($pattern))).Count -gt 0) { $r += $f.Name }
    }
    return @($r | Sort-Object -Unique)
}

# ① mint 那一对
foreach ($sig in @('"_AddRef(void* instance) {', '"_Release(void* instance) {')) {
    $bs = @($backendFiles | Select-String -SimpleMatch $sig)
    if ($bs.Count -ne 1) {
        $fail += ("pair mint sites for " + $sig + " = " + $bs.Count + " (want exactly 1)")
    } elseif ($bs[0].Path -notmatch 'cgen_com\.cpp$') {
        $fail += ("pair mint moved into " + $bs[0].Path)
    }
}

# ② 计数头的 mint
$hdr = @($backendFiles | Select-String -SimpleMatch '    int32_t __refcount;')
if ($hdr.Count -ne 1) {
    $fail += ("refcount-header mint = " + $hdr.Count + " (want exactly 1)")
} elseif ($hdr[0].Path -notmatch 'cgen_iface_vtbl\.cpp$') {
    $fail += ("refcount-header mint moved into " + $hdr[0].Path)
}

# ③ 覆写/置空那位主人的消费文件
$ownerFiles = @(FilesOf 'emitClassSlotRefSet' |
    Where-Object { $_ -ne 'cgen_com.cpp' -and $_ -ne 'cgen_helpers.inc' })
if ($ownerFiles.Count -ne 2) {
    $fail += ("emitClassSlotRefSet consumer files = " + $ownerFiles.Count +
              " (want exactly 2): " + ($ownerFiles -join ', '))
}

# ④ 形参登记那枚出口：文件 3 份、调用点 9 处
$regFiles = @(FilesOf 'registerClassFormal' |
    Where-Object { $_ -ne 'cgen_util.cpp' -and $_ -ne 'cgen_helpers.inc' })
if ($regFiles.Count -ne 3) {
    $fail += ("registerClassFormal consumer files = " + $regFiles.Count +
              " (want exactly 3): " + ($regFiles -join ', '))
}
$regSites = CountOf 'registerClassFormal(pLower, '
if ($regSites -ne 9) {
    $fail += ("registerClassFormal call sites = " + $regSites + " (want exactly 9)")
}

# ⑤ procClassFormals_ 的名册面
$cfFiles = FilesOf 'procClassFormals_'
if ($cfFiles.Count -ne 7) {
    $fail += ("procClassFormals_ files = " + $cfFiles.Count +
              " (want exactly 7): " + ($cfFiles -join ', '))
}

# ⑥ __refcount 字面量的主人名单
$rcFiles = FilesOf '__refcount'
$wantRc = @('cgen_com.cpp', 'cgen_expr_call_arg_emit.inc', 'cgen_iface_vtbl.cpp',
            'cgen_setlet_set_prop.inc', 'driver_classchain.cpp', 'vb6comserver_obj.c') |
    Sort-Object -Unique
if (($rcFiles -join ',') -ne ($wantRc -join ',')) {
    $fail += ("__refcount file list changed: got " + ($rcFiles -join ', ') +
              " (want " + ($wantRc -join ', ') + ")")
}
$rtlRc = @(Get-ChildItem -Path (Join-Path $Root "src\rtl") -Recurse -File |
    Where-Object { $_.Extension -match '^\.(c|h)$' } |
    ForEach-Object { ([regex]::Matches([IO.File]::ReadAllText($_.FullName), '__refcount')).Count } |
    Measure-Object -Sum).Sum
if ($rtlRc -ne 1) {
    $fail += ("__refcount hits in src/rtl = " + $rtlRc +
              " (want exactly 1: the comment in vb6comserver_obj.c)")
}

# ⑦ 陈旧注释
$stale = CountOf '没有引用计数'
if ($stale -ne 0) {
    $fail += ("stale '没有引用计数' comment lines = " + $stale + " (want 0)")
}

# ⑧ 出口那一腿的登记面
$srFiles = FilesOf 'slotsToRelease_'
if ($srFiles.Count -ne 5) {
    $fail += ("slotsToRelease_ files = " + $srFiles.Count +
              " (want exactly 5): " + ($srFiles -join ', '))
}
$csSites = CountOf 'trackClassSlotForRelease(cIdent(var.name)'
if ($csSites -ne 2) {
    $fail += ("trackClassSlotForRelease registrant sites = " + $csSites +
              " (want exactly 2)")
}

# ⑨ 出参槽那一档 (腿②'): claim-only 的发码形状只许住在主人本体里一处, 站点也恰好一处。
#    为什么单独钉: 腿④ 会在过程出口放手局部, 写进出参槽的那一枚若不自己取走引用, 调用方
#    还看着它时它就拆了 (门 #763 实测 b187 第五臂 probe exit=-1073740940)。
$primeShapes = CountOf '(甲) 腿②'': Set 出参槽 '
if ($primeShapes -ne 1) {
    $fail += ("out-slot claim shape sites = " + $primeShapes + " (want exactly 1)")
}
$primeCalls = CountOf 'emitClassSlotRefSet(target, clsOfRhs'
if ($primeCalls -ne 1) {
    $fail += ("out-slot claim call sites = " + $primeCalls + " (want exactly 1)")
}

if ($fail.Count -gt 0) {
    Write-Host "CLASS-REFCOUNT-AUTHORITY FAIL"
    foreach ($f in $fail) { Write-Host ("  - " + $f) }
    exit 1
}
Write-Host "CLASS-REFCOUNT-AUTHORITY OK (1 pair mint / 1 header / 2 leg consumers / 3+9 formals / 7 roster / 6 literal files / 0 stale / 5 registry files + 2 registrants / 1+1 out-slot)"
exit 0
