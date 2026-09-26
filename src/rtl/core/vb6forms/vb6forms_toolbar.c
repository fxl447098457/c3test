// vb6forms_toolbar.c — ai/029 C29-5a: VB6 Toolbar 控件的窗口 + 标量属性 + 设计期按钮
//
// 复刻口径 (ai/内置控件/Toolbar 与 StatusBar 控件.md + D6): **不加载 MSCOMCTL.OCX**。
// 改之前这枚控件连窗口都没有 —— `controlTypeToWin32Class` 无格，且被
// `cgen_form_create_controls.inc` 里"ImageList || Toolbar 走 CoCreateInstance"那一组扣住
// (直接 continue)，于是 `vb6_hwnd_tb1` 压根不声明：实测读一个 `tb1.Visible` 就是
// **C2065: vb6_hwnd_tb1 未声明的标识符**，整件工程编不过 (x64 里那条 OCX 路本来就静默空转)。
// 本批把它从那组里**摘出来**换成原生 `ToolbarWindow32` (comctl32 注册类；
// vb6_ComCtl_Init 里的 ICC_BAR_CLASSES 早就覆盖了它，不需要新登记)。
//
// 本批范围 = 不吃 Buttons 集合语法的那三面:
//   ShowTips         Boolean, VB6 默认 True     → TBSTYLE_TOOLTIPS   (0x100)
//   TextStyle        0=文字在图标下方(默认) 1=在右侧 → TBSTYLE_LIST     (0x1000)
//   AllowCustomize   Boolean                    → CCS_ADJUSTABLE     (0x20)
//   Align            0..4 (VB6 默认 1 靠上)      → 属性袋 (停靠引擎还没接, 见下)
//   设计期 Buttons   逐条 TB_ADDBUTTONSW 真建进控件
//   留 5b (等成员对象机制, 与 TreeView 8b / ListView 同一套): `Buttons` 的 VB 侧读写
//   (Count / (i).Key / .Caption / .ToolTipText / Add / Remove)、`ButtonClick(Button As Button)`、
//   `ImageList` 关联 (没有它时按钮一律 I_IMAGENONE = 纯文字, 与 VB6 不配图标的观感一致)、
//   `Align` 的真停靠。
//
// 真值放哪: 前三条就是**窗口样式位** (getter 读 GWL_STYLE，读数与观感同源，与 C29-8a 同一口径)。
//   Align 是 VB6 侧概念、原生没有对应位 ⇒ 存窗口属性 (SetProp +1 偏移那套手法：
//   GetPropW 对"未设置"与"存了 0"都返回 NULL，不偏移就把 `Align = 0 不停靠` 当成没写)。
//   按钮自己那张表 (key/caption/style/image/width) 存在本文件的固定槽表里 —— 5b 的
//   Button 对象、将来的 ButtonClick 分发与 TB_GETBUTTONTEXT 那一路都要用它。

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#endif

#include "vb6forms.h"
#include "vb6forms_internal.h"

#ifdef _WIN32

// cgen 那侧按字面量发，这里也钉死数值 (低 _WIN32_IE 下 commctrl.h 不给全)。
#ifndef TBSTYLE_TOOLTIPS
#define TBSTYLE_TOOLTIPS   0x0100
#endif
#ifndef TBSTYLE_FLAT
#define TBSTYLE_FLAT       0x0800
#endif
#ifndef TBSTYLE_LIST
#define TBSTYLE_LIST       0x1000
#endif
#ifndef TBSTYLE_DROPDOWN
#define TBSTYLE_DROPDOWN   0x0080
#endif
#ifndef CCS_ADJUSTABLE
#define CCS_ADJUSTABLE     0x0020
#endif
#ifndef BTNS_BUTTON
#define BTNS_BUTTON        0x0000
#endif
#ifndef BTNS_SEP
#define BTNS_SEP           0x0001
#endif
#ifndef BTNS_CHECK
#define BTNS_CHECK         0x0002
#endif
#ifndef BTNS_GROUP
#define BTNS_GROUP         0x0004
#endif
#ifndef BTNS_DROPDOWN
#define BTNS_DROPDOWN      0x0008
#endif
#ifndef TBSTATE_ENABLED
#define TBSTATE_ENABLED    0x0004
#endif
#ifndef I_IMAGENONE
#define I_IMAGENONE        (-2)
#endif
// 本文件用的消息与结构体一律**与 SDK 头核对过** (`.build/tbprobe2` 那件探针在
// 与 RTL 同一套编译档下量的): `TB_ADDSTRINGW`(+77) / `TB_AUTOSIZE`(+33) /
// `TB_GETBUTTONINFOW`(+63) / `TB_SETBUTTONINFOW`(+64) / `TBIF_*` / `TBBUTTONINFOW`
// 在 10.0.19041 的 commctrl.h 里**都是无条件给的** ⇒ 下面这些 #ifndef 其实是死代码,
// 留着只为"万一换低档头"。**它们一旦存在就必须是对的** —— 5a 当初把 TB_ADDSTRINGW 写成
// +137、TB_AUTOSIZE 写成 +27，全靠 SDK 已定义才没咬人 (同族事故见 8b 的 TVM_SETITEMSTATE)。
// 另: **`TB_RESET` 在这份头里根本没有** (comctl 早已移除)，5a 试过那条路并写下了"不能用
// TB_RESET"的结论，那个 #define 只是残留 ⇒ 现在删掉；Clear 走逐条 TB_DELETEBUTTON。
#ifndef TB_BUTTONCOUNT
#define TB_BUTTONCOUNT      (WM_USER + 24)
#endif
#ifndef TB_BUTTONSTRUCTSIZE
#define TB_BUTTONSTRUCTSIZE (WM_USER + 30)
#endif
#ifndef TB_ADDSTRINGW
#define TB_ADDSTRINGW       (WM_USER + 77)
#endif
#ifndef TB_ADDBUTTONSW
#define TB_ADDBUTTONSW      (WM_USER + 68)
#endif
#ifndef TB_INSERTBUTTONW
#define TB_INSERTBUTTONW    (WM_USER + 67)
#endif
#ifndef TB_AUTOSIZE
#define TB_AUTOSIZE         (WM_USER + 33)
#endif
#ifndef TB_DELETEBUTTON
#define TB_DELETEBUTTON     (WM_USER + 22)
#endif
#ifndef TB_GETBUTTONINFOW
#define TB_GETBUTTONINFOW   (WM_USER + 63)
#endif
#ifndef TB_SETBUTTONINFOW
#define TB_SETBUTTONINFOW   (WM_USER + 64)
#endif
#ifndef TBN_DROPDOWN
#define TBN_DROPDOWN        (0 - 710)     // TBN_FIRST(0-700) - 10
#endif
#ifndef TBIF_IMAGE
#define TBIF_IMAGE          0x00000001
#endif
#ifndef TBIF_TEXT
#define TBIF_TEXT           0x00000002
#endif
#ifndef TBIF_STATE
#define TBIF_STATE          0x00000004
#endif
#ifndef TBIF_STYLE
#define TBIF_STYLE          0x00000008
#endif
#ifndef TBIF_SIZE
#define TBIF_SIZE           0x00000040
#endif
#ifndef TBIF_BYINDEX
#define TBIF_BYINDEX        0x80000000
#endif
#ifndef TBSTATE_HIDDEN
#define TBSTATE_HIDDEN      0x0008
#endif
#ifndef _TBBUTTON_DEFINED
#define _TBBUTTON_DEFINED
typedef struct _TBBUTTONW {
    INT_PTR  iBitmap;
    int      idCommand;
    BYTE     fsState;
    BYTE     fsStyle;
    BYTE     bReserved[2];
    ULONG_PTR dwData;
    INT_PTR  iString;
} TBBUTTONW, *PTBBUTTONW;
#endif

static const wchar_t kTbAlign[] = L"VB6_TB_Align";

#define VB6_TB_MAX        16      // 一窗体里的 Toolbar 枚数
#define VB6_TB_BTNS_MAX  128      // 一枚 Toolbar 的按钮数

typedef struct {
    wchar_t* key;
    wchar_t* caption;
    wchar_t* tooltip;
    wchar_t* tag;        // 5b: VB6 的 Button.Tag (原生没有对应物，只能自己存)
    int      style;      // VB6 那一套 0..5 (不是 BTNS_*)
    int      image;      // ImageList 索引, -1 = 无
    int      width;      // VB6 请求宽度 (缇), 分隔符直接用它的像素值
} Vb6TbBtn;

typedef struct {
    HWND    hwnd;
    int     btnCount;     // 表里的按钮数
    int     nNative;      // **已经发进控件**的按钮数 (与 btnCount 分开算, 见 AddButton 那段)
    Vb6TbBtn btn[VB6_TB_BTNS_MAX];
} Vb6Toolbar;

static Vb6Toolbar g_tbs[VB6_TB_MAX];
static int g_tbCount = 0;

static Vb6Toolbar* vb6_TbFind(HWND h) {
    for (int i = 0; i < g_tbCount; i++) if (g_tbs[i].hwnd == h) return &g_tbs[i];
    return NULL;
}

static Vb6Toolbar* vb6_TbEnsure(HWND h) {
    Vb6Toolbar* t = vb6_TbFind(h);
    if (t) return t;
    if (g_tbCount >= VB6_TB_MAX) return NULL;
    t = &g_tbs[g_tbCount++];
    ZeroMemory(t, sizeof(*t));
    t->hwnd = h;
    return t;
}

static wchar_t* vb6_TbDup(const wchar_t* s) {
    if (!s || !*s) return NULL;
    int n = lstrlenW(s);
    wchar_t* p = (wchar_t*)HeapAlloc(GetProcessHeap(), 0, (size_t)(n + 1) * sizeof(wchar_t));
    if (p) { CopyMemory(p, s, (size_t)n * sizeof(wchar_t)); p[n] = 0; }
    return p;
}

static void vb6_TbFree(wchar_t** pp) {
    if (*pp) { HeapFree(GetProcessHeap(), 0, *pp); *pp = NULL; }
}

static DWORD vb6_TbStyle(void* hwnd) {
    return (DWORD)(UINT_PTR)GetWindowLongPtrW((HWND)hwnd, GWL_STYLE);
}

static void vb6_TbSetBit(void* hwnd, DWORD mask, int on) {
    if (!hwnd) return;
    DWORD cur = vb6_TbStyle(hwnd);
    DWORD next = on ? (cur | mask) : (cur & ~mask);
    if (next == cur) return;
    SetWindowLongPtrW((HWND)hwnd, GWL_STYLE, (LONG_PTR)next);
    // TBSTYLE_LIST 改的是"文字与图标的排布", 要让控件重算一遍按钮尺寸才见效。
    SendMessageW((HWND)hwnd, TB_AUTOSIZE, 0, 0);
    InvalidateRect((HWND)hwnd, NULL, TRUE);
}

// ---------------- ShowTips ----------------
void vb6_Toolbar_SetShowTips(void* hwnd, int32_t val) {
    vb6_TbSetBit(hwnd, TBSTYLE_TOOLTIPS, val != 0);
}
int32_t vb6_Toolbar_GetShowTips(void* hwnd) {
    if (!hwnd) return -1;                 // VB6 默认 True
    return (vb6_TbStyle(hwnd) & TBSTYLE_TOOLTIPS) ? -1 : 0;
}

// ---------------- TextStyle ----------------
// 0 = tbrTextBelow(默认): 文字在图标下方; 1 = tbrTextRight: 文字在右侧 (TBSTYLE_LIST)。
void vb6_Toolbar_SetTextStyle(void* hwnd, int32_t val) {
    vb6_TbSetBit(hwnd, TBSTYLE_LIST, val != 0);
}
int32_t vb6_Toolbar_GetTextStyle(void* hwnd) {
    if (!hwnd) return 0;
    return (vb6_TbStyle(hwnd) & TBSTYLE_LIST) ? 1 : 0;
}

// ---------------- AllowCustomize ----------------
// 原生 CCS_ADJUSTABLE 只在**创建时**起作用 (运行期拖拽把手的行为是控件在 WM_NCHITTEST
// 里定的)，这里改位是为了让读数与"用户写了什么"一致 —— 真自定义行为等 5b 一起看。
void vb6_Toolbar_SetAllowCustomize(void* hwnd, int32_t val) {
    vb6_TbSetBit(hwnd, CCS_ADJUSTABLE, val != 0);
}
int32_t vb6_Toolbar_GetAllowCustomize(void* hwnd) {
    if (!hwnd) return 0;
    return (vb6_TbStyle(hwnd) & CCS_ADJUSTABLE) ? -1 : 0;
}

// ---------------- Align ----------------
void vb6_Toolbar_SetAlign(void* hwnd, int32_t val) {
    if (!hwnd) return;
    SetPropW((HWND)hwnd, kTbAlign, (HANDLE)(INT_PTR)(val + 1));
}
int32_t vb6_Toolbar_GetAlign(void* hwnd) {
    if (!hwnd) return 0;
    HANDLE h = GetPropW((HWND)hwnd, kTbAlign);
    return h ? (int32_t)(INT_PTR)h - 1 : 1;    // VB6 默认 1 = 靠上
}

// ---------------- 设计期/运行期按钮 ----------------
// VB6 的 Button.Style → 原生 BTNS_*:
//   0 普通 / 1 复选 / 2 按钮组(单选) / 3 分隔符 / 4 占位符 / 5 下拉
static int vb6_TbNativeStyle(int vbStyle) {
    switch (vbStyle) {
        case 1:  return BTNS_CHECK;
        case 2:  return BTNS_GROUP;
        case 3:  return BTNS_SEP;
        case 4:  return BTNS_SEP;          // 占位符在原生里就是"带宽度的分隔符"
        case 5:  return BTNS_DROPDOWN;
        default: return BTNS_BUTTON;
    }
}

int vb6_Toolbar_AddButton(void* hwnd, int32_t index, const wchar_t* key, const wchar_t* caption,
                          int32_t style, int32_t image, const wchar_t* tooltip, int32_t width) {
    if (!hwnd) return 0;
    Vb6Toolbar* t = vb6_TbEnsure((HWND)hwnd);
    if (!t || t->btnCount >= VB6_TB_BTNS_MAX) return 0;

    // index <= 0 = 追加; VB6 的 Buttons.Add 第一形参就是"插到谁前面"的序号。
    int slot = (index > 0 && index - 1 < t->btnCount) ? (index - 1) : t->btnCount;
    if (slot < t->btnCount) {
        MoveMemory(&t->btn[slot + 1], &t->btn[slot],
                   (size_t)(t->btnCount - slot) * sizeof(Vb6TbBtn));
    }
    Vb6TbBtn* b = &t->btn[slot];
    // 上面那次 MoveMemory 把 btn[slot..] 整体**按值**上移一格 —— 槽里的指针此刻被
    // slot 与 slot+1 共同指着，下面逐字段覆盖才把老指针交给 slot+1 独占。tag 是新加的
    // 字段，也必须在这里显式清掉，否则它就成了同一块堆的两任主人 (5b 的 Tag 写进去
    // 再 Remove/Clear 就是二次释放)。
    b->key = vb6_TbDup(key); b->caption = vb6_TbDup(caption); b->tooltip = vb6_TbDup(tooltip);
    b->tag = NULL;
    b->style = (int)style; b->image = (int)image; b->width = (int)width;
    t->btnCount++;

    int sep = ((int)style == 3 || (int)style == 4);
    // **v6 主题下的前置**: C3 产出的 exe 都嵌了 Common-Controls 6.0 的 manifest
    // (driver_link.cpp:780)，而 v6 的工具栏在**没被告知结构体尺寸**之前会收下
    // TB_ADDBUTTONSW 的 TRUE 返回值却一个按钮都不加 (实测: 表里 3 条、TB_BUTTONCOUNT=0，
    // 而不嵌 manifest 的独立 C 探针 (v5) 同一串消息是 1/2/3)。这条幂等、每次一发而已。
    SendMessageW((HWND)hwnd, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTONW), 0);
    TBBUTTONW bt;
    ZeroMemory(&bt, sizeof(bt));
    bt.iBitmap   = sep ? (INT_PTR)width                       // 分隔符: iBitmap 就是宽度
                       : ((image >= 0) ? (INT_PTR)image : (INT_PTR)I_IMAGENONE);
    bt.idCommand = (int)(slot + 1);                            // 1 基: 5b 的 ButtonClick 靠它回推
    bt.fsStyle   = (BYTE)vb6_TbNativeStyle((int)style);
    bt.fsState   = sep ? (BYTE)0 : (BYTE)TBSTATE_ENABLED;
    // 文字进控件自己的字符串表, iString 存它给的偏移 (分隔符不配图)。
    bt.iString   = sep ? 0
        : (INT_PTR)SendMessageW((HWND)hwnd, TB_ADDSTRINGW, 0,
                                (LPARAM)(caption ? caption : L""));

    // 只往控件上发这一条 —— **不能先 TB_RESET 再整表重发**: 实测那样 `TB_ADDBUTTONSW`
    // 返回 TRUE 却一个按钮都不进 (`.build/tbprobe` 那件 C 探针: add=1 而 count=0)，
    // 因为 reset 连字符串表一起清了、随后重发的 iString 全是旧偏移。
    LRESULT ok = (slot >= t->nNative)
        ? SendMessageW((HWND)hwnd, TB_ADDBUTTONSW, 1, (LPARAM)&bt)
        : SendMessageW((HWND)hwnd, TB_INSERTBUTTONW, (WPARAM)slot, (LPARAM)&bt);
    if (!ok) {                                  // 控件没收下 ⇒ 表也要退回去，别让两边分叉
        vb6_TbFree(&b->key); vb6_TbFree(&b->caption); vb6_TbFree(&b->tooltip);
        MoveMemory(&t->btn[slot], &t->btn[slot + 1],
                   (size_t)(t->btnCount - slot) * sizeof(Vb6TbBtn));
        t->btnCount--;
        return 0;
    }
    if (slot >= t->nNative) t->nNative = slot + 1;
    // **v6 的 TB_ADDBUTTONSW / TB_INSERTBUTTONW 不吃调用方给的 fsState**: 实测建完之后
    // TB_GETBUTTONINFOW(TBIF_STATE) 读回 0 (连 TBSTATE_ENABLED 都没有)，于是 5b 的
    // `Button.Enabled` 默认读数会是 False —— 与 VB6 相反。建完补一条 SETBUTTONINFO
    // 把启用位打上去 (SETBUTTONINFO 那一路是认的：5b 的 TB21/TB22 就是它的读数)。
    if (!sep) {
        TBBUTTONINFOW si;
        ZeroMemory(&si, sizeof(si));
        si.cbSize  = sizeof(TBBUTTONINFOW);
        si.dwMask  = TBIF_STATE | TBIF_BYINDEX;
        si.fsState = TBSTATE_ENABLED;
        SendMessageW((HWND)hwnd, TB_SETBUTTONINFOW, (WPARAM)slot, (LPARAM)&si);
    }
    InvalidateRect((HWND)hwnd, NULL, TRUE);
    return slot + 1;                            // 5b: Buttons.Add 的返回值 = 1 基序号
}

// 原生侧的按钮数 —— 判据用它证"设计期那几条真进了控件"（VB 侧的 Buttons.Count 属于 5b）。
int32_t vb6_Toolbar_GetButtonCount(void* hwnd) {
    if (!hwnd) return 0;
    return (int32_t)(INT_PTR)SendMessageW((HWND)hwnd, TB_BUTTONCOUNT, 0, 0);
}

// ---------------- 设计期初值 ----------------
// 哨兵 -999 = .frm 里没写 (与 C29-8a 同一口径：不能用 -1，VB6 的 True 就是 -1)。
void vb6_Toolbar_Init(void* hwnd, int32_t showTips, int32_t textStyle,
                      int32_t allowCustomize, int32_t align) {
    if (!hwnd) return;
    if (showTips       != -999) vb6_Toolbar_SetShowTips(hwnd, showTips);
    if (textStyle      != -999) vb6_Toolbar_SetTextStyle(hwnd, textStyle);
    if (allowCustomize != -999) vb6_Toolbar_SetAllowCustomize(hwnd, allowCustomize);
    if (align          != -999) vb6_Toolbar_SetAlign(hwnd, align);
}

/* ============================================================================
 * C29-5b: Buttons 集合与 Button 对象 —— 复用 vb6forms_memberobj.c 那套真 IDispatch
 * 成员对象机制 (与 C29-3 ImageList / C29-7 ListView / C29-4 Panel / C29-8b Node 同族)。
 * 集合入口 vb6_Toolbar_Buttons() 由 memberobj.c 提供，本文件只交数据面。
 *
 * 读数分两处，分界线是"原生答不答得了"：
 *   Text / Enabled / Visible / Image  —— **现问控件** (TB_GETBUTTONINFOW)，改也同步下去
 *   Style / Key / Tag / ToolTipText / Width —— 住在 5a 那张表里
 * Style 为什么不走原生：原生 fsStyle 分不出"分隔符"(BTNS_SEP) 与"占位符"(VB6 的 4 ——
 * 5a 把两者都发成 BTNS_SEP，只在 iBitmap/cx 里带宽度)，问原生就丢一档。ToolTipText 是
 * VB 侧字符串 (原生要在 TTN_GETDISPINFO 里回文本，那是 ButtonClick 那一格的事)。
 *
 * 一律按**索引**找按钮 (TBIF_BYINDEX)：idCommand 虽然发的是 1 基序号，但分隔符在 VB6 里
 * 也可以有 id，靠 id 找会撞车。
 * ==========================================================================*/

#define VB6_TB_TEXT_MAX 256
// 唯一的消费者是 memberobj 的 memSetStr，它当场拷成自己的 BSTR；单线程 UI 代码，
// 一个静态缓冲足够 (与 8b 那几个"表内自有指针"同一口径)。
static wchar_t g_tbText[VB6_TB_TEXT_MAX];

static Vb6TbBtn* vb6_TbBtnAt(void* hwnd, int32_t idx) {
    if (!hwnd || idx < 1) return NULL;
    Vb6Toolbar* t = vb6_TbFind((HWND)hwnd);
    if (!t || idx > t->btnCount) return NULL;
    return &t->btn[idx - 1];
}

static BOOL vb6_TbGetInfo(HWND h, int32_t idx, TBBUTTONINFOW* bi, DWORD mask) {
    ZeroMemory(bi, sizeof(*bi));
    bi->cbSize    = sizeof(TBBUTTONINFOW);
    bi->dwMask    = mask | TBIF_BYINDEX;
    bi->idCommand = (int)idx - 1;        // TBIF_BYINDEX ⇒ 这一栏是 0 基索引
    return SendMessageW(h, TB_GETBUTTONINFOW, (WPARAM)idx - 1, (LPARAM)bi) != (LRESULT)-1;
}

static BOOL vb6_TbSetInfo(HWND h, int32_t idx, TBBUTTONINFOW* bi) {
    if (!h) return FALSE;
    bi->cbSize = sizeof(TBBUTTONINFOW);
    bi->dwMask |= TBIF_BYINDEX;
    SendMessageW(h, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTONW), 0);   // v6 前置，幂等
    BOOL ok = SendMessageW(h, TB_SETBUTTONINFOW, (WPARAM)idx - 1, (LPARAM)bi) != 0;
    InvalidateRect(h, NULL, FALSE);
    return ok;
}

int32_t vb6_Toolbar_ButtonCount(void* hwnd) {
    return vb6_Toolbar_GetButtonCount(hwnd);          // 就是 TB_BUTTONCOUNT，不另数一遍表
}

int32_t vb6_Toolbar_ButtonIndexByKey(void* hwnd, const wchar_t* key) {
    if (!hwnd || !key || !*key) return 0;
    Vb6Toolbar* t = vb6_TbFind((HWND)hwnd);
    if (!t) return 0;
    for (int i = 0; i < t->btnCount; i++)
        if (t->btn[i].key && lstrcmpiW(t->btn[i].key, key) == 0) return (int32_t)(i + 1);
    return 0;
}

// ---------------- Text / Image / Enabled / Visible：现问控件 ----------------
const wchar_t* vb6_Toolbar_GetButtonCaption(void* hwnd, int32_t idx) {
    g_tbText[0] = 0;
    if (!hwnd) return g_tbText;
    wchar_t buf[VB6_TB_TEXT_MAX];
    buf[0] = 0;
    TBBUTTONINFOW bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.cbSize    = sizeof(TBBUTTONINFOW);
    bi.dwMask    = TBIF_TEXT | TBIF_BYINDEX;
    bi.idCommand = (int)idx - 1;
    bi.pszText   = buf;
    bi.cchText   = VB6_TB_TEXT_MAX;
    // pszText 必须**一上来就带缓冲**：TB_GETBUTTONINFOW 只填它指的那块内存，
    // 先问一次"有没有"再问文本是错的用法 (第一趟就会往 NULL 里写)。
    if (SendMessageW((HWND)hwnd, TB_GETBUTTONINFOW, (WPARAM)idx - 1, (LPARAM)&bi) != (LRESULT)-1)
        lstrcpynW(g_tbText, buf, VB6_TB_TEXT_MAX);
    return g_tbText;
}

void vb6_Toolbar_SetButtonCaption(void* hwnd, int32_t idx, const wchar_t* v) {
    Vb6TbBtn* b = vb6_TbBtnAt(hwnd, idx);
    if (!b) return;
    vb6_TbFree(&b->caption);
    b->caption = vb6_TbDup(v);                       // 表里那份跟着走，Remove/清表要放它
    TBBUTTONINFOW bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.dwMask  = TBIF_TEXT;
    bi.pszText = (LPWSTR)(v ? v : (wchar_t*)L"");
    vb6_TbSetInfo((HWND)hwnd, idx, &bi);
}

int32_t vb6_Toolbar_GetButtonImage(void* hwnd, int32_t idx) {
    if (!hwnd) return -1;
    TBBUTTONINFOW bi;
    if (!vb6_TbGetInfo((HWND)hwnd, idx, &bi, TBIF_IMAGE)) return -1;
    return (bi.iImage == (int)I_IMAGENONE) ? -1 : (int32_t)bi.iImage;
}

void vb6_Toolbar_SetButtonImage(void* hwnd, int32_t idx, int32_t v) {
    Vb6TbBtn* b = vb6_TbBtnAt(hwnd, idx);
    if (!b) return;
    b->image = (int)v;
    TBBUTTONINFOW bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.dwMask = TBIF_IMAGE;
    bi.iImage = (v >= 0) ? (int)v : (int)I_IMAGENONE;
    vb6_TbSetInfo((HWND)hwnd, idx, &bi);
}

int32_t vb6_Toolbar_GetButtonEnabled(void* hwnd, int32_t idx) {
    if (!hwnd) return 0;
    TBBUTTONINFOW bi;
    return vb6_TbGetInfo((HWND)hwnd, idx, &bi, TBIF_STATE) && (bi.fsState & TBSTATE_ENABLED) ? -1 : 0;
}

void vb6_Toolbar_SetButtonEnabled(void* hwnd, int32_t idx, int32_t v) {
    if (!hwnd) return;
    TBBUTTONINFOW bi;
    if (!vb6_TbGetInfo((HWND)hwnd, idx, &bi, TBIF_STATE)) return;
    bi.fsState = (BYTE)((v != 0) ? (bi.fsState | TBSTATE_ENABLED)
                                 : (bi.fsState & (BYTE)~TBSTATE_ENABLED));
    bi.dwMask  = TBIF_STATE;
    vb6_TbSetInfo((HWND)hwnd, idx, &bi);
}

int32_t vb6_Toolbar_GetButtonVisible(void* hwnd, int32_t idx) {
    if (!hwnd) return -1;
    TBBUTTONINFOW bi;
    if (!vb6_TbGetInfo((HWND)hwnd, idx, &bi, TBIF_STATE)) return 0;
    return (bi.fsState & TBSTATE_HIDDEN) ? 0 : -1;
}

void vb6_Toolbar_SetButtonVisible(void* hwnd, int32_t idx, int32_t v) {
    if (!hwnd) return;
    TBBUTTONINFOW bi;
    if (!vb6_TbGetInfo((HWND)hwnd, idx, &bi, TBIF_STATE)) return;
    bi.fsState = (BYTE)((v != 0) ? (bi.fsState & (BYTE)~TBSTATE_HIDDEN)
                                 : (bi.fsState | TBSTATE_HIDDEN));
    bi.dwMask  = TBIF_STATE;
    vb6_TbSetInfo((HWND)hwnd, idx, &bi);
}

// ---------------- Style / Key / Tag / ToolTipText / Width：住在表里 ----------------
int32_t vb6_Toolbar_GetButtonStyle(void* hwnd, int32_t idx) {
    Vb6TbBtn* b = vb6_TbBtnAt(hwnd, idx);
    return b ? (int32_t)b->style : -1;
}

void vb6_Toolbar_SetButtonStyle(void* hwnd, int32_t idx, int32_t v) {
    Vb6TbBtn* b = vb6_TbBtnAt(hwnd, idx);
    if (!b) return;
    b->style = (int)v;                               // VB 那一套 0..5 (占位符只有表里分得出)
    TBBUTTONINFOW bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.dwMask  = TBIF_STYLE;
    bi.fsStyle = (BYTE)vb6_TbNativeStyle((int)v);
    vb6_TbSetInfo((HWND)hwnd, idx, &bi);
}

const wchar_t* vb6_Toolbar_GetButtonKey(void* hwnd, int32_t idx) {
    Vb6TbBtn* b = vb6_TbBtnAt(hwnd, idx);
    return (b && b->key) ? b->key : L"";
}
void vb6_Toolbar_SetButtonKey(void* hwnd, int32_t idx, const wchar_t* v) {
    Vb6TbBtn* b = vb6_TbBtnAt(hwnd, idx);
    if (!b) return;
    vb6_TbFree(&b->key);
    b->key = vb6_TbDup(v);
}
const wchar_t* vb6_Toolbar_GetButtonTag(void* hwnd, int32_t idx) {
    Vb6TbBtn* b = vb6_TbBtnAt(hwnd, idx);
    return (b && b->tag) ? b->tag : L"";
}
void vb6_Toolbar_SetButtonTag(void* hwnd, int32_t idx, const wchar_t* v) {
    Vb6TbBtn* b = vb6_TbBtnAt(hwnd, idx);
    if (!b) return;
    vb6_TbFree(&b->tag);
    b->tag = vb6_TbDup(v);
}
const wchar_t* vb6_Toolbar_GetButtonToolTip(void* hwnd, int32_t idx) {
    Vb6TbBtn* b = vb6_TbBtnAt(hwnd, idx);
    return (b && b->tooltip) ? b->tooltip : L"";
}
void vb6_Toolbar_SetButtonToolTip(void* hwnd, int32_t idx, const wchar_t* v) {
    Vb6TbBtn* b = vb6_TbBtnAt(hwnd, idx);
    if (!b) return;
    vb6_TbFree(&b->tooltip);
    b->tooltip = vb6_TbDup(v);
}
int32_t vb6_Toolbar_GetButtonWidth(void* hwnd, int32_t idx) {
    Vb6TbBtn* b = vb6_TbBtnAt(hwnd, idx);
    return b ? (int32_t)b->width : 0;
}
void vb6_Toolbar_SetButtonWidth(void* hwnd, int32_t idx, int32_t v) {
    Vb6TbBtn* b = vb6_TbBtnAt(hwnd, idx);
    if (!b) return;
    b->width = (int)v;
    TBBUTTONINFOW bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.dwMask = TBIF_SIZE;
    bi.cx     = (WORD)v;
    vb6_TbSetInfo((HWND)hwnd, idx, &bi);
}

// Value = VB6 那一套里"复选按钮勾上没有"，落在原生 TBSTATE_CHECKED 位上 (与 Enabled/
// Visible 同一族：读改写整个 fsState，TBBUTTONINFO 没有 stateMask 这一栏)。
#ifndef TBSTATE_CHECKED
#define TBSTATE_CHECKED   0x0001
#endif
int32_t vb6_Toolbar_GetButtonValue(void* hwnd, int32_t idx) {
    if (!hwnd) return 0;
    TBBUTTONINFOW bi;
    return vb6_TbGetInfo((HWND)hwnd, idx, &bi, TBIF_STATE) && (bi.fsState & TBSTATE_CHECKED) ? -1 : 0;
}

void vb6_Toolbar_SetButtonValue(void* hwnd, int32_t idx, int32_t v) {
    if (!hwnd) return;
    TBBUTTONINFOW bi;
    if (!vb6_TbGetInfo((HWND)hwnd, idx, &bi, TBIF_STATE)) return;
    bi.fsState = (BYTE)((v != 0) ? (bi.fsState | TBSTATE_CHECKED)
                                 : (bi.fsState & (BYTE)~TBSTATE_CHECKED));
    bi.dwMask  = TBIF_STATE;
    vb6_TbSetInfo((HWND)hwnd, idx, &bi);
}

// ---------------- Remove / Clear ----------------
// 控件与表一起退格：先 TB_DELETEBUTTON (按索引)，再把表里那一格往后挪。
// 与 8b 的 RemoveNode 同一理由 —— 只动一边，Count 与屏幕立刻分叉。
int32_t vb6_Toolbar_RemoveButton(void* hwnd, int32_t idx) {
    Vb6Toolbar* t = hwnd ? vb6_TbFind((HWND)hwnd) : NULL;
    if (!t || idx < 1 || idx > t->btnCount) return 0;
    SendMessageW((HWND)hwnd, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTONW), 0);
    if (!SendMessageW((HWND)hwnd, TB_DELETEBUTTON, (WPARAM)idx - 1, 0)) return 0;
    Vb6TbBtn* b = &t->btn[idx - 1];
    vb6_TbFree(&b->key); vb6_TbFree(&b->caption); vb6_TbFree(&b->tooltip); vb6_TbFree(&b->tag);
    MoveMemory(&t->btn[idx - 1], &t->btn[idx],
               (size_t)(t->btnCount - idx) * sizeof(Vb6TbBtn));
    t->btnCount--;
    if (t->nNative > 0) t->nNative--;
    InvalidateRect((HWND)hwnd, NULL, TRUE);
    return -1;
}

void vb6_Toolbar_ClearButtons(void* hwnd) {
    Vb6Toolbar* t = hwnd ? vb6_TbFind((HWND)hwnd) : NULL;
    if (!t) return;
    // 从后往前删：TB_DELETEBUTTON 按索引，正着删会把后面的格一个个跳过去。
    while (t->btnCount > 0) {
        if (!SendMessageW((HWND)hwnd, TB_DELETEBUTTON, (WPARAM)t->btnCount - 1, 0)) break;
        Vb6TbBtn* b = &t->btn[t->btnCount - 1];
        vb6_TbFree(&b->key); vb6_TbFree(&b->caption); vb6_TbFree(&b->tooltip); vb6_TbFree(&b->tag);
        t->btnCount--;
    }
    t->nNative = 0;
    InvalidateRect((HWND)hwnd, NULL, TRUE);
}


// ---------------- C29-5c: 两条按钮事件的判据辅助 ----------------
// 无头环境点不了鼠标，而直接调 handler 会绕开整条派发链。这里的做法是**照真控件的样子把
// 消息发给父窗**，让 cgen 那两处派发分支各跑一遍 (手法照 C29-8c 的 TvSendNotify)。
//
// **两条事件不在同一条通道上** —— 原先 #89 记的"跟 TreeView 一样共用 WM_NOTIFY"只对了一半：
//   ButtonClick      → WM_COMMAND, LOWORD(wParam)=按钮的 idCommand, HIWORD=0, lParam=工具栏 HWND
//   ButtonMenuClick  → WM_NOTIFY,  hdr.code=TBN_DROPDOWN(-710), hdr.idFrom=同一个 idCommand
//                      (只有 BTNS_DROPDOWN = VB6 的 Style 5 那种带下拉箭头的按钮发得出来)
//
// **为什么不向控件现问 idCommand**：试过节的两条路，都在本产品 (嵌了 Common-Controls 6.0
// manifest 的产物) 里问不出东西 ——
//   · `TB_GETBUTTON` 在不嵌 manifest 的独立探针里 rc=1 / idCommand=1..3 / 越界 rc=0，看着全对，
//     而产物里同一句一条都不回 (探针与产物不同 comctl 版本这条老嫌疑，见 ai/029 §三 D3)；
//   · `TB_GETBUTTONINFO + TBIF_COMMAND|TBIF_STYLE|TBIF_BYINDEX` 的实测读数是
//     `rc=0/1/2 且结构体一个字段都没被填` (idx=1/2/3 ⇒ rc 恰好等于传进去的 wParam)，
//     也就是这条 GET 压根不认 TBIF_COMMAND。⇒ 判据改用**表**：序号边界问 `vb6_TbBtnAt`、
//     下拉位问 `b->style == 5`。这两条各自都能被读数证伪，而"控件里的 idCommand 就是槽号+1"
//     由 `Buttons.Count`(TB_BUTTONCOUNT) 与表数一致 (TB34) 从另一侧钉住。
static Vb6TbBtn* vb6_TbSimTarget(void* hwnd, int32_t idx, int wantDropdown) {
    Vb6TbBtn* b = vb6_TbBtnAt(hwnd, idx);
    if (!b) return NULL;
    // 真控件不会为普通按钮发 TBN_DROPDOWN，判据也不发 —— 否则那条读数成了自证。
    if (wantDropdown && b->style != 5) return NULL;
    if (!wantDropdown && b->style == 3) return NULL;   // 分隔符按不动
    return b;
}

void vb6_Toolbar_SimButtonClick(void* hwnd, int32_t idx) {
    HWND parent;
    if (!vb6_TbSimTarget(hwnd, idx, 0)) return;
    parent = GetParent((HWND)hwnd);
    if (!parent) return;
    SendMessageW(parent, WM_COMMAND, (WPARAM)MAKEWPARAM((WORD)idx, 0), (LPARAM)(HWND)hwnd);
}

void vb6_Toolbar_SimButtonMenuClick(void* hwnd, int32_t idx) {
    NMTOOLBARW nt;
    HWND parent;
    if (!vb6_TbSimTarget(hwnd, idx, 1)) return;
    parent = GetParent((HWND)hwnd);
    if (!parent) return;
    ZeroMemory(&nt, sizeof(nt));
    nt.hdr.hwndFrom = (HWND)hwnd;
    nt.hdr.idFrom   = (UINT_PTR)idx;
    nt.hdr.code     = (DWORD)TBN_DROPDOWN;
    SendMessageW(parent, WM_NOTIFY, 0, (LPARAM)&nt);
}


#endif // _WIN32
