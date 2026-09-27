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

// ---------------- C29-RT-b：Sel* 的格式面 ----------------
// 三态怎么问出来的（探针 `.build/rtprobe4.c`，本机读数）：EM_GETCHARFORMAT / EM_GETPARAFORMAT
// 把返回的 dwMask 里**选区内不一致**那些位**清掉** —— 只问一位时最干净：
//   全加粗 (20..26 只涂斜体那格反过来看) mask=0xFFFFFFFF / 跨边界 mask=0xFFFFFFFD（那一位没了）
//   全文（多种属性都参差）mask=0x1FFFFFF1
// 字符面这条**返回值就等于那张掩码**（rc=-3 即 0xFFFFFFFD），段落面不等（要读结构体里的 dwMask）。
// VB6 混合回 Null，本项目按布尔给 ⇒ 混合 = False（VB6 文档自己也教 `If .SelBold = True`，
// 而 `Null = True` 本来就是假，所以两种口径在常见写法下等价）。
#ifndef CFM_BOLD
#define CFM_BOLD        0x00000001
#define CFM_ITALIC      0x00000002
#define CFM_UNDERLINE   0x00000004
#define CFM_STRIKEOUT   0x00000008
#define CFM_COLOR       0x40000000
#define CFM_FACE        0x20000000
#define CFM_SIZE        0x80000000
#define CFE_BOLD        0x00000001
#define CFE_ITALIC      0x00000002
#define CFE_UNDERLINE   0x00000004
#define CFE_STRIKEOUT   0x00000008
#define CFE_AUTOCOLOR   0x40000000
#endif
#ifndef PFA_LEFT
#define PFA_LEFT        1
#define PFA_RIGHT       2
#define PFA_CENTER      3
#endif
#ifndef PFM_ALIGNMENT
#define PFM_ALIGNMENT       0x00000008
#define PFM_OFFSET          0x00000004
#define PFM_RIGHTINDENT     0x00000002
#define PFM_STARTINDENT     0x00000001
#endif

// 读一位字符效果：掩码里那一位在 = 一致（按 effects 给 True/False），不在 = 混合（给 False）。
static int32_t vb6_RtbGetEffect(void* hwnd, DWORD bit) {
    CHARFORMAT2W cf;
    if (!hwnd) return 0;
    ZeroMemory(&cf, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = bit;
    SendMessageW((HWND)hwnd, EM_GETCHARFORMAT, (WPARAM)SCF_SELECTION, (LPARAM)&cf);
    if (!(cf.dwMask & bit)) return 0;                 /* 混合 ⇒ 本项目按 False 给 */
    return (cf.dwEffects & bit) ? -1 : 0;
}

static void vb6_RtbSetEffect(void* hwnd, DWORD bit, int32_t on) {
    CHARFORMAT2W cf;
    if (!hwnd) return;
    ZeroMemory(&cf, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = bit;
    cf.dwEffects = on ? bit : 0;
    SendMessageW((HWND)hwnd, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
}

int32_t vb6_RTB_GetSelBold(void* hwnd)      { return vb6_RtbGetEffect(hwnd, CFM_BOLD); }
void    vb6_RTB_SetSelBold(void* hwnd, int32_t on)      { vb6_RtbSetEffect(hwnd, CFM_BOLD, on); }
int32_t vb6_RTB_GetSelItalic(void* hwnd)    { return vb6_RtbGetEffect(hwnd, CFM_ITALIC); }
void    vb6_RTB_SetSelItalic(void* hwnd, int32_t on)    { vb6_RtbSetEffect(hwnd, CFM_ITALIC, on); }
int32_t vb6_RTB_GetSelUnderline(void* hwnd) { return vb6_RtbGetEffect(hwnd, CFM_UNDERLINE); }
void    vb6_RTB_SetSelUnderline(void* hwnd, int32_t on) { vb6_RtbSetEffect(hwnd, CFM_UNDERLINE, on); }
int32_t vb6_RTB_GetSelStrikethru(void* hwnd){ return vb6_RtbGetEffect(hwnd, CFM_STRIKEOUT); }
void    vb6_RTB_SetSelStrikethru(void* hwnd, int32_t on){ vb6_RtbSetEffect(hwnd, CFM_STRIKEOUT, on); }

// SelColor：显式色的判据是**自动色那一位被清掉**（CFE_AUTOCOLOR 与 CFM_COLOR 同位，量过的）。
// 没涂过色 = 自动色 ⇒ 读回控件自己的前景色（与 VB6 一致：那时 SelColor 就是 ForeColor）。
// 混合（掩码里没有 CFM_COLOR）⇒ 回 0，与布尔那几条同一口径。
extern int vb6_GetControlForeColor(void* hwnd);

int32_t vb6_RTB_GetSelColor(void* hwnd) {
    CHARFORMAT2W cf;
    if (!hwnd) return 0;
    ZeroMemory(&cf, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    SendMessageW((HWND)hwnd, EM_GETCHARFORMAT, (WPARAM)SCF_SELECTION, (LPARAM)&cf);
    if (!(cf.dwMask & CFM_COLOR)) return 0;                       /* 混合 */
    if (cf.dwEffects & CFE_AUTOCOLOR)                             /* 自动色 */
        return (int32_t)vb6_GetControlForeColor(hwnd);
    return (int32_t)(cf.crTextColor & 0x00FFFFFF);
}

void vb6_RTB_SetSelColor(void* hwnd, int32_t colorRef) {
    CHARFORMAT2W cf;
    if (!hwnd) return;
    ZeroMemory(&cf, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.dwEffects = 0;                     /* 清掉 CFE_AUTOCOLOR = 改成显式色 */
    cf.crTextColor = (COLORREF)(colorRef & 0x00FFFFFF);
    SendMessageW((HWND)hwnd, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
}

// SelFontName / SelFontSize：原生字号单位是 **1/20 磅**（yHeight），混合时回 ""/0。
wchar_t* vb6_RTB_GetSelFontName(void* hwnd) {
    CHARFORMAT2W cf;
    if (!hwnd) return SysAllocString(L"");
    ZeroMemory(&cf, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_FACE;
    SendMessageW((HWND)hwnd, EM_GETCHARFORMAT, (WPARAM)SCF_SELECTION, (LPARAM)&cf);
    if (!(cf.dwMask & CFM_FACE)) return SysAllocString(L"");      /* 混合 */
    cf.szFaceName[31] = 0;
    return SysAllocString(cf.szFaceName);
}

void vb6_RTB_SetSelFontName(void* hwnd, void* bstr) {
    CHARFORMAT2W cf;
    if (!hwnd) return;
    ZeroMemory(&cf, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_FACE;
    if (bstr) lstrcpynW(cf.szFaceName, (const wchar_t*)bstr, 32);
    SendMessageW((HWND)hwnd, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
}

float vb6_RTB_GetSelFontSize(void* hwnd) {
    CHARFORMAT2W cf;
    if (!hwnd) return 0.0f;
    ZeroMemory(&cf, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_SIZE;
    SendMessageW((HWND)hwnd, EM_GETCHARFORMAT, (WPARAM)SCF_SELECTION, (LPARAM)&cf);
    if (!(cf.dwMask & CFM_SIZE)) return 0.0f;                     /* 混合 */
    return (float)cf.yHeight / 20.0f;
}

void vb6_RTB_SetSelFontSize(void* hwnd, float points) {
    CHARFORMAT2W cf;
    if (!hwnd) return;
    ZeroMemory(&cf, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_SIZE;
    cf.yHeight = (LONG)(points * 20.0f + 0.5f);
    SendMessageW((HWND)hwnd, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
}

// 段落三项：dwMask 里读回的那一位才是"一致"的凭据（这条与字符面同型，只是**不能拿 rc 当掩码**）。
static PARAFORMAT2 vb6_RtbGetPara(void* hwnd, DWORD ask) {
    PARAFORMAT2 pf;
    ZeroMemory(&pf, sizeof(pf));
    pf.cbSize = sizeof(pf);
    pf.dwMask = ask;
    if (hwnd) SendMessageW((HWND)hwnd, EM_GETPARAFORMAT, (WPARAM)SCF_SELECTION, (LPARAM)&pf);
    return pf;
}

static void vb6_RtbSetPara(void* hwnd, DWORD ask, const PARAFORMAT2* src) {
    PARAFORMAT2 pf;
    if (!hwnd) return;
    pf = *src;
    pf.cbSize = sizeof(pf);
    pf.dwMask = ask;
    SendMessageW((HWND)hwnd, EM_SETPARAFORMAT, SCF_SELECTION, (LPARAM)&pf);
}

// SelAlignment：VB6 是 0 左 / 1 中 / 2 右，原生是 PFA_LEFT=1 / PFA_CENTER=3 / PFA_RIGHT=2
// —— **两套数**，必须折算（不折算的话"居中"会被读成"右对齐"）。混合 ⇒ 0（左）。
int32_t vb6_RTB_GetSelAlignment(void* hwnd) {
    PARAFORMAT2 pf = vb6_RtbGetPara(hwnd, PFM_ALIGNMENT);
    if (!(pf.dwMask & PFM_ALIGNMENT)) return 0;
    switch (pf.wAlignment) {
        case PFA_CENTER: return 1;
        case PFA_RIGHT:  return 2;
        default:         return 0;
    }
}

void vb6_RTB_SetSelAlignment(void* hwnd, int32_t align) {
    PARAFORMAT2 pf;
    ZeroMemory(&pf, sizeof(pf));
    pf.cbSize = sizeof(pf);
    pf.wAlignment = (align == 1) ? PFA_CENTER : (align == 2) ? PFA_RIGHT : PFA_LEFT;
    vb6_RtbSetPara(hwnd, PFM_ALIGNMENT, &pf);
}

// 三个缩进：原生单位是 twips（1/20 磅），与 VB6 那三条同名属性的单位一致，直接对传。
// 悬挂缩进在原生里是 **dxOffset 取负**（首行往外凸）；探针量到别用 PFM_OFFSETINDENT ——
// 那条是把整段往右推（写 240 之后 dxStartIndent 从 720 变 960），不是悬挂。
int32_t vb6_RTB_GetSelIndent(void* hwnd) {
    PARAFORMAT2 pf = vb6_RtbGetPara(hwnd, PFM_STARTINDENT);
    return (pf.dwMask & PFM_STARTINDENT) ? (int32_t)pf.dxStartIndent : 0;
}
void vb6_RTB_SetSelIndent(void* hwnd, int32_t twips) {
    PARAFORMAT2 pf;
    ZeroMemory(&pf, sizeof(pf));
    pf.cbSize = sizeof(pf);
    pf.dxStartIndent = (LONG)twips;
    vb6_RtbSetPara(hwnd, PFM_STARTINDENT, &pf);
}
int32_t vb6_RTB_GetSelRightIndent(void* hwnd) {
    PARAFORMAT2 pf = vb6_RtbGetPara(hwnd, PFM_RIGHTINDENT);
    return (pf.dwMask & PFM_RIGHTINDENT) ? (int32_t)pf.dxRightIndent : 0;
}
void vb6_RTB_SetSelRightIndent(void* hwnd, int32_t twips) {
    PARAFORMAT2 pf;
    ZeroMemory(&pf, sizeof(pf));
    pf.cbSize = sizeof(pf);
    pf.dxRightIndent = (LONG)twips;
    vb6_RtbSetPara(hwnd, PFM_RIGHTINDENT, &pf);
}
int32_t vb6_RTB_GetSelHangingIndent(void* hwnd) {
    PARAFORMAT2 pf = vb6_RtbGetPara(hwnd, PFM_OFFSET);
    if (!(pf.dwMask & PFM_OFFSET)) return 0;
    return (pf.dxOffset < 0) ? (int32_t)(-pf.dxOffset) : 0;
}
void vb6_RTB_SetSelHangingIndent(void* hwnd, int32_t twips) {
    PARAFORMAT2 pf;
    ZeroMemory(&pf, sizeof(pf));
    pf.cbSize = sizeof(pf);
    pf.dxOffset = (LONG)(twips > 0 ? -twips : 0);
    vb6_RtbSetPara(hwnd, PFM_OFFSET, &pf);
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
