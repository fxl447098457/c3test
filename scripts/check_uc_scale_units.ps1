#!/usr/bin/env powershell
# check_uc_scale_units.ps1 - 结构哨兵: 控件几何的**单位**只有一个权威 (账 #175)
#
# 背景 (实测): UserControl.ScaleWidth / 控件 Left/Top/Width/Height / Move / 鼠标 X,Y
# 这四处以前各自把"缇"写死在实现里, 而语料里 8/9 份 .ctl 声明 ScaleMode=3(Pixel)
# —— 于是像素型控件的几何比绘图 DC 大 15 倍, Charts 2020 的饼/柱/面积/矩形
# 全画在画布外 (整幅空白)。修法是把这条重复决策收进一对入口:
#   vb6_ScalePxToUser(px, scaleMode, vert) / vb6_ScaleUserToPx(user, scaleMode, vert)
# 单位取自容器声明的 ScaleMode (窗体缺省 1=缇, 与改动前逐字节等价)。
#
# 本哨兵钉的是"别再退回写死缇": 任何一处重新出现下面这些旧形状就报红。
# 账 #305 第297刀再补一层: 「按谁的档」这一问连**容器是哪一窗**都要只有一个答案 ——
# vb6_UC_ContainerWindowOf (uc_host.c) 是唯一那位, 它答**创建时记录的容器**,
# 不随 SetParent 漂; 站点数钉在 9+1 (见文末)。
# 与 check_di_stubs.ps1 同一类静态用例, 由 run_tests.ps1 的 [STATIC] 项调用。
param(
    [string]$Root = (Split-Path -Parent $PSScriptRoot)
)
$ErrorActionPreference = "Continue"

# 被禁的旧形状: 文件 -> @(正则, 说明)
$deny = @(
    @{ File = "src/rtl/core/vb6forms/vb6forms_ctrl.c";   Pat = "vb6_TwipToX\(|vb6_TwipToY\(|vb6_XToTwipX\(|vb6_YToTwipY\(" },
    # 账 #305 第297刀: 「按谁的档」不许由几何站点自己拿现父窗答 —— band 里的控件
    # 父窗会漂 (SetParent 到 CoolBar 宿主, 之后 rebar 又搬进 ReBarWindow32) ⇒ 缇被当像素。
    @{ File = "src/rtl/core/vb6forms/vb6forms_ctrl.c";   Pat = "vb6_ContainerScaleMode\(GetParent\(" },
    @{ File = "src/rtl/core/vb6forms/vb6forms_widget.c"; Pat = "vb6_TwipToX\(|vb6_TwipToY\(|vb6_XToTwipX\(|vb6_YToTwipY\(" },
    @{ File = "src/rtl/core/vb6forms/uc/uc_host.c";      Pat = "=\s*vb6_XToTwipX\(|=\s*vb6_YToTwipY\(" },
    @{ File = "src/rtl/core/vb6forms/uc/uc_host_window.c"; Pat = "\*\s*15\.0f" }
    # 账 #177: 单位表只剩一份。vb6rtl_com.c 里那份 (Fix 184 时抄的) 已改成转调权威,
    # 再出现 1440 这个缇系数就说明有人抄回第二张表。
    @{ File = "src/rtl/core/vb6rtl/vb6rtl_com.c";          Pat = "1440" }
)
$viol = @()
foreach ($d in $deny) {
    $p = Join-Path $Root $d.File
    if (-not (Test-Path $p)) { $viol += ("MISSING " + $d.File); continue }
    foreach ($m in (Select-String -Path $p -Pattern $d.Pat)) {
        $viol += ("{0}:{1}: {2}" -f $d.File, $m.LineNumber, $m.Line.Trim())
    }
}

# 权威本身必须在场 (定义 + 声明), 否则上面的"禁旧形状"会退化成没人管
$must = @(
    @{ File = "src/rtl/core/vb6forms/vb6forms.c";       Pat = "double vb6_ScalePxToUser\(double px, int32_t mode, int vert\) \{" },
    @{ File = "src/rtl/core/vb6forms/vb6forms.c";       Pat = "int vb6_ScaleUserToPx\(double user, int32_t mode, int vert\) \{" },
    @{ File = "src/rtl/core/vb6forms/vb6forms_window.h"; Pat = "vb6_ScalePxToUser" },
    @{ File = "src/rtl/core/vb6forms/vb6forms_window.h"; Pat = "vb6_ScaleUserToPx" }
    @{ File = "src/rtl/core/vb6rtl/vb6rtl_com.c";        Pat = "vb6_ScaleUnitsPerPx" },
    @{ File = "src/rtl/core/vb6forms/uc/uc_host.c";      Pat = "void\* vb6_UC_ContainerWindowOf\(const void\* hwnd\) \{" },
    @{ File = "src/rtl/core/vb6forms/vb6forms_window.h"; Pat = "void\* vb6_UC_ContainerWindowOf\(const void\* hwnd\);" },
    @{ File = "src/rtl/core/vb6forms/uc/uc_controls.c";  Pat = "vb6_UC_ContainerWindowOf\(\(void\*\)rc->hwnd\)" }
)
foreach ($m in $must) {
    $p = Join-Path $Root $m.File
    if (-not (Test-Path $p)) { $viol += ("MISSING " + $m.File); continue }
    if (-not (Select-String -Path $p -Pattern $m.Pat -Quiet)) {
        $viol += ("NO-AUTHORITY {0}: {1}" -f $m.File, $m.Pat)
    }
}

# 账 #305 第297刀: 单位那一问的站点数 —— 几何九处 (八对 getter/setter + vb6_ControlMove)
# 全在 vb6forms_ctrl.c, 容器档那一问在 uc_controls.c。加站点就得同时加读数, 撤站点就红。
$ctrlSrc = Join-Path $Root "src/rtl/core/vb6forms/vb6forms_ctrl.c"
$nSite = 0
if (Test-Path $ctrlSrc) {
    $nSite = ([regex]::Matches(((Get-Content -Raw -LiteralPath $ctrlSrc)), "vb6_ContainerScaleMode\(vb6_UC_ContainerWindowOf\(")).Count + ([regex]::Matches((Get-Content -Raw -LiteralPath $ctrlSrc), "vb6_ContainerScaleMode\(child \? vb6_UC_ContainerWindowOf\(")).Count
}
if ($nSite -ne 9) {
    $viol += ("container-window sites in vb6forms_ctrl.c = " + $nSite + " (want 9: 8 对属性 + Move 那一收口)")
}

if ($viol.Count -gt 0) {
    Write-Host ("FAIL uc_scale_units ({0} 处)" -f $viol.Count)
    $viol | ForEach-Object { Write-Host ("  " + $_) }
    exit 1
}
Write-Host "OK uc_scale_units"
exit 0
