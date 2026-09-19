// vb6forms_uc_internal.h - vb6forms_uc 模块内部共享声明（Fix 112 UserControl 宿主）
//
// 2026-09-19 把单文件 vb6forms_uc.c（1546 行）按族拆为 uc/ 下 6 个编译单元 +
// uc/detail/ 下 7 个函数体片段。RTL 各 .c 是独立编译单元（MSVC 分别编译再链接），
// 文件级 static 跨文件不可见 —— 因此把**被多个族使用**的符号集中声明在此。
//
// 搬进本头的三件事：
//   1. 容量上限宏与合成对象 tag；
//   2. 被多族按成员访问的结构体定义（vb6_UCRec / vb6_UCSaved / vb6_HostObjRec /
//      vb6_UCControls / vb6_UCFontRec）—— 定义在头内是必要的，不能只做前置声明，
//      因为各族要解引用其字段；
//   3. 被多族调用的函数与共享状态（原为文件级 static，现提升为外部链接）。
//
// 只被单一族使用的 static 一律随族搬走并保持 static，不进本头。
//
// 注意：RTL 侧 include 一律用 basename（源码树的 uc/ 子目录在解包后的平铺目录里
// 不存在）。本头本身是嵌入资源，各单元用 #include "vb6forms_uc_internal.h"。

#ifndef VB6C3_VB6FORMS_UC_INTERNAL_H
#define VB6C3_VB6FORMS_UC_INTERNAL_H

#include "vb6forms.h"
#include "vb6rtl.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// 容量上限（多族共用）
// ============================================================

#define VB6_UC_MAX_DESC 32
#define VB6_UC_MAX_INST 64
#define VB6_UC_MAX_OBJ  128
#define VB6_UC_NAME_LEN 64

// 合成对象 tag
#define VB6_UC_CONTROLS_TAG 0x0C70C70C
#define VB6_UC_FONT_TAG     0xF0A7F0A7
#define VB6_UC_COLL_TAG     0xC01C01C0

// ============================================================
// 跨族共享结构体定义
// ============================================================

// UserControl 实例记录（uc_host.c 持有实例表）
typedef struct vb6_UCRec {
    const vb6_UserControlDesc* desc;
    void*  me;
    int32_t ready;        // Initialize 完成前禁止窗口消息进入 VB6 实例方法
    HWND   hwnd;
    HWND   parent;
    int32_t scaleWidth;   // ScaleMode 单位
    int32_t scaleHeight;
    HDC    hdc;
    int16_t enabled;
    void*  font;          // vb6_ComIface_Font*
    int32_t extLeft;      // Extender.Left/Top (容器坐标, 缇)
    int32_t extTop;
    int32_t index;        // 控件数组下标, -1=非数组
    wchar_t ctrlName[VB6_UC_NAME_LEN];
    BSTR    displayNameBstr;   // Fix 116: Ambient.DisplayName 缓存 (控件实例名)
} vb6_UCRec;

// UserControl 宿主状态快照（push/pop 用）
typedef struct vb6_UCSaved {
    int32_t scaleWidth, scaleHeight, scaleMode;
    void*   hDC;
    int32_t containerHwnd;
    int16_t enabled;
    void*   font;
    void*   ambientFont;
    int32_t extLeft, extTop;
    vb6_UCRec* current;
    void*   displayName;      // Fix 116: Ambient.DisplayName (BSTR)
} vb6_UCSaved;

// 宿主对象登记记录（uc_hostmodel.c 持有登记表）
typedef struct vb6_HostObjRec {
    void*   hwnd;
    wchar_t name[VB6_UC_NAME_LEN];
    wchar_t typeName[VB6_UC_NAME_LEN];
    int32_t isForm;
    int32_t index;        // 控件数组下标 (-1=非数组)
    void*   instance;     // 工程 UserControl 实例, 否则 NULL
} vb6_HostObjRec;

// 合成对象: Controls 集合
typedef struct vb6_UCControls {
    int32_t tag;          // VB6_UC_CONTROLS_TAG
    void*   formHwnd;
} vb6_UCControls;

// 合成对象: Font (stdole.StdFont 的最小形态)
typedef struct vb6_UCFontRec {
    int32_t tag;          // VB6_UC_FONT_TAG
    void*  font;          // vb6_ComIface_Font*
    struct vb6_UCFontRec* next;
} vb6_UCFontRec;

// Fix 113h: 离屏 DIB（定义在 uc_debug.c，uc_host_window.c 的 WM_PAINT 调试路径也要用）
typedef struct vb6_UCDib {
    HDC     memDC;
    HBITMAP bmp;
    HBITMAP oldBmp;
    void*   bits;
    int32_t w, h, stride;
} vb6_UCDib;

// ============================================================
// 跨族共享状态（定义在各族 .c，原为文件级 static）
// ============================================================

// --- 描述表 / 实例表 / 宿主状态（uc_host.c）---
extern const vb6_UserControlDesc* g_uc_descs[VB6_UC_MAX_DESC];
extern int32_t g_uc_descCount;
extern vb6_UCRec g_uc_recs[VB6_UC_MAX_INST];
extern int32_t g_uc_recCount;
extern vb6_UCRec* g_uc_current;                  // 最近进入的实例
extern vb6_ComIface_Font* g_uc_pendingFont;      // Fix 119: 待应用实例字体

// --- 宿主对象登记表（uc_hostmodel.c）---
extern vb6_HostObjRec g_ho[VB6_UC_MAX_OBJ];
extern int32_t g_hoCount;

// --- Font 链表头（uc_controls.c）---
extern vb6_UCFontRec* g_uc_fonts;

// --- 字体单例（定义在 vb6rtl_com.c，uc_controls.c 依赖其地址做身份判定）---
extern vb6_ComIface_Font g_vb6_UserControl_FontObj;

// ============================================================
// 跨族共享函数（原为文件级 static，现提升为外部链接）
// ============================================================

// --- uc_host.c ---
void vb6_uc_trace(const char* phase, const char* type, void* me);
vb6_UCRec* vb6_uc_findByHwnd(const void* hwnd);
vb6_UCRec* vb6_uc_findByInstance(const void* inst);

// --- uc_hostmodel.c ---
vb6_HostObjRec* vb6_ho_find(const void* hwnd);

// --- 判定辅助（uc_controls.c / uc_collection.c 提供）---
int32_t vb6_uc_ptrReadable(const void* p, size_t n);
int32_t vb6_uc_isControls(const void* p);
int32_t vb6_uc_isFont(const void* p);
int32_t vb6_uc_isColl(const void* p);

// --- 查表 / 构造辅助 ---
vb6_ComIface_Font* vb6_uc_fontOf(void* p);       // uc_controls.c
vb6_UCControls* vb6_uc_newControls(void* formHwnd);  // uc_controls.c

// --- 变体工具（uc_hostmodel.c 定义，uc_collection.c 依赖 setVariantEmpty）---
void vb6_ho_setVariantEmpty(vb6_VARIANT* out);
void vb6_ho_setVariantLong(vb6_VARIANT* out, int32_t v);
void vb6_ho_setVariantBstr(vb6_VARIANT* out, BSTR s);
void vb6_ho_setVariantDouble(vb6_VARIANT* out, double v);
void vb6_ho_setVariantDispatch(vb6_VARIANT* out, void* p);
int32_t vb6_ho_variantToLong(const vb6_VARIANT* v);
double vb6_ho_variantToDouble(const vb6_VARIANT* v);
BSTR vb6_ho_variantToBstr(const vb6_VARIANT* v);

// --- Fix 113h/Fix 123 调试辅助（uc_debug.c 定义，uc_host_window.c 的 WM_PAINT 依赖）---
// 离屏 DIB 工具 + 整窗合成 dump；未设环境变量 C3_UC_DUMPDIR 时不会被调用。
void vb6_uc_dibCreate(vb6_UCDib* d, HDC refDC, int32_t w, int32_t h);
void vb6_uc_dibSaveBmp(const vb6_UCDib* d, const char* path);
void vb6_uc_dibDestroy(vb6_UCDib* d);
void vb6_uc_dumpFormComposite(HWND root, const char* dumpDir);
extern int32_t g_uc_dumpSeq;   // dump 文件序号

#ifdef __cplusplus
} // extern "C"
#endif

#endif // VB6C3_VB6FORMS_UC_INTERNAL_H
