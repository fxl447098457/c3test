// vb6forms_sstab.c — VB6 SSTab 控件 (TabDlg.SSTab) 复刻
//
// 用 comctl32 的 **SysTabControl32** 复刻 VB6 选项卡容器, **不加载 TABCTL32.OCX**
// (本机未注册; 且 32 位 inproc OCX 无法进 x64 进程)。
//
// 与 StatusBar(#10) 的一处重要差别: msctls_status32 两个版本的 comctl32 都不注册
// (要 RTL 自注册兜底), 而 **SysTabControl32 是注册好的** —— 实测 x64/x86 两个进程里
// GetClassInfoW 都直接成功, 连 InitCommonControlsEx 之前就已经在。所以这里不需要那个兜底。
//
// 属性表 (照 ai/内置控件/SSTab 控件（选项卡容器).md 全量实现):
//   Tabs            页总数
//   Tab             当前活动页索引 (**0 基**, 与其它 VB6 集合的 1 基不同)
//   TabCaption(i)   第 i 页标题          —— 带下标的索引属性
//   TabVisible(i)   第 i 页是否可见       —— 带下标的索引属性
//   TabOrientation  0=上(默认) 1=下 2=左 3=右
//   TabStyle        0=选项卡对话框式(默认) 1=属性页式
//   TabsPerRow      每行显示的标签数
//   WordWrap        标题过长时是否换行
//
// ===================== 容器语义: 页码从 Left 反推 =====================
//
// 这是本控件与已做的 ProgressBar/ImageList/StatusBar 最大的不同: SSTab 是**容器**。
// VB6 把每一页的子控件都摆在同一份坐标里, 靠把非活动页的子控件 Left **减 75000 缇**
// 推出可见区来隐藏。微软 KB Q187562 / Q206904 都明确写了这一点:
//   "The SSTab control hides the controls present in the inactive tabs by setting
//    their Left property values less by 75000."
// 所以 .frm 里子控件是顺序平铺在 SSTab 的 Begin..End 之内的, **没有任何显式的页号字段**,
// 页码只能从 Left 反推:
//       storedLeft = realLeft - 75000 * page
// 取让 realLeft 落回 [0, 75000) 的 page 即为页号 (阈值用 -37500, 免得把本来就有
// 小负值 Left 的控件误判到下一页)。这段反推在 codegen 里做, RTL 只认页码。
//
// 为什么不照抄 VB6 的"挪 Left"做法? 因为那样 `Debug.Print Label1.Left` 会读出一个被内部
// hack 挪过的数, 而 VB6 里这个属性就是设计期的那个值。这里改为**给子控件登记页号 +
// 切页时 ShowWindow 开关**, Left 保持原值不动, 对外口径干净。

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#endif

#include "vb6forms.h"
#include "vb6forms_internal.h"
#include "vb6rtl_bstr.h"

#ifdef _WIN32

// ===================== VB6 枚举常量 =====================
#define VB6_TAB_ORIENT_TOP     0
#define VB6_TAB_ORIENT_BOTTOM  1
#define VB6_TAB_ORIENT_LEFT    2
#define VB6_TAB_ORIENT_RIGHT   3

#define VB6_TAB_STYLE_DIALOG    0   // 选项卡对话框式 (Win32 TCS_TABS)
#define VB6_TAB_STYLE_PROPPAGE  1   // 属性页式 (扁平按钮式)

// ===================== 实例 =====================

typedef struct {
    void* hwnd;      // 子控件句柄
    int   page;      // 所属页 (0 基)
} Vb6TabChild;

typedef struct {
    HWND         hwnd;         // SysTabControl32
    wchar_t**    captions;     // 平行表: 每页标题
    int*         visible;      // 平行表: 每页可见与否
    int          count;        // 页数 (逻辑页总数, 含被 TabVisible 藏掉的)
    int          cur;          // 当前活动页 (逻辑号, 0 基)
    int          prev;         // 上一次活动页 —— Click(PreviousTab) 要这个值
    int          orientation;
    int          tabStyle;
    int          tabsPerRow;
    int          wordWrap;
    Vb6TabChild* children;
    int          childCount;
    int          childCap;
} Vb6SSTab;

// 实例表: 窗体数量级, 线性查找足够 (与 vb6forms_statusbar.c 同口径)。
#define VB6_SSTAB_MAX 64
static Vb6SSTab* g_tabs[VB6_SSTAB_MAX];
static int       g_tabCount = 0;

static Vb6SSTab* sstabFind(HWND h) {
    if (!h) return NULL;
    for (int i = 0; i < g_tabCount; i++)
        if (g_tabs[i] && g_tabs[i]->hwnd == h) return g_tabs[i];
    return NULL;
}

static Vb6SSTab* sstabEnsure(HWND h) {
    Vb6SSTab* t = sstabFind(h);
    if (t) return t;
    if (g_tabCount >= VB6_SSTAB_MAX) return NULL;
    t = (Vb6SSTab*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(Vb6SSTab));
    if (!t) return NULL;
    t->hwnd = h;
    t->cur = 0;
    t->prev = 0;
    t->orientation = VB6_TAB_ORIENT_TOP;
    t->tabStyle = VB6_TAB_STYLE_DIALOG;
    g_tabs[g_tabCount++] = t;
    return t;
}

// ---- 逻辑页 ↔ 视图项 ------------------------------------------------
//
// TabVisible(i)=False 的页在 VB6 里是连标签带内容一起消失, 而 Win32 TabCtrl **没有
// per-item 可见位**, 只能"不插入"。代价是 view 下标与逻辑页号错位, 所以下面这两张
// 方向的换算是唯一入口 —— 凡是碰 GetCurSel/SetCurSel 的返回值都不能直接拿去当页号用。
static int sstabLogicalToView(const Vb6SSTab* t, int logical) {
    if (!t || logical < 0 || logical >= t->count) return -1;
    if (!t->visible || !t->visible[logical]) return -1;
    int v = 0;
    for (int i = 0; i < logical; i++)
        if (t->visible[i]) v++;
    return v;
}

static int sstabViewToLogical(const Vb6SSTab* t, int view) {
    if (!t || !t->visible || view < 0) return -1;
    for (int i = 0; i < t->count; i++) {
        if (!t->visible[i]) continue;
        if (view == 0) return i;
        view--;
    }
    return -1;
}

// 重建全部可见标签项。改了任何影响布局的样式之后必须走一遍 —— 否则旧 item 尺寸不重算。
static void sstabRebuildVisibleTabs(Vb6SSTab* t) {
    if (!t || !t->hwnd) return;
    TabCtrl_DeleteAllItems(t->hwnd);
    if (!t->captions) return;
    for (int i = 0; i < t->count; i++) {
        if (t->visible && !t->visible[i]) continue;
        TCITEMW ti;
        ZeroMemory(&ti, sizeof(ti));
        ti.mask = TCIF_TEXT;
        ti.pszText = t->captions[i] ? t->captions[i] : (wchar_t*)L"";
        TabCtrl_InsertItem(t->hwnd, TabCtrl_GetItemCount(t->hwnd), &ti);
    }
}

// VB6 的样位 → TCS_*。位置类样式 (TCS_BOTTOM / TCS_VERTICAL / TCS_RIGHT) 只在
// 创建时给最稳, 运行期改靠 SetWindowLongPtr + 重建 item 强制重排。
static void sstabApplyRowMetrics(Vb6SSTab* t);   // 定义在下面, sstabReapplyStyle 用得到
static void sstabPostSelChange(Vb6SSTab* t);  // SetTab 末尾补发 TCN_SELCHANGE, 定义在下方

static LONG sstabStyleBits(const Vb6SSTab* t, LONG base) {
    LONG s = base;
    if (t->orientation == VB6_TAB_ORIENT_BOTTOM) s |= TCS_BOTTOM;
    if (t->orientation == VB6_TAB_ORIENT_LEFT)   s |= TCS_VERTICAL;
    if (t->orientation == VB6_TAB_ORIENT_RIGHT)  s |= TCS_VERTICAL | TCS_RIGHT;
    if (t->tabStyle == VB6_TAB_STYLE_PROPPAGE)   s |= TCS_BUTTONS | TCS_FLATBUTTONS;
    if (t->wordWrap)                             s |= TCS_MULTILINE;
    return s;
}

#define SST_STYLE_MASK (TCS_BOTTOM | TCS_VERTICAL | TCS_RIGHT | TCS_BUTTONS \
                        | TCS_FLATBUTTONS | TCS_MULTILINE)

static void sstabReapplyStyle(Vb6SSTab* t) {
    if (!t || !t->hwnd) return;
    LONG st = (LONG)GetWindowLongPtr(t->hwnd, GWL_STYLE);
    SetWindowLongPtr(t->hwnd, GWL_STYLE, sstabStyleBits(t, st & ~SST_STYLE_MASK));
    sstabRebuildVisibleTabs(t);
    sstabApplyRowMetrics(t);
}

// TabsPerRow: Win32 没有"第行几个标签"的直接位, 折成最小标签宽
// (可视宽 / 每行个数), 让换行按 VB6 的意图走。0 或非法值 = 不限制。
static void sstabApplyRowMetrics(Vb6SSTab* t) {
    if (!t || !t->hwnd) return;
    if (t->tabsPerRow <= 0) { TabCtrl_SetMinTabWidth(t->hwnd, 0); return; }
    RECT rc;
    GetClientRect(t->hwnd, &rc);
    int w = rc.right - rc.left;
    if (w <= 0) return;
    // 竖向标签下滚动方向不同, 这里按可视区间 / 每行数给最小宽。
    int minW = w / t->tabsPerRow;
    if (minW < 8) minW = 8;
    TabCtrl_SetMinTabWidth(t->hwnd, minW);
}

// 切页后按"登记的页号"开关子控件 —— 见文件头: 不动 Left, 只动可见性。
static void sstabRefreshChildren(Vb6SSTab* t) {
    if (!t) return;
    for (int i = 0; i < t->childCount; i++) {
        HWND c = (HWND)t->children[i].hwnd;
        if (!c) continue;
        int p = t->children[i].page;
        // 两种隐藏: 不是当前页 / 该页被 TabVisible=False 藏了
        int show = (p == t->cur && p >= 0 && p < t->count
                    && (!t->visible || t->visible[p]));
        ShowWindow(c, show ? SW_SHOW : SW_HIDE);
    }
}

// ===================== 初始化 =====================

void vb6_SSTab_Init(void* tabHwnd, int tabs, int curTab, int orientation, int tabStyle,
                    int tabsPerRow, int wordWrap) {
    if (!tabHwnd) return;
    Vb6SSTab* t = sstabEnsure((HWND)tabHwnd);
    if (!t) return;

    t->orientation = orientation;
    t->tabStyle = tabStyle;
    t->tabsPerRow = tabsPerRow;
    t->wordWrap = wordWrap;

    // 清掉旧平行表, 重新按 tabs 建
    if (t->captions) {
        for (int i = 0; i < t->count; i++)
            if (t->captions[i]) HeapFree(GetProcessHeap(), 0, t->captions[i]);
        HeapFree(GetProcessHeap(), 0, t->captions);
        t->captions = NULL;
    }
    if (t->visible) { HeapFree(GetProcessHeap(), 0, t->visible); t->visible = NULL; }
    t->count = 0;

    if (tabs > 0) {
        t->captions = (wchar_t**)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(wchar_t*) * tabs);
        t->visible = (int*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(int) * tabs);
        if (t->captions && t->visible) {
            for (int i = 0; i < tabs; i++) { t->captions[i] = NULL; t->visible[i] = 1; }
            t->count = tabs;
        }
    }

    sstabReapplyStyle(t);

    t->cur = 0;
    if (curTab > 0 && curTab < t->count) t->cur = curTab;
    if (t->visible && t->cur < t->count && !t->visible[t->cur]) {
        // VB6 不会让活动页落在隐藏页上; 顺延到第一个可见页。
        int first = -1;
        for (int i = 0; i < t->count; i++) if (t->visible[i]) { first = i; break; }
        t->cur = (first < 0) ? 0 : first;
    }
    t->prev = t->cur;
    int view = sstabLogicalToView(t, t->cur);
    if (view >= 0) TabCtrl_SetCurSel(t->hwnd, view);
    sstabRefreshChildren(t);
}

// --- 运行期样式重算出口: 改了 orientation/style/wordWrap 之后统一走这里 ---
static void sstabAfterVisualChange(Vb6SSTab* t) {
    sstabReapplyStyle(t);
    int view = sstabLogicalToView(t, t->cur);
    if (view >= 0) TabCtrl_SetCurSel(t->hwnd, view);
    sstabRefreshChildren(t);
}

// ===================== 属性: Tabs / Tab =====================

// Tabs 变多时补空页, 变少时截掉 —— 与 StatusBar 的 Panels 不同, TabCtrl 的 item 数
// 就是页本身, 没有平行集合对象, 所以 count 与视图 item 数必须一起动。
static void sstabResize(Vb6SSTab* t, int n) {
    if (!t || n < 0 || n == t->count) return;
    wchar_t** nc = NULL;
    int* nv = NULL;
    if (n > 0) {
        nc = (wchar_t**)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(wchar_t*) * n);
        nv = (int*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(int) * n);
        if (!nc || !nv) {
            if (nc) HeapFree(GetProcessHeap(), 0, nc);
            if (nv) HeapFree(GetProcessHeap(), 0, nv);
            return;
        }
        int keep = (n < t->count) ? n : t->count;
        for (int i = 0; i < keep; i++) { nc[i] = t->captions[i]; nv[i] = t->visible[i]; }
        // 缩容时**必须**显式释放被截掉那几页的字符串 (否则泄漏), 且只能释放
        // 超出新长度的那些 —— 保留段已经搬进 nc, 不能碰。
        for (int i = keep; i < t->count; i++)
            if (t->captions[i]) HeapFree(GetProcessHeap(), 0, t->captions[i]);
        for (int i = keep; i < n; i++) nv[i] = 1;
    } else {
        for (int i = 0; i < t->count; i++)
            if (t->captions[i]) HeapFree(GetProcessHeap(), 0, t->captions[i]);
    }
    if (t->captions) HeapFree(GetProcessHeap(), 0, t->captions);
    if (t->visible) HeapFree(GetProcessHeap(), 0, t->visible);
    t->captions = nc;
    t->visible = nv;
    t->count = n;
    if (t->cur >= n) t->cur = (n > 0) ? n - 1 : 0;
}

int32_t vb6_SSTab_GetTabs(void* tabHwnd) {
    Vb6SSTab* t = sstabFind((HWND)tabHwnd);
    return t ? (int32_t)t->count : 0;
}

void vb6_SSTab_SetTabs(void* tabHwnd, int32_t n) {
    Vb6SSTab* t = sstabFind((HWND)tabHwnd);
    if (!t) return;
    sstabResize(t, (int)n);
    sstabAfterVisualChange(t);
}

int32_t vb6_SSTab_GetTab(void* tabHwnd) {
    Vb6SSTab* t = sstabFind((HWND)tabHwnd);
    return t ? (int32_t)t->cur : 0;
}

void vb6_SSTab_SetTab(void* tabHwnd, int32_t idx) {
    Vb6SSTab* t = sstabFind((HWND)tabHwnd);
    if (!t) return;
    if (idx < 0 || idx >= t->count) return;
    if (t->visible && !t->visible[idx]) return;   // VB6: 不能切到隐藏页
    t->prev = t->cur;
    t->cur = (int)idx;
    int view = sstabLogicalToView(t, t->cur);
    if (view >= 0) TabCtrl_SetCurSel(t->hwnd, view);
    sstabRefreshChildren(t);

    // VB6 的 SSTab 在**页被切换**时就触发 Click(PreviousTab), 程序化改 Tab 也算。
    // 而 comctl32 只在**用户点标签**时才自己发 TCN_SELCHANGE —— 实测程序化
    // TabCtrl_SetCurSel 一点通知都不发, 于是 VB 侧的 Click 永远不触发。
    // 所以这里补发一条: 让 SetTab 与真实点击走同一条 WM_NOTIFY 路径。
    // 不会递归: OnSelChange 不调 SetTab。
    sstabPostSelChange(t);
}

// 给父窗口发一条 TCN_SELCHANGE —— 与 comctl32 在用户点击时发的那条同构。
static void sstabPostSelChange(Vb6SSTab* t) {
    if (!t || !t->hwnd) return;
    HWND parent = GetParent(t->hwnd);
    if (!parent) return;
    NMHDR nm;
    ZeroMemory(&nm, sizeof(nm));
    nm.hwndFrom = t->hwnd;
    nm.idFrom = (UINT_PTR)GetWindowLongPtr(t->hwnd, GWLP_ID);
    nm.code = TCN_SELCHANGE;
    SendMessageW(parent, WM_NOTIFY, (WPARAM)nm.idFrom, (LPARAM)&nm);
}

// ===================== 属性: TabCaption(i) / TabVisible(i) =====================
//
// 这两个是"带下标的索引属性", 不是集合对象 (VB6 里写作 SSTab1.TabCaption(0))。
// 与 StatusBar 的 Panels(i).Text 不同, 后者是集合成员的属性, 前者是控件自身的
// 参数化属性 —— codegen 侧也得走不同的分支。

void* vb6_SSTab_GetTabCaption(void* tabHwnd, int32_t idx) {
    Vb6SSTab* t = sstabFind((HWND)tabHwnd);
    if (!t || idx < 0 || idx >= t->count) return (void*)vb6_BSTR_Empty();
    return (void*)vb6_BSTR_FromStr(t->captions[idx] ? t->captions[idx] : L"");
}

void vb6_SSTab_SetTabCaption(void* tabHwnd, int32_t idx, void* bstr) {
    Vb6SSTab* t = sstabFind((HWND)tabHwnd);
    if (!t || idx < 0 || idx >= t->count) return;
    const wchar_t* s = (const wchar_t*)bstr;
    if (t->captions[idx]) { HeapFree(GetProcessHeap(), 0, t->captions[idx]); t->captions[idx] = NULL; }
    if (s && *s) {
        int n = (int)lstrlenW(s);
        wchar_t* cp = (wchar_t*)HeapAlloc(GetProcessHeap(), 0, sizeof(wchar_t) * (n + 1));
        if (cp) { memcpy(cp, s, sizeof(wchar_t) * n); cp[n] = 0; t->captions[idx] = cp; }
    }
    // 只改文字, 不必重建整个样式, 但视图里的 item 要跟着更新
    int view = sstabLogicalToView(t, idx);
    if (view >= 0) {
        TCITEMW ti;
        ZeroMemory(&ti, sizeof(ti));
        ti.mask = TCIF_TEXT;
        ti.pszText = t->captions[idx] ? t->captions[idx] : (wchar_t*)L"";
        TabCtrl_SetItem(t->hwnd, view, &ti);
    }
}

int32_t vb6_SSTab_GetTabVisible(void* tabHwnd, int32_t idx) {
    Vb6SSTab* t = sstabFind((HWND)tabHwnd);
    if (!t || idx < 0 || idx >= t->count) return 0;
    return t->visible ? (int32_t)(t->visible[idx] ? -1 : 0) : 0;   // VB6 True = -1
}

void vb6_SSTab_SetTabVisible(void* tabHwnd, int32_t idx, int32_t v) {
    Vb6SSTab* t = sstabFind((HWND)tabHwnd);
    if (!t || idx < 0 || idx >= t->count || !t->visible) return;
    t->visible[idx] = v ? 1 : 0;
    if (t->cur == idx && !t->visible[idx]) {
        int first = -1;
        for (int i = 0; i < t->count; i++) if (t->visible[i]) { first = i; break; }
        t->cur = (first < 0) ? 0 : first;
    }
    // 隐藏页会改变"视图项"的个数与编号 → 必须整表重建, 不能只改一个 item
    sstabAfterVisualChange(t);
}

// ===================== 属性: 外观四件套 =====================

int32_t vb6_SSTab_GetTabOrientation(void* tabHwnd) {
    Vb6SSTab* t = sstabFind((HWND)tabHwnd);
    return t ? (int32_t)t->orientation : 0;
}

void vb6_SSTab_SetTabOrientation(void* tabHwnd, int32_t v) {
    Vb6SSTab* t = sstabFind((HWND)tabHwnd);
    if (!t || v < 0 || v > 3) return;
    t->orientation = (int)v;
    sstabAfterVisualChange(t);
}

int32_t vb6_SSTab_GetTabStyle(void* tabHwnd) {
    Vb6SSTab* t = sstabFind((HWND)tabHwnd);
    return t ? (int32_t)t->tabStyle : 0;
}

void vb6_SSTab_SetTabStyle(void* tabHwnd, int32_t v) {
    Vb6SSTab* t = sstabFind((HWND)tabHwnd);
    if (!t || v < 0 || v > 1) return;
    t->tabStyle = (int)v;
    sstabAfterVisualChange(t);
}

int32_t vb6_SSTab_GetTabsPerRow(void* tabHwnd) {
    Vb6SSTab* t = sstabFind((HWND)tabHwnd);
    return t ? (int32_t)t->tabsPerRow : 0;
}

void vb6_SSTab_SetTabsPerRow(void* tabHwnd, int32_t v) {
    Vb6SSTab* t = sstabFind((HWND)tabHwnd);
    if (!t || v < 0) return;
    t->tabsPerRow = (int)v;
    sstabApplyRowMetrics(t);
}

int32_t vb6_SSTab_GetWordWrap(void* tabHwnd) {
    Vb6SSTab* t = sstabFind((HWND)tabHwnd);
    return t ? (int32_t)(t->wordWrap ? -1 : 0) : 0;
}

void vb6_SSTab_SetWordWrap(void* tabHwnd, int32_t v) {
    Vb6SSTab* t = sstabFind((HWND)tabHwnd);
    if (!t) return;
    t->wordWrap = v ? 1 : 0;
    sstabAfterVisualChange(t);
}

// ===================== 容器: 子控件登记 =====================

// codegen 对每个 .frm 里落在 SSTab Begin..End 内的子控件调一次, page 由 Left 反推。
void vb6_SSTab_RegisterChild(void* tabHwnd, void* childHwnd, int32_t page) {
    Vb6SSTab* t = sstabFind((HWND)tabHwnd);
    if (!t || !childHwnd) return;
    if (t->childCount + 1 > t->childCap) {
        int nc = t->childCap ? t->childCap * 2 : 8;
        // **首次分配必须走 HeapAlloc**: HeapReAlloc 要求 lpMem 是先前由
        // HeapAlloc/HeapReAlloc 返回的指针, 传 NULL 直接失败返回 NULL —— 于是
        // 子控件一个都登记不上, 切页时谁也不会显示 (表现是 Visible 恒 0,
        // 而 Left 看着正常, 极易误判成"页码反推错了")。
        Vb6TabChild* na;
        if (t->children) {
            na = (Vb6TabChild*)HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                           t->children, sizeof(Vb6TabChild) * nc);
        } else {
            na = (Vb6TabChild*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                         sizeof(Vb6TabChild) * nc);
        }
        if (!na) return;
        t->children = na;
        t->childCap = nc;
    }
    t->children[t->childCount].hwnd = childHwnd;
    t->children[t->childCount].page = (int)page;
    t->childCount++;
    sstabRefreshChildren(t);
}

// ===================== 事件: Click(PreviousTab) =====================

// 用户在窗体 WndProc 收到 TCN_SELCHANGE 后调这个: 把 Win32 的视图下标换算成逻辑页号,
// 刷新子控件可见性, 并返回**切换前**的页号 (VB6 的 Click 参数就是它)。
int32_t vb6_SSTab_OnSelChange(void* tabHwnd) {
    Vb6SSTab* t = sstabFind((HWND)tabHwnd);
    if (!t) return 0;
    int view = (int)TabCtrl_GetCurSel(t->hwnd);
    int logical = sstabViewToLogical(t, view);
    if (logical < 0) return 0;
    int prev = t->prev;
    t->prev = logical;
    t->cur = logical;
    sstabRefreshChildren(t);
    return (int32_t)prev;
}

#endif  // _WIN32
