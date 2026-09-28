// vb6forms_slider.c — VB6 Slider 控件 (ai/内置控件/Slider 控件（滑杆）.md, ai/029 C29-SL)
//
// 复刻口径: 用 comctl32 的 msctls_trackbar32 复刻 VB6 Slider 的等价行为,
// **不加载任何 OCX** (mscomctl.ocx 在本机未注册, 且 32 位 inproc 无法进 x64 进程)。
//
// 本文件是 **SL-a**: 窗口 + 创建样式 + 标量属性面。值面 (Min/Max/Value/Small·LargeChange/
// Sel*) 归 SL-b, 事件归 SL-c。
//
// 实测口径 (.build/slprobe/slmeasure*.c, x64 真跑; 全文抄在 ai/029 §九 C29-SL-0 那一格):
//   1) **方向的正解**: TBM_GETCHANNELRECT 的 rect 永远把行程长度放在 **x 分量** ——
//      300x40 的水平杆与 40x300 的竖杆都答 (8,10)-(292,14)，它压根不随方向旋转，
//      所以"量 channel 的长短边"等于什么都没量（本格第一发就这么错过一次，见下第 4 条）。
//      能用的证人是 **TBM_GETTHUMBRECT 在 min/max 两点之间的位移轴** (TravelIsVert)。
//   2) Orientation **运行期改是有效的**: 创建水平之后写 TBS_VERT + SetWindowPos(SWP_FRAMECHANGED)，
//      滑块矩形与"创建时就是垂直"逐字相同（(2,8)-(24,19)）；对照组同样 SetWindowPos 但不翻样式，
//      读数不变 ⇒ 翻样式这一步才是因。**第一发探针量出"只能创建时定"是错的**，错因就是第 1 条
//      那条假证人 —— 样式位读回来一直是"写了"，而布局到底听不听，得推滑块才知道。
//   3) TickFrequency 原生**问不出**: 没有 TBM_GETTICFREQ；TBM_GETTIC(pos) 在 freq=10 下对
//      pos=15 也返回非 0（返回值 = pos+1，答的是"在不在量程内"）；TBM_GETTICPOS(i) 在 freq=10
//      与 freq=25 下读数逐字相同（16,18,21,24,27,29），只有"有没有刻度"这一维能证
//      （TBM_CLEARTICS 之后全变 -1）。⇒ 频率只能自存读回 + 真下发一次 TBM_SETTICFREQ；
//      "有刻度"这条用 TickPresent 当控件侧证人。
//   4) Value 越界 = 原生钳位（range 10..100 时 SETPOS(150) 读回 100、(-5) 读回 10）—— SL-b 用。
//      TBM_GETCHANNELRECT / GETTHUMBRECT 的**返回值不是成功标志**（实测返回 0 而 rect 有效），
//      一律只看 rect 内容。
//
// VB6 枚举（本文件用到的这一档）：
//   Orientation: 0 = sldHorizontal（默认），1 = sldVertical
//   TickStyle 四档与 TBS_ 的对应 **本机拿不到 VB6 真值**（OCX 未注册、typelib 读不到），
//   刻意不实现，已按 #120 那条口径押后等裁决 —— 别照猜的枚举写代码。

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#endif

#include "vb6forms.h"
#include "vb6forms_internal.h"

#ifdef _WIN32

#define VB6_SLD_HORZ 0
#define VB6_SLD_VERT 1

// 刻意**不写** `#ifndef TBS_xxx / #define ...` 那一族兜底宏：上一版手写的
// `#define TBM_GETTHUMBRECT (WM_USER + 17)` 与 SDK 头对不上（头里 GETTHUMBRECT 是 +25，
// +17 是 TBM_GETSELSTART）—— 只因 commctrl.h 先定义了它，#ifndef 才没盖上去。
// 本文件全程 #include <commctrl.h>，消息名一律用头里的，不再抄第二份编号表。
#ifndef VB6_SLD_UNSET
#define VB6_SLD_UNSET (-999)     // 设计期"没写过"那一档的哨兵（同 vb6_TreeView_Init）
#endif
#ifndef VB6_SLD_I16MIN
#define VB6_SLD_I16MIN (-32768)
#endif
#ifndef VB6_SLD_I16MAX
#define VB6_SLD_I16MAX 32767
#endif

// --- 自存槽（+1 偏移，同 vb6forms_progress.c 那条：SetPropW(hw,name,NULL) 看着像"存了个空值"，
//     而 GetPropW 对"未设置"与"存了 0"都返回 NULL —— 不偏移就会把 `TickFrequency = 0`
//     这种合法值当成未设置，静默答回默认）---
static const wchar_t kSdTickFreq[] = L"VB6_SD_TickFrequency";

static void vb6_SdSetProp(void* hwnd, const wchar_t* name, LONG val) {
    if (!hwnd) return;
    SetPropW((HWND)hwnd, name, (HANDLE)(INT_PTR)((LONG)val + 1));
}

static LONG vb6_SdGetProp(void* hwnd, const wchar_t* name, LONG def) {
    if (!hwnd) return def;
    HANDLE h = GetPropW((HWND)hwnd, name);
    return h ? ((LONG)(INT_PTR)h - 1) : def;
}

// 换一次帧：实测样式位（Orientation / EnableSelRange）写完要配这一脚，控件才真按新位重排
// （SetWindowPos 传原尺寸，只挂 NOZORDER|NOMOVE|FRAMECHANGED）。
static void vb6_SdReframe(HWND hw) {
    RECT wr;
    GetWindowRect(hw, &wr);
    SetWindowPos(hw, NULL, 0, 0, (int)(wr.right - wr.left), (int)(wr.bottom - wr.top),
                 SWP_NOZORDER | SWP_NOMOVE | SWP_FRAMECHANGED);
}

// 设计期下发（SL-a 只有 tickFrequency 一参，SL-b 把它扩成完整值面）。
// 哨兵 -999 = .frm 里**没写过**这一项 ⇒ 那一条消息都不发，保持控件默认
// （同 vb6_TreeView_Init：拿 -1 当"未写"会让设计期永远设不上合法的 0/-1 那一档）。
// 顺序是量出来的，不能换：
//   1) range 先立 —— 实测 `TBM_SETRANGE(10,100)` 会把 `pos` 顶到下限 10、把 `page` 从 20
//      重算成 18、把 `selstart` 跟到 10；先设值再设 range 就会被这三处连带改动吃掉；
//   2) line/page 再设 —— 它们同样会被 range 重算，必须在 range 之后；
//   3) pos 第三（量程已定，越界由控件钳位 —— 实测 SETPOS(150)→100、(-5)→10）；
//   4) 刻度与 Sel 最后。
// Orientation 不走这里 —— 它是创建参数，cgen 立进 vb6_CreateControl 的样式位。
void vb6_Slider_Init(void* hwnd, long min, long max, long value,
                     long smallChange, long largeChange, long tickFrequency,
                     long selStart, long selEnd, long selectRange) {
    HWND hw;
    long lo, hi, curLo, curHi;
    if (!hwnd) return;
    hw = (HWND)hwnd;
    // 只有 .frm **真写过** Min/Max 才发 range：控件当前值要先读出来当另一端。
    // （踩过的坑：早先写成"没写就从当前值取"再比 `lo != min` 判要不要发 —— 那比较拿
    //  当前值与哨兵 -999 比，永远不等 ⇒ 每枚 Slider 都被重发一次 range，而重发 range
    //  会把 page 从 20 重算成 18、把刻度画出来 ⇒ 什么都没写的控件也"自己长出刻度"。）
    curLo = (long)SendMessageW(hw, TBM_GETRANGEMIN, 0, 0);
    curHi = (long)SendMessageW(hw, TBM_GETRANGEMAX, 0, 0);
    if (min != VB6_SLD_UNSET || max != VB6_SLD_UNSET) {
        lo = (min == VB6_SLD_UNSET) ? curLo : min;
        hi = (max == VB6_SLD_UNSET) ? curHi : max;
        // VB6 的 Min/Max 是 Long，原生这条消息的 lParam 是两个 16 位半字（实测
        // MAKELONG(40000,50000) 读回 -25536/-15536 = 截断）⇒ 先钳到 i16 再下发，
        // 这样"我们答出去的数"与"控件真走得动的数"是同一个。超界那一档 VB6 怎么办：
        // 本机 OCX 未注册、拿不到真值 ⇒ 押后（见 029）。
        if (lo < VB6_SLD_I16MIN) lo = VB6_SLD_I16MIN;
        if (lo > VB6_SLD_I16MAX) lo = VB6_SLD_I16MAX;
        if (hi < VB6_SLD_I16MIN) hi = VB6_SLD_I16MIN;
        if (hi > VB6_SLD_I16MAX) hi = VB6_SLD_I16MAX;
        if (hi < lo) hi = lo;
        if (lo != curLo || hi != curHi) {
            SendMessageW(hw, TBM_SETRANGE, TRUE, MAKELONG((WORD)(SHORT)lo, (WORD)(SHORT)hi));
        }
    }
    if (smallChange != VB6_SLD_UNSET && smallChange >= 0) {
        SendMessageW(hw, TBM_SETLINESIZE, TRUE, (WPARAM)smallChange);
    }
    if (largeChange != VB6_SLD_UNSET && largeChange >= 0) {
        SendMessageW(hw, TBM_SETPAGESIZE, TRUE, (WPARAM)largeChange);
    }
    if (value != VB6_SLD_UNSET) {
        SendMessageW(hw, TBM_SETPOS, TRUE, (WPARAM)value);
    }
    if (tickFrequency != VB6_SLD_UNSET) {
        vb6_SdSetProp(hwnd, kSdTickFreq, (LONG)tickFrequency);
        if (tickFrequency > 0) {
            // 先清再设：控件自带的默认刻度不清掉的话，新旧两套刻度会叠在一起画。
            SendMessageW(hw, TBM_CLEARTICS, TRUE, 0);
            SendMessageW(hw, TBM_SETTICFREQ, (WPARAM)tickFrequency, 0);
        }
    }
    if (selectRange != VB6_SLD_UNSET && selectRange != 0) {
        LONG st = (LONG)GetWindowLongPtrW(hw, GWL_STYLE);
        if (!(st & TBS_ENABLESELRANGE)) {
            SetWindowLongPtrW(hw, GWL_STYLE, st | TBS_ENABLESELRANGE);
            vb6_SdReframe(hw);
        }
    }
    if (selStart != VB6_SLD_UNSET || selEnd != VB6_SLD_UNSET) {
        long a = (selStart == VB6_SLD_UNSET)
                     ? vb6_Slider_GetSelStart(hwnd) : selStart;
        long b = (selEnd == VB6_SLD_UNSET)
                     ? vb6_Slider_GetSelEnd(hwnd) : selEnd;
        if (a > b) a = b;                     // 原生不接受反向区段
        SendMessageW(hw, TBM_SETSEL, TRUE, MAKELONG((WORD)(SHORT)a, (WORD)(SHORT)b));
    }
}

// VB6 属性面：读样式位那一档（运行期写也走样式位，实测有效 —— 见文件头第 2 条）。
int vb6_Slider_GetOrientation(void* hwnd) {
    if (!hwnd) return VB6_SLD_HORZ;
    return ((GetWindowLongPtrW((HWND)hwnd, GWL_STYLE) & TBS_VERT) != 0)
               ? VB6_SLD_VERT : VB6_SLD_HORZ;
}

void vb6_Slider_SetOrientation(void* hwnd, int orientation) {
    LONG st;
    if (!hwnd) return;
    st = (LONG)GetWindowLongPtrW((HWND)hwnd, GWL_STYLE);
    if (orientation != 0) st |= TBS_VERT;
    else                  st &= ~TBS_VERT;
    SetWindowLongPtrW((HWND)hwnd, GWL_STYLE, st);
    // 光写样式不换帧不重排（实测对照组：只做 SetWindowPos 不翻样式 ⇒ 方向不变；
    // 翻了样式再做同一次 SetWindowPos ⇒ 方向跟着变）。尺寸传原值，只挂 FRAMECHANGED。
    {
        RECT wr;
        GetWindowRect((HWND)hwnd, &wr);
        SetWindowPos((HWND)hwnd, NULL, 0, 0,
                     (int)(wr.right - wr.left), (int)(wr.bottom - wr.top),
                     SWP_NOZORDER | SWP_NOMOVE | SWP_FRAMECHANGED);
    }
}

// C3 扩展（不是 VB6 属性，判据专用）：这杆到底**横着走还是竖着走**。
//
// 三条候选证人量过（`.build/slprobe/slmeasure3/4/5.c`），只有一条站得住：
//   * TBM_GETCHANNELRECT 的"长短边" **废** —— 它的 rect 永远把行程长度放在 x 分量：
//     300x40 的横杆与 40x300 的竖杆都答 (8,10)-(292,14)。第一发探针拿它判方向，
//     于是得出"Orientation 运行期改不动"的**错结论**（已订正，见文件头第 2 条）。
//   * "把滑块推到量程两端看位移轴" 也 **不单独够用** —— 133x27 的窗口里挂 TBS_VERT 时
//     竖直行程被压成 0（实测 Δx=Δy=0），Δ 退化成一潭死水，答"横"。
//   * **滑块自己的形状** 才是稳的：横杆的滑块是"竖块" (11x22)、竖杆的是"横块" (22x11)，
//     因为那张滑块图是随控件方向一起转的。实测四种情形（创建横 / 创建竖 / 运行期翻竖 /
//     窄高竖）里 w>h ⟺ 竖直 **四条全对**，而 Δ 判在"太矮的竖杆"上失手。
// 所以这里用形状判；位移量留在注释里当反例（别再有人拿 channel 矩形判方向）。
int vb6_Slider_TravelIsVert(void* hwnd) {
    RECT th;
    if (!hwnd) return 0;
    th.left = th.top = th.right = th.bottom = 0;
    SendMessageW((HWND)hwnd, TBM_GETTHUMBRECT, 0, (LPARAM)&th);
    return ((th.right - th.left) > (th.bottom - th.top)) ? 1 : 0;
}

// C3 扩展（证人）：控件当前**有没有**画刻度。实测这条只能证"有/无"，证不了频率
// （freq=10 与 freq=25 下 GETTICPOS(0..5) 读数逐字相同；清完刻度才全变 -1）。
int vb6_Slider_TickPresent(void* hwnd) {
    if (!hwnd) return 0;
    return (int)SendMessageW((HWND)hwnd, TBM_GETTICPOS, 0, 0) != -1 ? -1 : 0;  // VB6: True=-1
}

// `TickFrequency`：原生没有回读消息 ⇒ 自存读回；写侧真下发 TBM_SETTICFREQ。
int vb6_Slider_GetTickFrequency(void* hwnd) {
    return (int)vb6_SdGetProp(hwnd, kSdTickFreq, 0);
}

void vb6_Slider_SetTickFrequency(void* hwnd, int freq) {
    if (!hwnd) return;
    vb6_SdSetProp(hwnd, kSdTickFreq, (LONG)freq);
    if (freq > 0) {
        SendMessageW((HWND)hwnd, TBM_CLEARTICS, TRUE, 0);
        SendMessageW((HWND)hwnd, TBM_SETTICFREQ, (WPARAM)freq, 0);
    }
}

/* ======================= C29-SL-b: 值面 ======================= *
 * 全部直问直发控件，**一格自存都没有**（自存的只有 TickFrequency —— 原生问不出，见上）。
 * 四条实测口径（.build/slprobe/slmeasure6.c）：
 *   · 默认档：min=0 max=100、pos=0、**line=1 page=20**、selstart=0 selend=0。
 *     （VB6 文档给的是 SmallChange=1 / LargeChange=5：小那条对得上，大那条原生是 20，
 *      "没写过 LargeChange 时读回 20 还是折成 5"本机 OCX 未注册、拿不到 VB6 真值 ⇒ 押后，
 *      这里一律**照原生答 20**，不自作主张改成 5。）
 *   · 改 range 会把三样东西一起重算：pos 顶到新下限、page 从 20 变 18、selstart 跟到下限
 *     ⇒ 所以 Init 的顺序是 range → line/page → pos → 刻度 → Sel（见 vb6_Slider_Init 上面）。
 *   · 收窄 range 时 pos 由控件自己钳位（实测 80 → 收到 0..50 之后读回 50）。
 *   · `TBM_GETSELSTART/END` 在**清掉/没设过**时答 **(UINT)-1**（实测 CLEARSEL 之后）
 *     ⇒ 本 getter 把那一个折算成 0（"无区段"），免得 VB6 侧冒出个 -1 的起点。
 */

// VB6 的 Min/Max 是 Long，原生这条消息只有 16 位（实测超界被截成别的数）。
// 下发前钳到 i16，读回也是那一个钳过的值 ⇒ "答出去的"与"控件真走得动的"始终是同一个数。
int vb6_Slider_GetMin(void* hwnd) {
    if (!hwnd) return 0;
    return (int)SendMessageW((HWND)hwnd, TBM_GETRANGEMIN, 0, 0);
}

int vb6_Slider_GetMax(void* hwnd) {
    if (!hwnd) return 0;
    return (int)SendMessageW((HWND)hwnd, TBM_GETRANGEMAX, 0, 0);
}

static void vb6_SdSetRange(void* hwnd, LONG lo, LONG hi) {
    LONG a, b;
    if (!hwnd) return;
    a = (LONG)SendMessageW((HWND)hwnd, TBM_GETRANGEMIN, 0, 0);
    b = (LONG)SendMessageW((HWND)hwnd, TBM_GETRANGEMAX, 0, 0);
    if (lo == a && hi == b) return;          // 幂等: 不重发, 免得 range 一动就连带重算
    SendMessageW((HWND)hwnd, TBM_SETRANGE, TRUE,
                 MAKELONG((WORD)(SHORT)lo, (WORD)(SHORT)hi));
}

void vb6_Slider_SetMin(void* hwnd, int v) {
    LONG hi;
    if (!hwnd) return;
    if (v < VB6_SLD_I16MIN) v = VB6_SLD_I16MIN;
    if (v > VB6_SLD_I16MAX) v = VB6_SLD_I16MAX;
    hi = (LONG)SendMessageW((HWND)hwnd, TBM_GETRANGEMAX, 0, 0);
    if (v > hi) hi = v;                 // 原生不接受 min > max
    vb6_SdSetRange(hwnd, (LONG)v, hi);
}

void vb6_Slider_SetMax(void* hwnd, int v) {
    LONG lo;
    if (!hwnd) return;
    if (v < VB6_SLD_I16MIN) v = VB6_SLD_I16MIN;
    if (v > VB6_SLD_I16MAX) v = VB6_SLD_I16MAX;
    lo = (LONG)SendMessageW((HWND)hwnd, TBM_GETRANGEMIN, 0, 0);
    if (v < lo) lo = v;
    vb6_SdSetRange(hwnd, lo, (LONG)v);
}

int vb6_Slider_GetValue(void* hwnd) {
    if (!hwnd) return 0;
    return (int)SendMessageW((HWND)hwnd, TBM_GETPOS, 0, 0);
}

void vb6_Slider_SetValue(void* hwnd, int v) {
    if (!hwnd) return;
    // 越界**不自己钳**：实测控件就钳（150→100、-5→10），让控件答这一档。
    SendMessageW((HWND)hwnd, TBM_SETPOS, TRUE, (WPARAM)v);
}

int vb6_Slider_GetSmallChange(void* hwnd) {
    if (!hwnd) return 0;
    return (int)SendMessageW((HWND)hwnd, TBM_GETLINESIZE, 0, 0);
}

void vb6_Slider_SetSmallChange(void* hwnd, int v) {
    if (!hwnd) return;
    SendMessageW((HWND)hwnd, TBM_SETLINESIZE, TRUE, (WPARAM)v);
}

int vb6_Slider_GetLargeChange(void* hwnd) {
    if (!hwnd) return 0;
    return (int)SendMessageW((HWND)hwnd, TBM_GETPAGESIZE, 0, 0);
}

void vb6_Slider_SetLargeChange(void* hwnd, int v) {
    if (!hwnd) return;
    SendMessageW((HWND)hwnd, TBM_SETPAGESIZE, TRUE, (WPARAM)v);
}

// SelectRange = 原生样式位 TBS_ENABLESELRANGE。实测**运行期改这一位有效**（补挂之后
// SETSEL 才答得回来、且换帧后区段画出来）⇒ 写口登记；挂上时按当前 Sel 重发一次 SETSEL，
// 否则位是挂上了、区段还是空的。
int vb6_Slider_GetSelectRange(void* hwnd) {
    if (!hwnd) return 0;
    return ((GetWindowLongPtrW((HWND)hwnd, GWL_STYLE) & TBS_ENABLESELRANGE) != 0) ? -1 : 0;
}

void vb6_Slider_SetSelectRange(void* hwnd, int on) {
    LONG st;
    if (!hwnd) return;
    st = (LONG)GetWindowLongPtrW((HWND)hwnd, GWL_STYLE);
    if (on) {
        if (st & TBS_ENABLESELRANGE) return;
        SetWindowLongPtrW((HWND)hwnd, GWL_STYLE, st | TBS_ENABLESELRANGE);
        vb6_SdReframe((HWND)hwnd);
        // 补挂之后按当前两端重发一次：原生那两位在没挂时 SETSEL 是不生效的（实测）。
        {
            LONG a = vb6_Slider_GetSelStart(hwnd);
            LONG b = vb6_Slider_GetSelEnd(hwnd);
            if (a > b) a = b;
            SendMessageW((HWND)hwnd, TBM_SETSEL, TRUE, MAKELONG((WORD)(SHORT)a, (WORD)(SHORT)b));
        }
    } else {
        if (!(st & TBS_ENABLESELRANGE)) return;
        SetWindowLongPtrW((HWND)hwnd, GWL_STYLE, st & ~(LONG)TBS_ENABLESELRANGE);
        vb6_SdReframe((HWND)hwnd);
    }
}

int vb6_Slider_GetSelStart(void* hwnd) {
    LONG v;
    if (!hwnd) return 0;
    v = (LONG)SendMessageW((HWND)hwnd, TBM_GETSELSTART, 0, 0);
    return (v == -1) ? 0 : (int)v;      // 实测"没设过/已 CLEARSEL"答 (UINT)-1 ⇒ 折成 0
}

int vb6_Slider_GetSelEnd(void* hwnd) {
    LONG v;
    if (!hwnd) return 0;
    v = (LONG)SendMessageW((HWND)hwnd, TBM_GETSELEND, 0, 0);
    return (v == -1) ? 0 : (int)v;
}

// VB6 那侧 SelStart 不能大于 SelEnd（区段是闭区间）。两端各自 setter 时**互相顶**：
// 设起点超过终点 ⇒ 终点跟上来（原生 SETSELSTART 会自己夹，实测分开发 15/60 正常往返）。
void vb6_Slider_SetSelStart(void* hwnd, int v) {
    LONG hi;
    if (!hwnd) return;
    hi = vb6_Slider_GetSelEnd(hwnd);
    if (v > hi) hi = v;
    SendMessageW((HWND)hwnd, TBM_SETSEL, TRUE, MAKELONG((WORD)(SHORT)v, (WORD)(SHORT)hi));
}

void vb6_Slider_SetSelEnd(void* hwnd, int v) {
    LONG lo;
    if (!hwnd) return;
    lo = vb6_Slider_GetSelStart(hwnd);
    if (v < lo) lo = v;
    SendMessageW((HWND)hwnd, TBM_SETSEL, TRUE, MAKELONG((WORD)(SHORT)lo, (WORD)(SHORT)v));
}

#endif /* _WIN32 */
