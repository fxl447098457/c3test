// vb6forms_uc.c - Fix 112: 工程内 UserControl (.ctl) 实例宿主 + 窗体/控件宿主对象模型
//
// 背景 (Charts 2020): 窗体上的子控件是**工程内 UserControl** (`Begin Proyecto1.ucChartBar
// ucChartBar1`)。此前 cgen 对这类控件什么也不生成 → `vb6_hwnd_ucChartBar1` 恒为 NULL,
// `ucChartBar1.AddSerie(...)` 全部落到 `vb6_ComCall(NULL, ...)` 被静默丢弃 → 白板窗体。
// 另外 `cResizer.SaveControlsPositions Me` 把窗体 `Me`(=HWND) 当 IDispatch 用
// (`.ScaleWidth/.Count/.Controls`) → `vb6_ComGetDoubleProp(HWND, ...)` 解引用野指针 → 0xC0000005。
//
// 本文件提供两块能力:
//   (1) UserControl 实例宿主: 为每个 .ctl 实例建一个真实子窗口, WM_PAINT 转发到
//       UserControl_Paint, 并把进程级 `vb6_UserControl_*` 宿主状态换入/换出;
//   (2) 宿主对象模型: 在 `vb6_Com*` 入口之前拦截「HWND/集合/字体」这类非 IDispatch
//       指针, 用 Win32 语义回答 VB6 属性/方法 (ScaleWidth/hwnd/Controls/Left/...),
//       使 `Me` 与标准控件可以像对象一样被晚期绑定代码使用。
//
// 生成代码接口 (cgen 发射):
//   vb6_UC_Register(&desc)                       — .ctl 模块自注册
//   vb6_UC_HostCreate(type, x,y,w,h, parent, hInst, ctrlName)
//   vb6_UC_InstanceOf(hwnd) / vb6_UC_HwndOf(inst)
//   vb6_Forms_RegisterControl(hwnd, name, vbTypeName, index)
//   vb6_Host_*  — 由 vb6com_* / vb6_CallByName 内部调用
//
// ============================================================
// 2026-09-19 按族拆为 6 个编译单元 + 7 个函数体片段（原 1546 行 → 本文件仅作入口）
// ============================================================
// 本文件（主文件）只保留上方的原始文件级说明 + include 骨架，不含实现。
// RTL 各 .c 是独立编译单元（由 MSVC 分别编译再链接），文件级 static 跨文件不可见
// —— 被多族使用的符号一律集中声明在 vb6forms_uc_internal.h。
//
// 族划分与原文行区间映射（拆分前 vb6forms_uc.c 的 1-based 行号）：
//   uc/uc_host.c           41~298  613~629  631~739  741~753
//                          UserControl 实例表 / 宿主状态 push·pop / 公开 API
//   uc/uc_host_window.c    459~565  567~607
//                          宿主窗口过程 / 类注册 / GDI+ 进程级初始化
//   uc/uc_hostmodel.c      755~819  1187~1300  1303~1494  1497~1542
//                          窗体·控件宿主对象登记 + 属性/方法分派 + VARIANT 转换
//   uc/uc_controls.c       821~891  893~962
//                          Controls 集合 / Font 对象（stdole.StdFont 最小实现）
//   uc/uc_collection.c     964~988  989~1185
//                          Fix 112c 内建 Collection + For Each 枚举器
//   uc/uc_debug.c          300~457
//                          Fix 113h 离屏 DIB 捕获 / Fix 123 整窗合成 dump（调试用）
//
// 函数体片段（放在 uc/detail/，在函数体内 #include，RTL 侧一律 basename）：
//   uc_host_create.inc          vb6_UC_HostCreate 函数体
//   uc_hostmodel_getprop.inc    vb6_Host_GetProp 函数体
//   uc_hostmodel_setprop.inc    vb6_Host_SetProp 函数体
//   uc_hostmodel_call.inc       vb6_Host_Call 函数体
//   uc_collection_api.inc       Collection 实现段
//   uc_debug_dib.inc            离屏 DIB 捕获
//   uc_debug_composite.inc      整窗合成 dump
//
// 说明：本文件本身**不参与编译**（不登记 driver_link.cpp 的 sourceFiles），
// 仅作为模块入口与上述映射表的载体 —— 与 vb6rtl_format.c 的伞文件定位一致。
// 若需编译本模块，请登记 uc/ 下 6 个 .c。

#include "vb6forms_uc_internal.h"

// 实现分散在 uc/ 下 6 个族编译单元，由构建管线分别编译链接（见上方映射表）。
