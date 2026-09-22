// vb6forms_axsite.c - Fix 143~149: 第三方 OCX (ActiveX 控件) 真宿主 —— 模块入口
//
// 2026-09-20 按功能家族拆为 5 个编译单元（原 1029 行 → 本文件仅作入口与映射表）
//
// 本文件（主文件）只保留文件级说明，不含实现。
// RTL 各 .c 是独立编译单元（由 MSVC 分别编译再链接），文件级 static 跨文件不可见
// —— 被多族使用的结构体完整定义与符号一律集中声明在 vb6forms_axsite_internal.h。
//
// 族划分与原文行区间映射（拆分前 vb6forms_axsite.c 的 1-based 行号）：
//   axsite/ax_site.c       29~33   72~206  523~531
//                          站点本体三套 vtable + 已创建宿主登记表
//   axsite/ax_site_ext.c   591~592 596~832
//                          windowless 三件套 (ambient / ControlSite / InPlaceSiteWindowless)
//   axsite/ax_propbag.c    277~280 287~405
//                          简易 IPropertyBag (设计期属性灌入 + 设计期 IFont)
//   axsite/ax_load.c       406~522
//                          多路径实例化 (免注册/同目录/CWD/注册表) + 缇↔HIMETRIC
//   axsite/ax_host.c       22~28   207~276  532~590  833~1029
//                          对外入口 + 消息转发/绘制 + OcxHost_Create 10 步流程
//
// 原文 1~21 行（文件头 + include 集）由各拆分片自带的头替代；
// 原文 34~71 / 281~286 行（Vb6AxSite / Vb6PropBag 结构定义）移入 vb6forms_axsite_internal.h；
// 原文 593~595 行（三个 static vtable 前置声明）删除 —— 与内部头的 extern 声明冲突 (C2370)，
// 且拆分后各 vtable 定义所在文件不再需要前置声明。
//
// 说明：本文件本身**不参与编译**（不登记 driver_link.cpp 的 sourceFiles），
// 仅作为模块入口与上述映射表的载体 —— 与 vb6forms_uc.c / vb6rtl_format.c 的伞文件定位一致。
// 若需编译本模块，请登记 axsite/ 下 5 个 .c。

#include "vb6forms_axsite_internal.h"

// 实现分散在 axsite/ 下 5 个族编译单元，由构建管线分别编译链接（见上方映射表）。
