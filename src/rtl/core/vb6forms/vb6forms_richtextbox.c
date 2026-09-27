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
// 事件面（C29-RT-d）：ENM_SELCHANGE **必须**在这里显式开 —— 三件事都是探针量出来的
// （.build\rtdprobe5.c，六种"建窗时机 × 掩码写法"组合各跑一份，读数 .build\rtdprobe5_c*_s*.txt）：
//   ① 建好不动掩码 ⇒ 选区变化一条通知都不发（s0 三格全 0）；Msftedit 的默认事件掩码是**空**的。
//   ② 整枚 32 位掩码塞 wParam、lParam 传 0 ⇒ 也发不出来（s1 与 s0 逐字相同）。原生这条只认
//      wParam 的**低 16 位**，高 16 位要靠 lParam 那个指针进出 —— 而 ENM_SELCHANGE = 0x00080000
//      恰好整枚都在高位上，写错这一句就是"永远收不到 SelChange"。
//   ③ 与之对照，文本变化的 EN_UPDATE(0x0400=1024) **不受掩码管**：s0/s1 里 WM_SETTEXT 照样发它。
//      （也别指望老的那条 EN_CHANGE=768：六份读数里它一次都没出现过。）
// 这条写在 Init 里而不是各 setter 里：掩码是控件的常驻状态，且这一刻还在 formLoading 挡派发
// （见 cgen 那条 `if (vb6_formLoading_…) break;`），它顺手 provoke 的那一条 1024 会被丢掉。
void vb6_RTB_Init(void* hwnd, int32_t wordWrap, int32_t readOnly) {
    if (!hwnd) return;
    vb6_RTB_SetWordWrap(hwnd, wordWrap == -999 ? -1 : wordWrap);
    if (readOnly != -999) vb6_RTB_SetReadOnly(hwnd, readOnly);
    {
        DWORD hi = (DWORD)(ENM_SELCHANGE >> 16);   /* 0x00080000 >> 16 = 8，进的是掩码高位 */
        SendMessageW((HWND)hwnd, EM_SETEVENTMASK,
                     (WPARAM)(DWORD)(ENM_CHANGE | ENM_UPDATE), (LPARAM)&hi);
    }
}

// 判据专用（C29-RT-d，与 vb6_MV_SimDateClick / vb6_DTP_Sim* 同先例）：替原生控件把一条
// **真**通知发到父窗，让"认来源 → 按 code 分流 → 调处理器"三段派发被真的消息喂一遍。
// 为什么这条事件需要它、而 Change 不需要：EN_SELCHANGE 在**同一份产物、同一个夹具**上都不
// 稳定 —— 三次连跑里 1 次走 WM_NOTIFY/1794、2 次走 WM_COMMAND/1815（读数 .build\rtdmin_out\tr_*.txt，
// 裸码由临时探针打出来），程序化 EM_EXSETSEL 有时一条都不发。也就是说 SelChange 的
// "原生自发"这一路今天给不出可依赖的判据，只能自己造通知；Change 那一路（EN_UPDATE，见夹具
// RT-d 的说明）是稳的，判据走真属性写。
// code = 1794 → WM_NOTIFY（负载 SELCHANGE：选区现值从 EM_EXGETSEL 拿）；
// 其余（1815 那一族）→ WM_COMMAND，wParam 高字 = code、lParam = 控件自己，与裸码一致。
void vb6_RTB_SimNotify(void* hwnd, int32_t code) {
    HWND h = (HWND)hwnd;
    HWND p;
    INT_PTR id;
    if (!h) return;
    p = GetParent(h);
    if (!p) return;
    id = (INT_PTR)GetWindowLongPtrW(h, GWLP_ID);
    if (code == EN_SELCHANGE) {
        SELCHANGE sc;
        ZeroMemory(&sc, sizeof(sc));
        sc.nmhdr.hwndFrom = h;
        sc.nmhdr.idFrom = (UINT_PTR)id;
        sc.nmhdr.code = (UINT)code;
        SendMessageW(h, EM_EXGETSEL, 0, (LPARAM)&sc.chrg);
        SendMessageW(p, WM_NOTIFY, (WPARAM)id, (LPARAM)&sc);
    } else {
        SendMessageW(p, WM_COMMAND, MAKEWPARAM((WORD)id, (WORD)code), (LPARAM)h);
    }
}


// ---------------- TextRTF / LoadFile / SaveFile / Find（C29-RT-c）----------------
// 原生这一族只有一个入口：EM_STREAMOUT / EM_STREAMIN + EDITSTREAM 那枚回调。三条量出来的
// 契约（探针 .build\rtprobe6.c，读数 .build\rtprobe6b_out.txt = 挂 v6 清单的产物形状，
// x64 与 x86 逐字相同）：
//   (1) **回调的返回值非零 = 中止**，而且那个值原样落进 EDITSTREAM.dwError —— 第一版照直觉
//       "返回搬掉的字节数"，四组读数一律 rc=0 / dwError=230 / 控件里一个字都没有（症状与
//       "根本没调"一模一样，光看 rc 看不出来）。改成"字节数只走 *pcb、返回值一律 0"之后：
//       SF_RTF 灌回 rc=32 / dwError=0，文本与 RTF 都回到母本那 230 字节。
//   (2) 流出来的 RTF **头里带本机 ANSI 码页与中文键盘语言**（实测 "{\rtf1\ansi\ansicpg936
//       \deff0\nouicompat\deflang1033\deflangfe2052..."）⇒ TextRTF 不许按字节比，判据一律问
//       "前缀 + round-trip + 两次自比"。非 ASCII 由控件自己逃生成 \uN，串本身仍是 7 位为主
//       —— 但码页那一格随机器变。
//   (3) SF_RTF 的 rc **不是字节数**（喂 230 字节回来 rc=32 = 落进去的字符数）⇒ 别拿它当量。
//
// 三条 FR_ 常数在 SDK 的 richedit.h 里**没有**（那里只有 FR_MATCHDIAC 一族），值抄 MSDN 的
// EM_FINDTEXT 文档，并且**按行为验过**（.build\rtprobe5_out.txt）：同一枚控件找 "ol"，
// FR_WHOLEWORD(2) 回 -1 而不带它回 7；找 "alpha" 带 FR_MATCHCASE(4) 跳过开头那枚大写
// Alpha、命中 20 ⇒ 两条位含义与文档一致。VB6 的两个常数与原生**不同位**（rtfWholeWord=1
// → FR_WHOLEWORD=2、rtfMatchCase=2 → FR_MATCHCASE=4），所以 flags 要逐位映射；不映射的症状
// 是"区分大小写被当成整词"。
#ifndef FR_DOWN
#define FR_DOWN       0x00000001L
#define FR_WHOLEWORD  0x00000002L
#define FR_MATCHCASE  0x00000004L
#endif
#ifndef EM_FINDTEXTEXW
#define EM_FINDTEXTEXW (WM_USER + 124)
#endif

typedef struct { char* p; size_t cap; size_t len; } vb6_RtbBuf;          /* 流出的收束缓冲 */
typedef struct { const char* p; size_t len; size_t pos; } vb6_RtbSrc;    /* 流入的字节源 */

static DWORD CALLBACK vb6_RtbSinkCb(DWORD_PTR cookie, LPBYTE buff, long cb, long* pcb) {
    vb6_RtbBuf* b = (vb6_RtbBuf*)cookie;
    size_t need;
    char* np;
    *pcb = 0;
    if (cb <= 0) return 0;
    need = b->len + (size_t)cb + 1;
    if (need > b->cap) {
        size_t want = b->cap ? b->cap : 1024;
        while (want < need) want *= 2;
        np = (char*)realloc(b->p, want);
        if (!np) return 1;                  /* 中止：缓冲都分配不出来，别装成功 */
        b->p = np; b->cap = want;
    }
    memcpy(b->p + b->len, buff, (size_t)cb);
    b->len += (size_t)cb;
    b->p[b->len] = 0;
    *pcb = cb;
    return 0;
}

static DWORD CALLBACK vb6_RtbSrcCb(DWORD_PTR cookie, LPBYTE buff, long cb, long* pcb) {
    vb6_RtbSrc* s = (vb6_RtbSrc*)cookie;
    size_t n;
    *pcb = 0;
    if (cb <= 0) return 0;
    n = s->len - s->pos;
    if (n > (size_t)cb) n = (size_t)cb;
    if (n) { memcpy(buff, s->p + s->pos, n); s->pos += n; }
    /* 搬了几格**必须写进 *pcb**：只填缓冲区而不报数，控件按"读到 0 字节 = 文件结束"处理
       ⇒ 选区被清掉、内容一个字不进来，症状与"没调"一样（本文件契约(1)说的就是这一族，
       写注释的人自己还是踩了一次）。 */
    *pcb = (long)n;
    return 0;                               /* 一律 0：非零 = 中止，见上面契约(1) */
}

// 整串流出去（fmt = SF_RTF / SF_TEXT）。拿到的是堆上一份宽字符副本，由调用方 SysFreeString。
static wchar_t* vb6_RtbStreamOut(HWND h, UINT fmt) {
    vb6_RtbBuf b;
    EDITSTREAM es;
    wchar_t* w = NULL;
    int wn;
    ZeroMemory(&b, sizeof(b));
    ZeroMemory(&es, sizeof(es));
    es.dwCookie = (DWORD_PTR)&b;
    es.pfnCallback = vb6_RtbSinkCb;
    SendMessageW(h, EM_STREAMOUT, fmt, (LPARAM)&es);
    if (!b.p) return SysAllocString(L"");
    /* 这条流是 **ANSI 字节流**（头里就写着 ansicpg），所以按 CP_ACP 折成宽字符。
       这里不能套 vb6_u8ToWideDup —— 那一条是 UTF-8 优先，中文那一段会折散。 */
    wn = MultiByteToWideChar(CP_ACP, 0, b.p, -1, NULL, 0);
    if (wn > 1) {
        w = (wchar_t*)SysAllocStringLen(NULL, wn - 1);
        if (w) MultiByteToWideChar(CP_ACP, 0, b.p, -1, w, wn);
    }
    free(b.p);
    return w ? w : SysAllocString(L"");
}

static void vb6_RtbStreamIn(HWND h, UINT fmt, const char* bytes, size_t len) {
    vb6_RtbSrc s;
    EDITSTREAM es;
    s.p = bytes; s.len = len; s.pos = 0;
    ZeroMemory(&es, sizeof(es));
    es.dwCookie = (DWORD_PTR)&s;
    es.pfnCallback = vb6_RtbSrcCb;
    es.dwError = 0;
    /* 先全选：VB6 的 LoadFile / 给 TextRTF 赋值都是"把内容换掉"，而原生 EM_STREAMIN 是
       "灌进当前选区" ⇒ 选区留在文末就变成追加。 */
    SendMessageW(h, EM_SETSEL, 0, -1);
    SendMessageW(h, EM_STREAMIN, fmt, (LPARAM)&es);
}

wchar_t* vb6_RTB_GetTextRTF(void* hwnd) {
    if (!hwnd) return SysAllocString(L"");
    return vb6_RtbStreamOut((HWND)hwnd, SF_RTF);
}

void vb6_RTB_SetTextRTF(void* hwnd, void* bstr) {
    const wchar_t* w = (const wchar_t*)bstr;
    char* a;
    int an;
    if (!hwnd || !w) return;
    an = WideCharToMultiByte(CP_ACP, 0, w, -1, NULL, 0, NULL, NULL);
    if (an <= 1) return;
    a = (char*)malloc((size_t)an);
    if (!a) return;
    WideCharToMultiByte(CP_ACP, 0, w, -1, a, an, NULL, NULL);
    /* 流尾**要带上那个 NUL**：探针把同一份 RTF 的尾 NUL 剪掉就五组全成空控件，
       带着就五组全成（.build\rtprobe7.ps1 的 A..E，母本 220 字节含 NUL、灌完回读 219）
       ⇒ SF_RTF 的解析器要看见结尾的 0 才收尾。 */
    vb6_RtbStreamIn((HWND)hwnd, SF_RTF, a, (size_t)an);
    free(a);
}

// LoadFile / SaveFile 的 filetype：VB6 = 0 rtfRTF / 1 rtfText（官方那页的示例与
// ai\内置控件 那张表同一口径）。返回 0 = 成、非 0 = Win32 错误码 —— VB6 那两条是 Sub，
// 失败靠运行期错误，本项目还没有那条通道，所以把码留着给判据（以及以后的 Err 面）用。
static int32_t vb6_RtbReadWhole(const wchar_t* path, char** out, size_t* outLen) {
    HANDLE f;
    DWORD size, got = 0;
    char* buf;
    *out = NULL; *outLen = 0;
    if (!path || !path[0]) return -1;
    f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return (int32_t)GetLastError();
    size = GetFileSize(f, NULL);
    if (size == INVALID_FILE_SIZE) { DWORD e = (DWORD)GetLastError(); CloseHandle(f); return (int32_t)e; }
    buf = (char*)malloc((size_t)size + 1);
    if (!buf) { CloseHandle(f); return -1; }
    if (size && !ReadFile(f, buf, size, &got, NULL)) {
        DWORD e = (DWORD)GetLastError(); free(buf); CloseHandle(f); return (int32_t)e;
    }
    CloseHandle(f);
    buf[got] = 0;
    *out = buf; *outLen = (size_t)got;
    return 0;
}

static int32_t vb6_RtbWriteWhole(const wchar_t* path, const char* bytes, size_t len) {
    HANDLE f;
    DWORD put = 0;
    if (!path || !path[0]) return -1;
    f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return (int32_t)GetLastError();
    if (len && !WriteFile(f, bytes, (DWORD)len, &put, NULL)) {
        DWORD e = (DWORD)GetLastError(); CloseHandle(f); return (int32_t)e;
    }
    CloseHandle(f);
    return (put == (DWORD)len) ? 0 : -1;
}

int32_t vb6_RTB_LoadFile(void* hwnd, const wchar_t* path, int32_t fileType) {
    char* bytes = NULL;
    size_t len = 0;
    int32_t rc;
    if (!hwnd) return -1;
    rc = vb6_RtbReadWhole(path, &bytes, &len);
    if (rc) return rc;
    vb6_RtbStreamIn((HWND)hwnd, fileType == 1 ? SF_TEXT : SF_RTF, bytes, len);
    free(bytes);
    return 0;
}

int32_t vb6_RTB_SaveFile(void* hwnd, const wchar_t* path, int32_t fileType) {
    vb6_RtbBuf b;
    EDITSTREAM es;
    int32_t rc;
    if (!hwnd) return -1;
    ZeroMemory(&b, sizeof(b));
    ZeroMemory(&es, sizeof(es));
    es.dwCookie = (DWORD_PTR)&b;
    es.pfnCallback = vb6_RtbSinkCb;
    SendMessageW((HWND)hwnd, EM_STREAMOUT, fileType == 1 ? SF_TEXT : SF_RTF, (LPARAM)&es);
    rc = vb6_RtbWriteWhole(path, b.p ? b.p : "", b.len);
    free(b.p);
    return rc;
}

// Find(text, start, end, flags)：命中回**起点**（原生 cpMin，与本控件 SelStart 同一把尺），
// 问不出回 -1。范围就是原生那张 CHARRANGE（end 传 -1 = 到文末）；VB6 的 start/end 两个
// 缺省值由 cgen 那侧填（-1 = 全文）。
int32_t vb6_RTB_Find(void* hwnd, const wchar_t* text, int32_t start, int32_t end, int32_t flags) {
    FINDTEXTEXW ft;
    int32_t rc;
    if (!hwnd || !text || !text[0]) return -1;
    /* 刻意**不动选区**（既不存也不复）：原生这条查询自己干了什么，由夹具量完再决定要不要
       齐平 VB6 —— 先写"恢复原选区"就是拿没量过的行为当已知（EM_FINDTEXT 与 EM_FINDTEXTEX
       在动不动选区这件事上并不一样）。命中回起点，未命中一律回 -1。 */
    ZeroMemory(&ft, sizeof(ft));
    ft.chrg.cpMin = (LONG)start;
    ft.chrg.cpMax = (LONG)end;
    ft.lpstrText = (LPCWSTR)text;
    rc = (int32_t)SendMessageW((HWND)hwnd, EM_FINDTEXTEXW,
                               (WPARAM)(FR_DOWN
                                        | ((flags & 1) ? (int32_t)FR_WHOLEWORD : 0)
                                        | ((flags & 2) ? (int32_t)FR_MATCHCASE  : 0)),
                               (LPARAM)&ft);
    return rc < 0 ? -1 : rc;
}


#endif /* _WIN32 */
