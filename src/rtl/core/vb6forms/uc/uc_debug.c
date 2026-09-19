// uc_debug.c - vb6forms_uc 拆分片：离屏 DIB 捕获 + 整窗合成 dump（调试用）
//
// 内容 = 拆分前 vb6forms_uc.c 第 300~369 / 371~457 行，纯搬移零重排无行为改动
// 跨族共享符号见 vb6forms_uc_internal.h

#include "vb6forms_uc_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// Fix 113h: 离屏 DIB 绘制捕获 (调试用, 环境变量 C3_UC_DUMPDIR 启用)
// ============================================================
// 背景: 在无交互桌面 / RDP 断连的会话里, 屏幕位图 (GetDC(0) BitBlt / PrintWindow)
// 全部读回黑色 (实测连标准 MessageBox 也是黑的), 无法用截图客观验证"图表到底画
// 没有画出来". 这里让宿主把控件改为绘制到**内存 DIB**, 再把 DIB 转存为 BMP,
// 完全不依赖桌面表面, 因而在任何会话下都能拿到真实绘制结果.
// 未设置 C3_UC_DUMPDIR 时行为与原来完全一致 (直接画到窗口 DC).
#include "uc_debug_dib.inc"
#include "uc_debug_composite.inc"

#ifdef __cplusplus
} // extern "C"
#endif
