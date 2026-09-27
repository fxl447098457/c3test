// vb6forms_dtpicker.c — ai/029 C29-DT-a: VB6 DTPicker 的窗口 + 样式 + 标量属性面
//
// 复刻口径（ai/内置控件/DTPicker 控件（日期时间选择器）.md + 029 §三 D6）：
//   **不加载 MSCOMCT2.OCX** —— 那是 32 位 inproc，x64 进程里 CoCreateInstance 直接失败。
//   原生等价类 = comctl32 注册的 SysDateTimePick32（SDK 头 commctrl.h:6653
//   `DATETIMEPICK_CLASSW`），类已由 vb6forms.c:67 那次 InitCommonControlsEx 的
//   ICC_DATE_CLASSES 请求过 ⇒ 与 TreeView/SSTab 同型，不需要 RTL 自注册兜底。
//
// 本格范围 = **不吃 Date 型值的那几格**（Value / MinDate / MaxDate 留 C29-DT-b）：
//   Format      0=dtpShortDate 1=dtpLongDate 2=dtpTime 3=dtpCustom → DTS_* 格式位 + 自定义串
//   CustomFormat String → DTM_SETFORMATW（DTM_SETFORMAT 只有 Set 没有 Get ⇒ 串自存，见下）
//   CheckBox    Boolean → DTS_SHOWNONE（勾掉 = GDT_NONE = VB6 那个"空日期"）
//   UpDown      Boolean → DTS_UPDOWN（不弹月历，改微调按钮）
//   Calendar*Color  五色 → DTM_SETMCCOLOR / DTM_GETMCCOLOR（下拉月历的颜色槽，与 MonthCal 同一张表）
//
// "窗口真建起来了"怎么证：不另开读数口子 —— 样式类属性走"写进 GWL_STYLE 再读回来"，
//   句柄为 NULL 时那条写是空转、读必回默认，写读不一致就翻红；类名字面串由 emitc 形状针钉住
//   （vb6_CreateControl("SysDateTimePick32"…)），两头夹住中间那段"建错类还能自洽"的空子。
//
// 真值放哪（029 那三条控件线通用口径）：格式位 / SHOWNONE / UPDOWN 的真值**就是窗口样式位**，
//   getter 直读 GWL_STYLE ⇒ 读数与观感同源。两处原生问不出来、只能自存：
//   ① `Format = 3 (dtpCustom)` —— 原生没有"自定义"这一位，自定义与否等于"有没有下发过格式串"；
//   ② CustomFormat 本身 —— commctrl.h 只有 DTM_SETFORMATW，**没有** DTM_GETFORMAT
//   （实测头里 0x1000 那一族从 +1 到 +15 没有 Get 对称项）⇒ 串存窗口属性。

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#endif

#include "vb6forms.h"
#include "vb6forms_internal.h"
#include <oleauto.h>   /* SysAllocString / SysFreeString */
#include <string.h>

#ifdef _WIN32

// commctrl.h 在低 _WIN32_IE 下不给这几位，且 cgen 那侧按字面量发 —— 两边都钉死数值。
// 数值全部抄自 D:\Windows Kits\10\Include\10.0.19041.0\um\commctrl.h。
#ifndef DTS_UPDOWN
#define DTS_UPDOWN                  0x0001L
#endif
#ifndef DTS_SHOWNONE
#define DTS_SHOWNONE                0x0002L
#endif
#ifndef DTS_SHORTDATEFORMAT
#define DTS_SHORTDATEFORMAT         0x0000L
#endif
#ifndef DTS_LONGDATEFORMAT
#define DTS_LONGDATEFORMAT          0x0004L
#endif
#ifndef DTS_TIMEFORMAT
#define DTS_TIMEFORMAT              0x0009L
#endif
#ifndef DTM_SETFORMATW
#define DTM_SETFORMATW              (0x1000 + 50)
#endif
#ifndef DTM_SETMCCOLOR
#define DTM_SETMCCOLOR              (0x1000 + 6)
#endif
#ifndef DTM_GETIDEALSIZE
#define DTM_GETIDEALSIZE            (0x1000 + 15)
#endif
#ifndef DTM_GETMCCOLOR
#define DTM_GETMCCOLOR              (0x1000 + 7)
#endif

// MonthCal 系（下拉月历）的颜色槽编号。**SDK 头里没有这些名字**（实测 grep MCMC 零命中），
// 只有文档与控件实现认这套序号 ⇒ 数值在此钉死，序号含义由夹具的"设一格、其余六格不动"
// 那条读数交叉验证（不是靠回忆）。
#define VB6_DTP_MC_BACKGROUND    0   // 月历底色
#define VB6_DTP_MC_TEXT          1   // 日期字色
#define VB6_DTP_MC_TRAILINGDAY   2   // 非本月日期字色
#define VB6_DTP_MC_HEADERTEXT    3   // 星期表头字色（VB6 无对应属性）
#define VB6_DTP_MC_TITLEBACK     4   // 年月标题底色
#define VB6_DTP_MC_TITLETEXT     5   // 年月标题字色
#define VB6_DTP_MC_TODAYTEXT     6   // "今天"字色（VB6 无对应属性）

// 格式位不是一段连续域：SHORT=0 / LONG=4 / TIME=9 / CENTURY=0xC，
// 合起来占 bit0、bit2、bit3 ⇒ 掩码 0x000D。**bit0 与 DTS_UPDOWN 撞**这一条是
// SDK 常数本身的形状（TIMEFORMAT 就带 0x1），不是本文件的处理手法，故切格式时
// 只清 bit2/bit3 那一段（0x000C）而**保留 bit0**，免得选个 TIME 就把微调按钮关掉。
#define VB6_DTP_FMTMASK          0x000CL

static const wchar_t kDtpCustomFormat[] = L"VB6_DTP_CustomFormat";

static DWORD vb6_DtpStyle(void* hwnd) {
    return (DWORD)(UINT_PTR)GetWindowLongPtrW((HWND)hwnd, GWL_STYLE);
}

// 样式位一改就要重算非客户区并强制重绘（与 TreeView 同一手法）：日期选择框的
// 显示区宽度按"当前格式"算，只 SetWindowLong 的话旧宽度还在那儿摆着。
static void vb6_DtpSetBits(void* hwnd, DWORD mask, DWORD on) {
    if (!hwnd) return;
    DWORD cur = vb6_DtpStyle(hwnd);
    DWORD next = (cur & ~mask) | (on & mask);
    if (next == cur) return;
    SetWindowLongPtrW((HWND)hwnd, GWL_STYLE, (LONG_PTR)next);
    SetWindowPos((HWND)hwnd, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    InvalidateRect((HWND)hwnd, NULL, TRUE);
}

static void vb6_DtpSetBit(void* hwnd, DWORD bit, int on) {
    vb6_DtpSetBits(hwnd, bit, on ? bit : 0);
}

// VB6 的 True 是 -1，不是 1（与 vb6_GetControlVisible / vb6_TvBitAsVbBool 同口径）。
static int32_t vb6_DtpBitAsVbBool(void* hwnd, DWORD bit, int32_t defIfNoHwnd) {
    if (!hwnd) return defIfNoHwnd;
    return (vb6_DtpStyle(hwnd) & bit) ? -1 : 0;
}

// ---------------- CustomFormat ----------------
// 自存的宽字符串。换掉旧值时把旧堆块放掉（照 CommonDialog 那族窗口属性存串的手法，
// 但那一族没有释放路径 —— 这里补上，属性可以在运行期反复下发）。
static wchar_t* vb6_DtpDup(const wchar_t* s) {
    size_t n;
    wchar_t* b;
    if (!s) s = L"";
    n = wcslen(s) + 1;
    b = (wchar_t*)HeapAlloc(GetProcessHeap(), 0, n * sizeof(wchar_t));
    if (b) wcscpy_s(b, n, s);
    return b;
}

static void vb6_DtpFreeStored(void* hwnd, const wchar_t* key) {
    HANDLE old = GetPropW((HWND)hwnd, key);
    if (old) {
        RemovePropW((HWND)hwnd, key);
        HeapFree(GetProcessHeap(), 0, old);
    }
}

// BSTR → 宽字符（cgen 传进来的实参是 void* 装箱的 BSTR；NULL 串按空串处理）。
void vb6_DTP_SetCustomFormat(void* hwnd, void* bstr) {
    const wchar_t* s = (const wchar_t*)bstr;
    if (!hwnd) return;
    vb6_DtpFreeStored(hwnd, kDtpCustomFormat);
    SetPropW((HWND)hwnd, kDtpCustomFormat, (HANDLE)vb6_DtpDup(s));
    // 下发了格式串 = VB6 侧的 dtpCustom。空串也照发（DTM_SETFORMATW 传 NULL 才"恢复默认"，
    // 传空串是把显示区清成空），所以**有没有存过串**就是 custom 的那一位，不做特判。
    SendMessageW((HWND)hwnd, DTM_SETFORMATW, 0, (LPARAM)(s ? s : L""));
}

wchar_t* vb6_DTP_GetCustomFormat(void* hwnd) {
    HANDLE h;
    if (!hwnd) return SysAllocString(L"");
    h = GetPropW((HWND)hwnd, kDtpCustomFormat);
    return SysAllocString(h ? (const wchar_t*)h : (const wchar_t*)L"");
}

// ---------------- Format ----------------
// 0 = dtpShortDate / 1 = dtpLongDate / 2 = dtpTime / 3 = dtpCustom。
// 3 不动样式位（自定义串自己决定画成什么），只认"下发过串"这一状态。
void vb6_DTP_SetFormat(void* hwnd, int32_t val) {
    if (!hwnd) return;
    if (val == 3) {
        HANDLE h = GetPropW((HWND)hwnd, kDtpCustomFormat);
        if (h) SendMessageW((HWND)hwnd, DTM_SETFORMATW, 0, (LPARAM)(const wchar_t*)h);
        return;
    }
    // 切回内建格式：把自存的串撤掉，否则 getter 还会报 3。
    vb6_DtpFreeStored(hwnd, kDtpCustomFormat);
    switch (val) {
        case 1:  vb6_DtpSetBits(hwnd, VB6_DTP_FMTMASK, DTS_LONGDATEFORMAT); break;
        case 2:  vb6_DtpSetBits(hwnd, VB6_DTP_FMTMASK, DTS_TIMEFORMAT & VB6_DTP_FMTMASK); break;
        default: vb6_DtpSetBits(hwnd, VB6_DTP_FMTMASK, DTS_SHORTDATEFORMAT); break;
    }
}

int32_t vb6_DTP_GetFormat(void* hwnd) {
    DWORD fmt;
    if (!hwnd) return 0;
    if (GetPropW((HWND)hwnd, kDtpCustomFormat)) return 3;
    fmt = vb6_DtpStyle(hwnd) & VB6_DTP_FMTMASK;
    if (fmt == (DWORD)DTS_LONGDATEFORMAT) return 1;
    // TIME 的 0x9 里 bit0 是 UPDOWN 那位，只比 bit2/3 那一段（0x8 且 LONG 的 0x4 不成立）
    if ((fmt & 0x8L) && !(fmt & 0x4L)) return 2;
    return 0;
}

// ---------------- CheckBox / UpDown ----------------
int32_t vb6_DTP_GetCheckBox(void* hwnd) { return vb6_DtpBitAsVbBool(hwnd, DTS_SHOWNONE, 0); }
void    vb6_DTP_SetCheckBox(void* hwnd, int32_t on) { vb6_DtpSetBit(hwnd, DTS_SHOWNONE, on != 0); }

int32_t vb6_DTP_GetUpDown(void* hwnd) { return vb6_DtpBitAsVbBool(hwnd, DTS_UPDOWN, 0); }
void    vb6_DTP_SetUpDown(void* hwnd, int32_t on) { vb6_DtpSetBit(hwnd, DTS_UPDOWN, on != 0); }

// ---------------- 下拉月历五色 ----------------
static int32_t vb6_DtpGetColor(void* hwnd, int32_t idx) {
    if (!hwnd) return 0;
    return (int32_t)(LRESULT)SendMessageW((HWND)hwnd, DTM_GETMCCOLOR, (WPARAM)idx, 0);
}

static void vb6_DtpSetColor(void* hwnd, int32_t idx, int32_t clr) {
    if (!hwnd) return;
    SendMessageW((HWND)hwnd, DTM_SETMCCOLOR, (WPARAM)idx, (LPARAM)(COLORREF)clr);
}

int32_t vb6_DTP_GetCalendarBackColor(void* hwnd);
void    vb6_DTP_SetCalendarBackColor(void* hwnd, int32_t v);
int32_t vb6_DTP_GetCalendarForeColor(void* hwnd);
void    vb6_DTP_SetCalendarForeColor(void* hwnd, int32_t v);
int32_t vb6_DTP_GetCalendarTrailingForeColor(void* hwnd);
void    vb6_DTP_SetCalendarTrailingForeColor(void* hwnd, int32_t v);
int32_t vb6_DTP_GetCalendarTitleBackColor(void* hwnd);
void    vb6_DTP_SetCalendarTitleBackColor(void* hwnd, int32_t v);
int32_t vb6_DTP_GetCalendarTitleForeColor(void* hwnd);
void    vb6_DTP_SetCalendarTitleForeColor(void* hwnd, int32_t v);

int32_t vb6_DTP_GetCalendarBackColor(void* hwnd)        { return vb6_DtpGetColor(hwnd, VB6_DTP_MC_BACKGROUND); }
void    vb6_DTP_SetCalendarBackColor(void* hwnd, int32_t v)   { vb6_DtpSetColor(hwnd, VB6_DTP_MC_BACKGROUND, v); }
int32_t vb6_DTP_GetCalendarForeColor(void* hwnd)        { return vb6_DtpGetColor(hwnd, VB6_DTP_MC_TEXT); }
void    vb6_DTP_SetCalendarForeColor(void* hwnd, int32_t v)   { vb6_DtpSetColor(hwnd, VB6_DTP_MC_TEXT, v); }
int32_t vb6_DTP_GetCalendarTrailingForeColor(void* hwnd){ return vb6_DtpGetColor(hwnd, VB6_DTP_MC_TRAILINGDAY); }
void    vb6_DTP_SetCalendarTrailingForeColor(void* hwnd, int32_t v) { vb6_DtpSetColor(hwnd, VB6_DTP_MC_TRAILINGDAY, v); }
int32_t vb6_DTP_GetCalendarTitleBackColor(void* hwnd)   { return vb6_DtpGetColor(hwnd, VB6_DTP_MC_TITLEBACK); }
void    vb6_DTP_SetCalendarTitleBackColor(void* hwnd, int32_t v)    { vb6_DtpSetColor(hwnd, VB6_DTP_MC_TITLEBACK, v); }
int32_t vb6_DTP_GetCalendarTitleForeColor(void* hwnd)   { return vb6_DtpGetColor(hwnd, VB6_DTP_MC_TITLETEXT); }
void    vb6_DTP_SetCalendarTitleForeColor(void* hwnd, int32_t v)    { vb6_DtpSetColor(hwnd, VB6_DTP_MC_TITLETEXT, v); }

// ---------------- 设计期初值 ----------------
// 只剩 CustomFormat 一条。CheckBox / UpDown **只能在创建时给**（它们是复选框/微调按钮那两枚
// 子窗口的创建参数，事后写 GWL_STYLE 会被控件抹回去 —— 判据读数记在 029 §九 本格），
// Format 切档运行期有效、但设计期值本来就该在创建立住 ⇒ 三条都由 cgen 写进创建样式。
// 空串不发（DTM_SETFORMATW 传空串会把显示区清成空），未写就是原生默认。
void vb6_DTP_Init(void* hwnd, const wchar_t* customFormat) {
    if (!hwnd) return;
    if (customFormat && customFormat[0] != L'\0') vb6_DTP_SetCustomFormat(hwnd, (void*)customFormat);
}

// ---------------- 格式真选中了吗（判据用） ----------------
// DTM_GETIDEALSIZE 问的是控件自己算出的"刚好装得下显示内容"的宽度 —— 长日期明显比时间宽，
// 于是"格式位到底选中了哪一档"第一次有了**控件侧**的读数。为什么非要它：
// Format 的 getter 读 GWL_STYLE，那只证明"我们写进去了"；而 SDK 常数 DTS_TIMEFORMAT = 0x0009
// 自带 bit0（= DTS_UPDOWN），格式段与它撞在同一处 => 只对自己的掩码读数会自洽地假绿。
int32_t vb6_DTP_IdealWidth(void* hwnd) {
    SIZE sz;
    if (!hwnd) return 0;
    memset(&sz, 0, sizeof(sz));
    if (!SendMessageW((HWND)hwnd, DTM_GETIDEALSIZE, 0, (LPARAM)&sz)) return 0;
    return (int32_t)sz.cx;
}

#endif /* _WIN32 */
