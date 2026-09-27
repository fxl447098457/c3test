// vb6forms_richtextbox.c — ai/029 C29-RT-a: VB6 RichTextBox 的窗口 + 创建样式 + 文本/选区面
//
// 复刻口径（ai/内置控件/RichTextBox 控件（富文本框）.md + 029 §三 D6）：
//   **不加载 RICHTX32.OCX**（32 位 inproc，x64 里 CoCreateInstance 直接失败）。
//   原生落点 = Msftedit.dll 注册的 RICHEDIT50W（richedit.h:41 MSFTEDIT_CLASS）。
//   类由 vb6_ComCtl_Init 里那次 LoadLibraryW(L"Msftedit.dll") 注册 —— 它**不在 comctl32 的
//   ICC_* 体系里**（实测：不 LoadLibrary 时 GetClassInfoW 问不到这个类），是这枚控件与
//   DTPicker/MonthView 唯一的通道差别。
//
// 本格范围 = 文本 / 选区 / 只读 / 上限 / 滚动条 / 自动换行。
//   Sel* 的**格式**面（粗斜体下划线删除线、颜色、字体、字号、对齐、缩进）→ C29-RT-b；
//   TextRTF + LoadFile/SaveFile + Find → C29-RT-c；Change / SelChange 两条事件 → C29-RT-d。
//
// 三条量出来的口径，判据的形状是它们决定的（三轮探针，见 029 §九 本格）：
//   ① **滚动条是控件自己的东西，样式位会被它抹掉**。创建时给了 WS_VSCROLL，空文本下
//      GWL_STYLE 里那两位就没了（内容不需要滚动 ⇒ 控件把 bar 拆了、连样式位一起清）；
//      灌进 60 行又自己回来。所以 cgen 一并挂 **ES_DISABLENOSCROLL(0x2000)**：bar 常驻、
//      不需要时只是灰着，样式位从此稳定，ScrollBars 才谈得上"读回设计期那个值"。
//      另一头，**事后** SetWindowLong 加那两位只有外观（客户区让位 21px），量程停在默认的
//      0..100 而内容真实高度是 0..1281 —— 看着像有、滚不动。所以这四位没有写口，判据也
//      不只问样式位：VScrollRange / HScrollRange（GetScrollInfo 的 nMax）是控件自己的答案。
//   ② ReadOnly 走 EM_SETREADONLY **事后有效**（样式位读写都跟得上）—— 与 ① 正相反，
//      同一枚控件里两种属性各有各的"事后行不行"，不许互相外推。
//   ③ WordWrap 原生**没有** Get 对称项（EM_SETTARGETDEVICE 单向；EM_SET/GETWRAPMODE 在这枚
//      上问不出也设不动，mode 恒读 0）⇒ 自存窗口属性。而开关的**方向**是反直觉的：
//      lParam 才是目标 DC —— 传 NULL = 折到本窗客户区宽（VB6 的 True），传 GetDC(本窗) =
//      目标宽度变成整屏（不折行）。原生从没调过这条时是"不折"，与 VB6 默认相反 ⇒ 设计期
//      没写也要显式下发一次 True。详见 SetWordWrap 那段。

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include "vb6forms.h"
#include "vb6forms_internal.h"
#include <richedit.h>
#include <oleauto.h>   /* SysAllocString / SysFreeString */
#include <string.h>

#ifdef _WIN32

// cgen 那侧按字面量发，这里也钉死（数值抄自本机那份 SDK 头）
#ifndef ES_READONLY
#define ES_READONLY              0x0040L
#endif
#ifndef EM_SETTARGETDEVICE
#define EM_SETTARGETDEVICE       (WM_USER + 72)
#endif

static const wchar_t kRtbWordWrap[] = L"VB6_RTB_WordWrap";

static DWORD vb6_RtbStyle(void* hwnd) {
    return (DWORD)(DWORD_PTR)GetWindowLongPtrW((HWND)hwnd, GWL_STYLE);
}

// ---------------- 选区：EM_EXGETSEL / EM_EXSETSEL ----------------
// VB6 的 SelStart/SelLength 与原生 CHARRANGE 同为"字符索引"，但原生按 **UTF-16 代码单元**
// 计数（与 EDIT 控件同口径）⇒ 代理对（emoji 这类）两边都算 2。记进手册，别当 VB6 的
// "字符数"来读。cpMax = -1 表示"到文末"，这里折成长度时按文末算。
int32_t vb6_RTB_GetSelStart(void* hwnd) {
    CHARRANGE cr;
    if (!hwnd) return 0;
    ZeroMemory(&cr, sizeof(cr));
    SendMessageW((HWND)hwnd, EM_EXGETSEL, 0, (LPARAM)&cr);
    return (int32_t)cr.cpMin;
}

int32_t vb6_RTB_GetSelLength(void* hwnd) {
    CHARRANGE cr;
    LONG n, end;
    if (!hwnd) return 0;
    ZeroMemory(&cr, sizeof(cr));
    SendMessageW((HWND)hwnd, EM_EXGETSEL, 0, (LPARAM)&cr);
    n = (LONG)SendMessageW((HWND)hwnd, WM_GETTEXTLENGTH, 0, 0);
    /* cpMax < 0 = "到文末"（原生约定）；越界也一律按文末夹，免得读出负长度 */
    end = (cr.cpMax < 0 || cr.cpMax > n) ? n : cr.cpMax;
    if (end < cr.cpMin) end = cr.cpMin;
    return (int32_t)(end - cr.cpMin);
}

static void vb6_RtbSetSel(void* hwnd, int32_t a, int32_t b) {
    CHARRANGE cr;
    LONG n;
    if (!hwnd) return;
    n = (LONG)SendMessageW((HWND)hwnd, WM_GETTEXTLENGTH, 0, 0);
    if (a < 0) a = 0;
    if (b < a) b = a;
    if (b > n) b = (int32_t)n;
    if (a > n) a = (int32_t)n;
    cr.cpMin = a; cr.cpMax = b;
    SendMessageW((HWND)hwnd, EM_EXSETSEL, 0, (LPARAM)&cr);
}

void vb6_RTB_SetSelStart(void* hwnd, int32_t v) {
    vb6_RtbSetSel(hwnd, v, v + vb6_RTB_GetSelLength(hwnd));
}

void vb6_RTB_SetSelLength(void* hwnd, int32_t v) {
    vb6_RtbSetSel(hwnd, vb6_RTB_GetSelStart(hwnd), vb6_RTB_GetSelStart(hwnd) + v);
}

// SelText 读 = EM_GETSELTEXT（含空选区时读到插入点处 0 个字符，与 VB6 一致回空串）
wchar_t* vb6_RTB_GetSelText(void* hwnd) {
    wchar_t buf[512];
    if (!hwnd) return SysAllocString(L"");
    ZeroMemory(buf, sizeof(buf));
    SendMessageW((HWND)hwnd, EM_GETSELTEXT, 0, (LPARAM)buf);
    return SysAllocString(buf);
}

void vb6_RTB_SetSelText(void* hwnd, void* bstr) {
    if (!hwnd) return;
    SendMessageW((HWND)hwnd, EM_REPLACESEL, (WPARAM)TRUE,
                 (LPARAM)(bstr ? (wchar_t*)bstr : (wchar_t)L""));
}

// ---------------- 只读 / 文本上限 ----------------
int32_t vb6_RTB_GetReadOnly(void* hwnd) {
    if (!hwnd) return 0;
    return (vb6_RtbStyle(hwnd) & ES_READONLY) ? -1 : 0;
}
void vb6_RTB_SetReadOnly(void* hwnd, int32_t on) {
    if (!hwnd) return;
    SendMessageW((HWND)hwnd, EM_SETREADONLY, (WPARAM)(on ? TRUE : FALSE), 0);
}

// 量出来的原生默认上限就是 32767（不是"天文数字"），而 VB6 的 MaxLength=0 才是"不限"。
// 所以读数只在**原生没被动过**时折成 0：==32767 -> 0，其余原样回（设过 40000 就读得出 40000）。
#define RTB_NATIVE_DEFAULT_LIMIT 32767u

int32_t vb6_RTB_GetMaxLength(void* hwnd) {
    DWORD n;
    if (!hwnd) return 0;
    n = (DWORD)(DWORD_PTR)SendMessageW((HWND)hwnd, EM_GETLIMITTEXT, 0, 0);
    return (n == RTB_NATIVE_DEFAULT_LIMIT) ? 0 : (int32_t)n;
}
void vb6_RTB_SetMaxLength(void* hwnd, int32_t v) {
    if (!hwnd) return;
    SendMessageW((HWND)hwnd, EM_SETLIMITTEXT,
                 (WPARAM)(v <= 0 ? RTB_NATIVE_DEFAULT_LIMIT : (UINT)v), 0);
}
// 探针量到的一条原生行为（判据 RT29 钉它）：**Text = 赋值会突破上限** —— 限 20 时塞 30 个
// 字符进去，控件把上限抬到了文本长度（读数从 20 变 30），程序化 EM_REPLACESEL 同样穿过去；
// 上限只管**用户键盘输入**那一条路。VB6 的 OCX 会在赋值时截断，本项目的读数跟着原生走。

// ---------------- 滚动条：读样式位、量程另开证人 ----------------
// VB6 的枚举是 0 无 / 1 水平 / 2 垂直 / 3 两者 —— 与 WS_HSCROLL(1 位)/WS_VSCROLL(2 位)
// 的对应**照这个来**。（顺带记下：TextBox 那枚今天把 1/2 映反了，见 029 §九 本格的账。）
int32_t vb6_RTB_GetScrollBars(void* hwnd) {
    DWORD st;
    int r;
    if (!hwnd) return 0;
    st = vb6_RtbStyle(hwnd);
    r = 0;
    if (st & WS_HSCROLL) r |= 1;
    if (st & WS_VSCROLL) r |= 2;
    return r;
}

static int32_t vb6_RtbScrollMax(void* hwnd, int which) {
    SCROLLINFO si;
    if (!hwnd) return 0;
    ZeroMemory(&si, sizeof(si));
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE;
    if (!GetScrollInfo((HWND)hwnd, which, &si)) return 0;
    return (int32_t)si.nMax;
}
// C3 扩展（不是 VB6 属性）：控件自己的滚动量程。它是"滚动条到底活没活"的唯一硬证人。
int32_t vb6_RTB_GetVScrollRange(void* hwnd)  { return vb6_RtbScrollMax(hwnd, SB_VERT); }
int32_t vb6_RTB_GetHScrollRange(void* hwnd)  { return vb6_RtbScrollMax(hwnd, SB_HORZ); }

// ---------------- 自动换行（原生没有 Get ⇒ 自存） ----------------
// 关换行 = EM_SETTARGETDEVICE(hdc=NULL)，恢复 = 传控件自己的 DC 且 repeat=TRUE。
// 设计期的值由 vb6_RTB_Init 下发（创建样式里没有这一位，原生靠的就是这条消息）。
int32_t vb6_RTB_GetWordWrap(void* hwnd) {
    HANDLE h;
    if (!hwnd) return -1;
    h = GetPropW((HWND)hwnd, kRtbWordWrap);
    if (!h) return -1;                    /* 没动过 = 原生默认（换行开着） */
    return *(int32_t*)h;
}

void vb6_RTB_SetWordWrap(void* hwnd, int32_t on) {
    HANDLE h;
    if (!hwnd) return;
    /* 方向是量出来的，别照抄网上那段经典片段：EM_SETTARGETDEVICE 的 wParam 恒 0、
       lParam 才是目标 DC。lParam=NULL ⇒ 折到**本窗客户区**宽（= VB6 的 WordWrap=True）；
       lParam=GetDC(本窗) ⇒ 目标宽度变成整屏（横向量程读出 559240 那种天文数字）= 实际不折行。
       从没调过这条时原生是"不折"（长行横向量程 3200 而纵向只有 1 行），所以设计期没写也要
       显式下发一次 True，否则观感与 VB6 相反。EM_SET/GETWRAPMODE 在这枚上问不出也设不动
       （mode 恒读 0），别指望它。 */
    if (on) {
        SendMessageW((HWND)hwnd, EM_SETTARGETDEVICE, 0, (LPARAM)NULL);
    } else {
        HDC dc = GetDC((HWND)hwnd);
        SendMessageW((HWND)hwnd, EM_SETTARGETDEVICE, 0, (LPARAM)dc);
        ReleaseDC((HWND)hwnd, dc);
    }
    h = GetPropW((HWND)hwnd, kRtbWordWrap);
    if (!h) {
        h = HeapAlloc(GetProcessHeap(), 0, sizeof(int32_t));
        if (h) SetPropW((HWND)hwnd, kRtbWordWrap, (HANDLE)h);
    }
    if (h) *(int32_t*)h = on ? -1 : 0;
}

// ---------------- 设计期初值 ----------------
// ScrollBars 在 cgen 那侧立进创建参数（事后写只有外观、没有量程），所以 Init 里只剩
// WordWrap 与 ReadOnly 两条。-999 = .frm 没写 ⇒ 下发 VB6 的那个默认（换行开、可编辑），
// 而不是"什么都不做" —— 原生自己的默认是**不折行**，与 VB6 相反（见上面那段）。
// 哨兵为什么不是 -1：VB6 的布尔在 .frm 里就序列化成 -1 = True，用 -1 当"未写"会把
// "写着 True 的那一条"读成没写（本线 5a/8a 同一口径）。
void vb6_RTB_Init(void* hwnd, int32_t wordWrap, int32_t readOnly) {
    if (!hwnd) return;
    vb6_RTB_SetWordWrap(hwnd, wordWrap == -999 ? -1 : wordWrap);
    if (readOnly != -999) vb6_RTB_SetReadOnly(hwnd, readOnly);
}

#endif /* _WIN32 */
