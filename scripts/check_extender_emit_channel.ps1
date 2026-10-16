# check_extender_emit_channel.ps1
# 第三片 3a: `Extender.<成员> = v` 必须走**认得实例**的入口, 不能发裸全局赋值。
#
# 背景: 4d5e0280 补齐了 push/pop 存取, eff4de7f/76f91f1e 把通道
# (vb6_UC_ExtenderField) 改指 vb6_UCRec.ext*。于是"用户代码写 vb6_Extender_X = v"
# 与"通道读记录"接不上了 —— 用户的 Extender.Height/Width/Left/Top 会**静默不生效**。
# 3a 的修法是代码生成侧改发 vb6_UC_SetExtenderMember(...), 内部按当前记录落进 ext*。
#
# 本哨兵钉四件事, 每条带负控:
#   1. 代码生成侧闸门用**发射符号前缀**(不是 hostPseudoBareName —— 它不覆盖 Extender 成员)
#   2. 代码生成侧确实改道到实例入口
#   3. RTL 定义了入口, 且头文件声明了它(生成码直接调, 漏声明就 C2065)
#   4. 入口内部走 vb6_UC_ExtenderField(否则又变回只写全局)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$cg = Join-Path $root 'src/backend/detail/stmt/cgen_assign_host_pseudo.inc'
$rtl = Join-Path $root 'src/rtl/core/vb6forms/uc/uc_controls.c'
$hdr = Join-Path $root 'src/rtl/core/vb6rtl/vb6rtl_userctl.h'

foreach ($p in @($cg, $rtl, $hdr)) {
    if (-not (Test-Path -LiteralPath $p)) { Write-Host "missing file: $p" -ForegroundColor Red; exit 1 }
}
$cgT = [System.IO.File]::ReadAllText($cg)
$rtlT = [System.IO.File]::ReadAllText($rtl)
$hdrT = [System.IO.File]::ReadAllText($hdr)

$bad = 0
function Check($name, $ok, $why) {
    if ($ok) { Write-Host "  [ok] $name" }
    else { Write-Host "  [FAIL] $name -- $why" -ForegroundColor Red; $script:bad++ }
}

$gateOk = $cgT -match 'rfind\(\s*"vb6_Extender_"\s*,\s*0\s*\)'
Check 'codegen gates on vb6_Extender_ prefix' $gateOk 'gate must match the emitted symbol prefix; hostPseudoBareName does not cover Extender members'

$emitOk = $cgT -match 'vb6_UC_SetExtenderMember'
Check 'codegen emits vb6_UC_SetExtenderMember' $emitOk 'Extender member writes must be redirected to the instance-aware entry point'

$defOk = $rtlT -match 'void\s+vb6_UC_SetExtenderMember\s*\('
Check 'RTL defines vb6_UC_SetExtenderMember' $defOk 'entry point missing from uc_controls.c'

$protOk = $hdrT -match 'void\s+vb6_UC_SetExtenderMember\s*\([^;]*\)\s*;'
Check 'header declares vb6_UC_SetExtenderMember' $protOk 'generated code calls it directly; without the prototype MSVC gives C2065'

$m = [regex]::Match($rtlT, 'void\s+vb6_UC_SetExtenderMember\s*\([^)]*\)\s*\{(?<b>.*?)\r?\n\}', 'Singleline')
Check 'entry point body present' $m.Success 'cannot locate function body'
if ($m.Success) {
    $b = $m.Groups['b'].Value
    # 只认**调用赋值**那一步, 不能拿函数体内的前向声明蒙混过关
    $callRx = '=\s*vb6_UC_ExtenderField\s*\('
    $viaChannel = $b -match $callRx
    Check 'entry point routes through vb6_UC_ExtenderField' $viaChannel 'must resolve via the channel so the write lands in vb6_UCRec.ext*'
    $neg = $rtlT -replace 'void\*\s+fp\s*=\s*vb6_UC_ExtenderField\([^;]*\);', 'void* fp = 0;'
    $mn = [regex]::Match($neg, 'void\s+vb6_UC_SetExtenderMember\s*\([^)]*\)\s*\{(?<b2>.*?)\r?\n\}', 'Singleline')
    $negFails = (-not $mn.Success) -or (-not ($mn.Groups['b2'].Value -match $callRx))
    Check 'negative control (deleting the channel call must go red)' $negFails 'sentinel is vacuous if neutering the call still passes'
}

# 读侧: 两个入口都要有, 且都必须走通道
foreach ($fn in @('vb6_UC_GetExtenderMemberNum', 'vb6_UC_GetExtenderMemberStr')) {
    $defR = $rtlT -match ("int32_t\s+" + $fn + "\s*\(|BSTR\s+" + $fn + "\s*\(")
    Check "RTL defines $fn" $defR 'read-side entry point missing from uc_controls.c'
    $protR = $hdrT -match ([regex]::Escape($fn) + '\s*\([^;]*\)\s*;')
    Check "header declares $fn" $protR 'generated code calls it directly; without the prototype MSVC gives C2065'
}
# 读侧刻意不跟写侧同形: 返回 void* 会逼所有调用点写强制转换
$voidRead = $rtlT -match 'void\*\s+vb6_UC_GetExtenderMember\s*\('
Check 'read side is NOT a single void* getter' (-not $voidRead) 'one void* getter would force a cast at every arithmetic call site'

# 负控: 两个读入口都必须真的调通道
foreach ($fn in @('vb6_UC_GetExtenderMemberNum', 'vb6_UC_GetExtenderMemberStr')) {
    $mr = [regex]::Match($rtlT, ([regex]::Escape($fn) + '\s*\([^)]*\)\s*\{(?<b>.*?)\r?\n\}'), 'Singleline')
    $negR = $rtlT -replace 'void\*\s+fp\s*=\s*vb6_UC_ExtenderField\([^;]*\);', 'void* fp = 0;'
    $mnr = [regex]::Match($negR, ([regex]::Escape($fn) + '\s*\([^)]*\)\s*\{(?<b>.*?)\r?\n\}'), 'Singleline')
    $ok = (-not $mnr.Success) -or (-not ($mnr.Groups['b'].Value -match '=\s*vb6_UC_ExtenderField\s*\('))
    Check "negative control: neutering the channel makes $fn red" $ok 'sentinel is vacuous if neutering the channel call still passes'
}
if ($bad) { Write-Host "check_extender_emit_channel: FAIL ($bad)"; exit 1 }
Write-Host 'check_extender_emit_channel: PASS'
exit 0