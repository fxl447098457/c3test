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

#ifndef TBS_AUTOTICKS
#define TBS_AUTOTICKS  0x0001
#endif
#ifndef TBS_VERT
#define TBS_VERT       0x0002
#endif
#ifndef TBM_SETTICFREQ
#define TBM_SETTICFREQ   (WM_USER + 20)
#endif
#ifndef TBM_GETTICPOS
#define TBM_GETTICPOS    (WM_USER + 15)
#endif
#ifndef TBM_GETTHUMBRECT
#define TBM_GETTHUMBRECT (WM_USER + 17)
#endif
#ifndef TBM_CLEARTICS
#define TBM_CLEARTICS    (WM_USER + 9)
#endif
#ifndef TBM_GETRANGEMIN
#define TBM_GETRANGEMIN  (WM_USER + 1)
#endif
#ifndef TBM_GETRANGEMAX
#define TBM_GETRANGEMAX  (WM_USER + 2)
#endif
#ifndef TBM_GETPOS
#define TBM_GETPOS       (WM_USER)
#endif
#ifndef TBM_SETPOS
#define TBM_SETPOS       (WM_USER + 5)
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

// 设计期下发。哨兵 -999 = .frm 里**没写过**这一项 ⇒ 一条消息都不发，保持控件默认
// （同 vb6_TreeView_Init：拿 -1 当"未写"会让设计期永远设不上合法的 0/-1 那一档）。
// Orientation 不走这里 —— 它是创建参数，cgen 直接立进 vb6_CreateControl 的样式位。
void vb6_Slider_Init(void* hwnd, long tickFrequency) {
    if (!hwnd) return;
    if (tickFrequency == -999) return;
    vb6_SdSetProp(hwnd, kSdTickFreq, (LONG)tickFrequency);
    if (tickFrequency > 0) {
        // 先清再设：控件自带的默认刻度不清掉的话，新旧两套刻度会叠在一起画。
        SendMessageW((HWND)hwnd, TBM_CLEARTICS, TRUE, 0);
        SendMessageW((HWND)hwnd, TBM_SETTICFREQ, (WPARAM)tickFrequency, 0);
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

#endif /* _WIN32 */
