# compare_emit_manifest.ps1 - 把一份发码清单与 checked-in 的期望清单逐行比；不同就非零退出。
#
# 为什么单独一个脚本、而且是唯一一份比较逻辑：CI 那一步（emit_manifest.ps1 出完清单后调它）与本机
# 复算/负控/重新登记必须走同一段代码，否则「CI 上绿、本机换个说法就红」这种口径分家又会长出来。
# 背景（台账 §B73，账 #245 + #267）：门 #418 实测 CI 与本机两台的全语料清单 395/395 行 sha256 相同，
# 于是「这次发码长什么样」第一次可以判红，而不是只留档。
#
# 用法:
#   powershell -File scripts/compare_emit_manifest.ps1 -Manifest <清单> [-Expect <期望>]
#   powershell -File scripts/compare_emit_manifest.ps1 -Manifest <清单> [-Expect <期望>] -Bless
# 退出码: 0 相同 / 1 不一致 / 2 文件或参数缺失
param(
    [string]$Manifest = "",
    [string]$Expect = "",
    [switch]$Bless
)
$ErrorActionPreference = 'Continue'
if (-not $Manifest) { Write-Host "用法: -Manifest <清单路径>"; exit 2 }
$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $Expect) { $Expect = Join-Path $repoRoot "emit-manifest.expected.txt" }
if (-not (Test-Path $Manifest)) { Write-Host ("MISSING " + $Manifest); exit 2 }
if (-not (Test-Path $Expect)) { Write-Host ("MISSING " + $Expect); exit 2 }

# 只比**数据行**。'#' 开头的是身份注脚（c3-exe 哈希、pe-lnk、runner 名、inputs= 之类），
# 天生随机器/随构建变 —— 留在清单里给人看，不参与判定。
function DataRows([string]$path) {
    @(Get-Content -LiteralPath $path -Encoding UTF8 | Where-Object { $_ -match '\S' -and $_ -notmatch '^#' })
}
function HeaderRows([string]$path) {
    $all = @(Get-Content -LiteralPath $path -Encoding UTF8)
    $stop = 0
    foreach ($ln in $all) {
        if ($ln -match '\S' -and $ln -notmatch '^#') { break }
        $stop++
    }
    @($all[0..([Math]::Max($stop - 1, 0))])
}
$got = DataRows $Manifest
# 自证覆盖率：清单自己的注脚里写着 `inputs=<N>`（emit_manifest.ps1 数的是它递给 C3.exe 的输入条数）。
# 判定只比 '#' 以外的行 —— 万一有人（或某次改动）把一**整行数据**折成注释，两边会一起少掉同一行、
# 于是"全对"而覆盖悄悄缩水。注脚与数据行各数各的，对不上就是这里出了问题。
$declared = 0
foreach ($h in @(Get-Content -LiteralPath $Manifest -Encoding UTF8 | Where-Object { $_ -match '^#\s*arch=.*inputs=(\d+)' })) {
    $declared = [int]([regex]::Match($h, 'inputs=(\d+)').Groups[1].Value)
}
if ($declared -gt 0 -and $declared -ne $got.Count) {
    Write-Host ("发码清单自相矛盾: 注脚 inputs=" + $declared + " 而数据行 " + $got.Count +
                " —— 有输入没进判定（别把数据行改成 '#' 开头）")
    exit 1
}
if ($Bless) {
    $hdr = @(HeaderRows $Expect)
    Set-Content -LiteralPath $Expect -Value ($hdr + $got) -Encoding UTF8
    Write-Host ("已登记 " + $Expect + "：注脚 " + $hdr.Count + " 行 + 数据行 " + $got.Count + " 行")
    exit 0
}
$exp = DataRows $Expect

# 键取行尾那段输入路径（语料顺序变了也不该误报），值取前面所有哈希/计数字段
function AsMap($rows) {
    $h = [ordered]@{}
    foreach ($r in $rows) {
        $i = $r.LastIndexOf(' ')
        if ($i -gt 0) { $h[$r.Substring($i + 1)] = $r.Substring(0, $i) }
    }
    return $h
}
$me = AsMap $exp
$mg = AsMap $got
$missing = @($me.Keys | Where-Object { -not $mg.Contains($_) })
$extra = @($mg.Keys | Where-Object { -not $me.Contains($_) })
$diff = @($me.Keys | Where-Object { $mg.Contains($_) -and $me[$_] -ne $mg[$_] })
$same = @($me.Keys | Where-Object { $mg.Contains($_) -and $me[$_] -eq $mg[$_] }).Count

Write-Host ("发码清单比较: 期望 " + $exp.Count + " 行 / 实得 " + $got.Count +
            " 行 / 相同 " + $same + " / 哈希不同 " + $diff.Count +
            " / 缺席 " + $missing.Count + " / 多出 " + $extra.Count)
foreach ($k in ($diff | Select-Object -First 10)) {
    Write-Host ("  哈希不同: " + $k)
    Write-Host ("    期望 " + ([string]$me[$k]).Substring(0, [Math]::Min(78, ([string]$me[$k]).Length)))
    Write-Host ("    实得 " + ([string]$mg[$k]).Substring(0, [Math]::Min(78, ([string]$mg[$k]).Length)))
}
foreach ($k in (@($missing + $extra) | Select-Object -First 8)) { Write-Host ("  行集合变了: " + $k) }

if ($diff.Count -eq 0 -and $missing.Count -eq 0 -and $extra.Count -eq 0 -and $exp.Count -eq $got.Count) {
    Write-Host "emit-manifest 与期望逐行相同"
    exit 0
}
Write-Host "emit-manifest 与期望不一致（发码形状变了，或语料清单要重新登记）"
exit 1
