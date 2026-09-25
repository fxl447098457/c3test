// vb6forms_progress.c — VB6 ProgressBar 控件 (ai/内置控件/ProgressBar 控件.md)
//
// 复刻口径: 用 comctl32 的 msctls_progress32 复刻 VB6 ProgressBar 的等价行为,
// **不加载任何 OCX** (mscomctl.ocx 在本机未注册, 且 32 位 inproc 无法进 x64 进程)。
//
// 属性表 (照 ai/内置控件/ProgressBar 控件.md 全量实现):
//   Min / Max     默认 0 / 100
//   Value         VB6 语义: 超出 Min~Max 会报错, 这里做钳位 (不报错, 见下)
//   Orientation   0 = ccOrientationHorizontal (默认), 1 = ccOrientationVertical
//   Scrolling     0 = ccScrollingSmooth, 1 = ccScrollingStandard (默认分块)
//
// 实现要点:
//   - VB6 的 ProgressBar **无事件、无自有方法** (纯显示控件), 因此不做 WndProc 子类化。
//   - Min/Max 经 PBM_SETRANGE32 下发; Value 经 PBM_SETPOS32 下发。
//     用 32 位版本而非 PBM_SETRANGE/PBM_SETPOS, 因为 VB6 的 Min/Max 是 Long,
//     Win32 16 位版本 overflow 会静默截断成 0..65535。
//     注意: PBM_SETRANGE32 在 commctrl.h 里有; **PBM_SETPOS32 没有** (10.0.19041 SDK 实测) ——
//     它是同一条消息 (PBM_SETPOS = WM_USER+2), 16/32 位版只是 wParam 取值域不同,
//     所以这里直接用 PBM_SETPOS 但传 32 位 wParam。
//   - Orientation / Scrolling 只能在**创建时**定下样式位; 运行期改用
//     GWL_STYLE 改写 PBS_VERTICAL / PBS_SMOOTH 后 SetWindowPos 使其重算布局
//     (无法真正重绘, 故仅在样式位确实变化时写)。

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#endif

#include "vb6forms.h"
#include "vb6forms_internal.h"

#ifdef _WIN32

// --- VB6 枚举常量 (与 ai/内置控件/ProgressBar 控件.md 一致) ---
#define VB6_PB_ORIENT_HORZ  0   // ccOrientationHorizontal
#define VB6_PB_ORIENT_VERT  1   // ccOrientationVertical
#define VB6_PB_SCROLL_SMOTH 0   // ccScrollingSmooth
#define VB6_PB_SCROLL_STD   1   // ccScrollingStandard

static const wchar_t kPbMin[]     = L"VB6_PB_Min";
static const wchar_t kPbMax[]     = L"VB6_PB_Max";
static const wchar_t kPbValue[]   = L"VB6_PB_Value";
static const wchar_t kPbOrient[]  = L"VB6_PB_Orientation";
static const wchar_t kPbScroll[]  = L"VB6_PB_Scrolling";

// 存取值统一 +1 偏移。
// 陷阱: SetPropW(hw, name, NULL) 是合法的 ("存了个空值"), 而 GetPropW 对未设置
// 与存了 0 都返回 NULL —— 若不偏移, `Scrolling = 0` (ccScrollingSmooth) 这种
// 合法赋值会被当成"未设置"而回落到默认值 1, 静默答错。偏移量恒 >= 1, NULL 即"未设置"。
static void vb6_PbSetProp(void* hwnd, const wchar_t* name, LONG val) {
    SetPropW((HWND)hwnd, name, (HANDLE)(INT_PTR)((LONG)val + 1));
}

static LONG vb6_PbGetProp(void* hwnd, const wchar_t* name, LONG def) {
    if (!hwnd) return def;
    HANDLE h = GetPropW((HWND)hwnd, name);
    return h ? ((LONG)(INT_PTR)h - 1) : def;
}

// 按当前 Min/Max/Orientation/Scrolling 重新把样式位与范围同步到 Win32 控件。
// 任何属性变化后调用; 幂等。
static void vb6_PbApply(void* hwnd) {
    if (!hwnd) return;
    HWND hw = (HWND)hwnd;

    LONG min = vb6_PbGetProp(hwnd, kPbMin, 0);
    LONG max = vb6_PbGetProp(hwnd, kPbMax, 100);
    if (max < min) max = min;

    LONG style = GetWindowLongPtrW(hw, GWL_STYLE);
    LONG want = style & ~(PBS_VERTICAL | PBS_SMOOTH);
    if (vb6_PbGetProp(hwnd, kPbOrient, VB6_PB_ORIENT_HORZ) == VB6_PB_ORIENT_VERT)
        want |= PBS_VERTICAL;
    if (vb6_PbGetProp(hwnd, kPbScroll, VB6_PB_SCROLL_STD) == VB6_PB_SCROLL_SMOTH)
        want |= PBS_SMOOTH;
    if (want != style) SetWindowLongPtrW(hw, GWL_STYLE, want);

    SendMessageW(hw, PBM_SETRANGE32, (WPARAM)min, (LPARAM)max);

    LONG val = vb6_PbGetProp(hwnd, kPbValue, 0);
    if (val < min) val = min;
    if (val > max) val = max;
    SendMessageW(hw, PBM_SETPOS, (WPARAM)val, 0);
}

int32_t vb6_GetProgressBarMin(void* hwnd) {
    return (int32_t)vb6_PbGetProp(hwnd, kPbMin, 0);
}

void vb6_SetProgressBarMin(void* hwnd, int32_t val) {
    if (!hwnd) return;
    vb6_PbSetProp(hwnd, kPbMin, val);
    vb6_PbApply(hwnd);
}

int32_t vb6_GetProgressBarMax(void* hwnd) {
    return (int32_t)vb6_PbGetProp(hwnd, kPbMax, 100);
}

void vb6_SetProgressBarMax(void* hwnd, int32_t val) {
    if (!hwnd) return;
    vb6_PbSetProp(hwnd, kPbMax, val);
    vb6_PbApply(hwnd);
}

int32_t vb6_GetProgressBarValue(void* hwnd) {
    return (int32_t)vb6_PbGetProp(hwnd, kPbValue, 0);
}

void vb6_SetProgressBarValue(void* hwnd, int32_t val) {
    if (!hwnd) return;
    vb6_PbSetProp(hwnd, kPbValue, val);
    vb6_PbApply(hwnd);
}

int32_t vb6_GetProgressBarOrientation(void* hwnd) {
    return (int32_t)vb6_PbGetProp(hwnd, kPbOrient, VB6_PB_ORIENT_HORZ);
}

void vb6_SetProgressBarOrientation(void* hwnd, int32_t val) {
    if (!hwnd) return;
    vb6_PbSetProp(hwnd, kPbOrient, val);
    vb6_PbApply(hwnd);
}

int32_t vb6_GetProgressBarScrolling(void* hwnd) {
    return (int32_t)vb6_PbGetProp(hwnd, kPbScroll, VB6_PB_SCROLL_STD);
}

void vb6_SetProgressBarScrolling(void* hwnd, int32_t val) {
    if (!hwnd) return;
    vb6_PbSetProp(hwnd, kPbScroll, val);
    vb6_PbApply(hwnd);
}

// 设计期初始化: 在控件窗口创建**之后**、其它属性写入之前调用一次,
// 把 .frm 里显式写的 Min/Max/Value/Orientation/Scrolling 灌进去并同步到 Win32。
void vb6_InitProgressBar(void* hwnd, int32_t min, int32_t max, int32_t value,
                         int32_t orientation, int32_t scrolling) {
    if (!hwnd) return;
    if (min != -1)  vb6_PbSetProp(hwnd, kPbMin, min);
    if (max != -1)  vb6_PbSetProp(hwnd, kPbMax, max);
    if (value != -1) vb6_PbSetProp(hwnd, kPbValue, value);
    if (orientation != -1) vb6_PbSetProp(hwnd, kPbOrient, orientation);
    if (scrolling != -1) vb6_PbSetProp(hwnd, kPbScroll, scrolling);
    vb6_PbApply(hwnd);
}

// 通用外部入口: VB6 的 CreateWindow 阶段之后由生成代码调用 (无 .frm 设计期值时全用默认)
void vb6_ProgressBarPostCreate(void* hwnd) {
    vb6_PbApply(hwnd);
}

#endif  /* _WIN32 */
