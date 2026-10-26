// uc_host_window.c - vb6forms_uc 拆分片：宿主窗口过程 + 类注册 + GDI+ 初始化
//
// 内容 = 拆分前 vb6forms_uc.c 第 459~565 / 567~607 行，纯搬移零重排无行为改动
// 跨族共享符号见 vb6forms_uc_internal.h

#include "vb6forms_uc_internal.h"
#include <stdio.h>  /* rev10: C3_UC_TRACE 的 fprintf */

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// 宿主窗口过程
// ============================================================

// ---------------------------------------------------------------------------
// 透明 UserControl (`.ctl` 里 `BackStyle = 0 'Transparent'`) 的宿主合成 —— 账 #492
//
// VB6 的 Windowless UC 直接画在容器上, 文字底下的兄弟控件内容天然可见。C3 的 UC
// 一直是真子窗口, 加上 WS_CLIPSIBLINGS 之后兄弟窗口绝不会画进本窗矩形, 于是
// BackStyle=0 的宿主必须自己把"下面那一层"补进离屏 DIB:
//   1) 先铺 ambient 底色 (容器 BackColor, 与 VB6 语义一致);
//   2) 再把 z 序在本窗**之下**、且矩形相交的兄弟窗口用 WM_PRINTCLIENT 画进来。
//      兄弟按 z 序从最底往上画 (后画者压住先画者), 与本窗的遮挡关系一致。
// 实测 (tests/Charts 2020): LabelPlus3 的 BackColorOpacity=0 只画文字, 之前宿主
// 拿 ambient 灰 (Form2.BackColor=E5E5E5) 铺满整块 ⇒ 白色卡片 LabelPlus2 被盖成灰。
// ---------------------------------------------------------------------------
static void vb6_uc_fillAmbient(HDC dc, int32_t w, int32_t h) {
    RECT full = { 0, 0, w, h };
    COLORREF cref = (vb6_Ambient_BackColor & 0x80000000L)
                        ? GetSysColor(vb6_Ambient_BackColor & 0xFF)
                        : (COLORREF)vb6_Ambient_BackColor;
    HBRUSH b = CreateSolidBrush(cref);
    FillRect(dc, &full, b);
    DeleteObject(b);
}

static void vb6_uc_composeBelow(HWND hwnd, HDC memDC, int32_t cw, int32_t ch) {
    HWND sibs[64];
    int sibCount = 0;
    POINT self = { 0, 0 };

    vb6_uc_fillAmbient(memDC, cw, ch);
    if (!ClientToScreen(hwnd, &self)) return;

    /* GW_HWNDNEXT 走"本窗之下"的同层兄弟; 收集后**倒序**绘制 (最底下的先画)。*/
    for (HWND s = GetWindow(hwnd, GW_HWNDNEXT); s && sibCount < 64;
         s = GetWindow(s, GW_HWNDNEXT)) {
        sibs[sibCount++] = s;
    }
    for (int i = sibCount - 1; i >= 0; --i) {
        HWND s = sibs[i];
        POINT sp = { 0, 0 };
        RECT src;
        if (!IsWindowVisible(s)) continue;
        if (!ClientToScreen(s, &sp)) continue;
        GetClientRect(s, &src);
        int32_t dx = sp.x - self.x, dy = sp.y - self.y;
        int32_t sw = src.right - src.left, sh = src.bottom - src.top;
        if (sw <= 0 || sh <= 0) continue;
        if (dx >= cw || dy >= ch || dx + sw <= 0 || dy + sh <= 0) continue;
        int saved = SaveDC(memDC);
        if (!saved) continue;
        /* 兄弟的客户区 (0,0) 落到本 DIB 的 (dx,dy); 裁剪也只留它自己那块。*/
        SetViewportOrgEx(memDC, dx, dy, NULL);
        HRGN clip = CreateRectRgn(0, 0, sw, sh);
        if (clip) SelectClipRgn(memDC, clip);
        SendMessageW(s, WM_PRINTCLIENT, (WPARAM)memDC, PRF_CLIENT | PRF_ERASEBKGND);
        RestoreDC(memDC, saved);
        if (clip) DeleteObject(clip);
    }
}

// 把本 UC 画进 target。composeBelow=1: target 是本窗自己的窗口 DC (WM_PAINT),
// 透明 UC 需要先把下层合成进 DIB; composeBelow=0: 正被别的窗口画进它的 DIB
// (WM_PRINTCLIENT), 下层内容已经在目标 DC 里, 透明 UC 只画自己。
static void vb6_uc_paintInto(HWND hwnd, vb6_UCRec* r, HDC target,
                             int32_t cw, int32_t ch, int composeBelow) {
    if (!r || !r->desc || !r->desc->paint || cw <= 0 || ch <= 0) return;
    const int transparent = r->desc->transparent != 0;

    vb6_UCDib dib;
    memset(&dib, 0, sizeof(dib));
    vb6_uc_dibCreate(&dib, target, cw, ch);
    const int useDib = (dib.memDC && dib.bits);
    HDC dc = useDib ? dib.memDC : target;

    if (transparent && composeBelow) {
        if (useDib) vb6_uc_composeBelow(hwnd, dc, cw, ch);
    } else if (!transparent) {
        vb6_uc_fillAmbient(dc, cw, ch);
    }
    /* 透明 + 非合成 (被别人 WM_PRINTCLIENT): 一个像素都不填 —— 目标 DC 里已经是
       合成好的下层, 再填 ambient 会把兄弟内容盖掉。*/

    r->hdc = dc;
    vb6_UCSaved saved;
    vb6_uc_push(r, &saved);
    vb6_uc_trace("paint.begin", r->desc->typeName, r->me);
    r->desc->paint(r->me);
    vb6_uc_trace("paint.end", r->desc->typeName, r->me);
    vb6_uc_pop(&saved);

    if (useDib) {
        BitBlt(target, 0, 0, dib.w, dib.h, dib.memDC, 0, 0, SRCCOPY);
        // Fix 113h/123: 调试捕获 (C3_UC_DUMPDIR) —— 只在真实 WM_PAINT 路径做,
        // WM_PRINTCLIENT 是被兄弟合成调用的, 重复 dump 会淹没序号。
        const char* dumpDir113h = composeBelow ? getenv("C3_UC_DUMPDIR") : NULL;
        if (dumpDir113h && *dumpDir113h) {
            char path113h[1024];
            _snprintf(path113h, sizeof(path113h), "%s\\%s_%d_%p.bmp", dumpDir113h,
                      r->desc->typeName, (int)++vb6_ucDumpSeq, hwnd);
            path113h[sizeof(path113h) - 1] = '\0';
            vb6_uc_dibSaveBmp(&dib, path113h);
            // Fix 123: 同步输出整窗合成图 (含 z 序/相对位置)
            vb6_uc_dumpFormComposite((HWND)GetAncestor(hwnd, GA_ROOT), dumpDir113h);
        }
        vb6_uc_dibDestroy(&dib);
    }
    r->hdc = NULL;
}

static LRESULT CALLBACK vb6_uc_wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    vb6_UCRec* r = (vb6_UCRec*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg) {
        case WM_NCCREATE: {
            CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
            break;
        }
        case WM_SIZE: {
            if (!r || !r->ready) break;
            r->scaleWidth = (int32_t)LOWORD(lParam);
            r->scaleHeight = (int32_t)HIWORD(lParam);
            if (r->desc && r->desc->resize) {
                vb6_UCSaved saved;
                vb6_uc_push(r, &saved);
                vb6_uc_trace("resize.begin", r->desc->typeName, r->me);
                r->desc->resize(r->me);
                vb6_uc_trace("resize.end", r->desc->typeName, r->me);
                vb6_uc_pop(&saved);
            }
            /* Fix <vbeclipse> rev22: 本 UC 的尺寸变了 ⇒ 它每个**设计期子控件**的
             * 尺寸也快变了 ⇒ 该跑它们的 `<Ctrl>_Resize` 事件 (VB6 语义)。
             *
             * 为什么挂在这里而不是 vb6_ControlMove (那是所有摆位的收口):
             *   放在 Move 里会**递归**。`ViewArea.Move …` 正是 ucFolder 的
             *   `UserControl_Resize` 内部发的, 而那个过程就是经
             *   WM_SIZE → vb6_uc_push → resize → Move 这条链进来的 ⇒ Move 里再跑
             *   子控件事件 ⇒ 又 Move ⇒ 无限递归。
             *   实测 play78 (rev22 首次实现): 视图窗体尺寸确实对上了容器
             *   (frmViewHelp 253x219 / frmViewSnapshot 778x190), 但**整个窗口
             *   上下颠倒 + 文字镜像** —— 递归 Move 把各层控件反复推挤, 坐标系翻转。
             *   两害相权: 宁可尺寸停在中间值 (rev21 状态) 也不能递归。
             *
             * 为什么放在 pop **之后** 是安全的: 此时 vb6_ucCurrent 已还原成外层值
             * (通常 NULL), 所以 vb6_UC_RunDesignResize 里 "上下文内就不跑" 的
             * 判据不会误挡本调用 —— 它是唯一允许在上下文外触发的入口。
             * 且它内部有同控件重入短路, 事件体里 Move 别的控件再发 WM_SIZE 也不会
             * 无限展开 (那一层 vb6_ucCurrent 非空, 直接被挡)。*/
            /* Fix <vbeclipse> rev23: 这里**只排队**, 不直接跑 —— WM_SIZE 是
             * SetWindowPos/MoveWindow 的**同步** SendMessage, 一整串嵌套 Move 全在
             * 同一个调用栈里跑完才返回, 那时 ucFolder 的 ViewArea 还没拿到最终尺寸
             * ⇒ `ViewArea_Resize` 按设计期宽度去 Move 视图窗体 (实测恒 W=8505)。
             * 排到消息循环里跑, 各控件尺寸才是最终值。详见 vb6_UC_QueueDesignResize。*/
            vb6_UC_QueueDesignResize(r->me);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        /* Fix <vbeclipse> rev23: 排队的子控件 Resize 事件在这里接住。
         * 必须排在消息循环里 (不能直接在 WM_SIZE 里跑) —— WM_SIZE 是
         * SetWindowPos/MoveWindow 的**同步** SendMessage, 一整串嵌套 Move 全在同
         * 一个调用栈里跑完才返回, 那时 ucFolder 的 ViewArea 还没拿到最终尺寸。*/
        case VB6_UC_DR_MSG:
            vb6_UC_DrainDesignResize(hwnd);
            return 0;
        case WM_SHOWWINDOW:
            if (r && r->ready && wParam && r->desc && r->desc->show) {
                vb6_UCSaved saved;
                vb6_uc_push(r, &saved);
                vb6_uc_trace("show.begin", r->desc->typeName, r->me);
                r->desc->show(r->me);
                vb6_uc_trace("show.end", r->desc->typeName, r->me);
                vb6_uc_pop(&saved);
            }
            return 0;
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MOUSEMOVE: {
            // czUI fix: 把宿主窗口收到的鼠标消息转成 UserControl_MouseDown/Up/Move
            // (此前完全没有转发, 控件无法点击/拖动 — czUI 标题栏拖动、按钮、开关全死)。
            if (!r || !r->ready || !r->desc || !r->me) break;
            void (*hook)(void*, int32_t, int32_t, float, float) = NULL;
            int32_t button = 0;
            switch (msg) {
                case WM_LBUTTONDOWN: hook = r->desc->mouseDown;  button = 1; break;
                case WM_RBUTTONDOWN: hook = r->desc->mouseDown;  button = 2; break;
                case WM_LBUTTONUP:   hook = r->desc->mouseUp;    button = 1; break;
                case WM_RBUTTONUP:   hook = r->desc->mouseUp;    button = 2; break;
                case WM_MOUSEMOVE:   hook = r->desc->mouseMove;  button = 0; break;
            }
            if (!hook) break;
            // 捕获鼠标, 保证按下后拖出窗口仍能收到 UP/MOVE (VB6 隐式行为)
            if (msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN) SetCapture(hwnd);
            else if (msg == WM_LBUTTONUP || msg == WM_RBUTTONUP) ReleaseCapture();
            float sx = (float)(short)LOWORD(lParam);
            float sy = (float)(short)HIWORD(lParam);
            // 坐标换算到控件当前 ScaleMode 单位 (账 #175: 与 ScaleWidth/Move 同一个权威)
            int32_t ucSm175 = r->desc->scaleMode;
            sx = (float)vb6_ScalePxToUser((double)sx, ucSm175, 0);
            sy = (float)vb6_ScalePxToUser((double)sy, ucSm175, 1);
            vb6_UCSaved saved;
            vb6_uc_push(r, &saved);
            // czUI fix: 回调只更新状态; 视觉刷新统一走 WM_PAINT 双缓冲
            // (直接 GetDC 画屏幕与擦除/重绘交错会造成闪烁)
            vb6_UserControl_hDC = NULL;
            hook(r->me, button, 0, sx, sy);
            vb6_uc_pop(&saved);
            // 账 #226: VB6 的 Click 是"按下并抬起"之后发的, 落点就在这条 WM_LBUTTONUP;
            // 必须在 MouseUp 转调**之后**, 顺序才与 VB6 一致。旧 cgen 不发这一槽 ⇒
            // 初始化式补 0 ⇒ 这里不转调, 行为与改动前逐字节一致。
            if (msg == WM_LBUTTONUP && r->desc->click) r->desc->click(r->me);
            InvalidateRect(hwnd, NULL, FALSE);
            UpdateWindow(hwnd);
            return 0;
        }
        case WM_LBUTTONDBLCLK: {
            // 账 #227: 这一槽 cgen 一直在填 (UC 里写 UserControl_DblClick 才有真身, 否则是空
            // stub), 但宿主从没有转调过它 —— 语料里六枚 UC 的 `RaiseEvent DblClick` 于是永远
            // 发不出去。类样式上 CS_DBLCLKS 早就立了 (下面那句 czUI fix), 消息收得到,
            // 缺的就是这一跳。
            // MouseDown/MouseUp 的转调**不**挂在这条消息上: 物理双击 Windows 发的是
            // DOWN/UP/DBLCLK/UP, Click 那一路已经由两条 UP 供过, 再挂就变三发。
            if (!r || !r->ready || !r->desc || !r->me || !r->desc->dblClick) break;
            vb6_UCSaved saved227;
            vb6_uc_push(r, &saved227);
            vb6_UserControl_hDC = NULL;
            r->desc->dblClick(r->me);
            vb6_uc_pop(&saved227);
            InvalidateRect(hwnd, NULL, FALSE);
            UpdateWindow(hwnd);
            return 0;
        }

        case WM_CAPTURECHANGED:
            break;
        case WM_PAINT: {
            if (!r || !r->ready) break;
            if (!r->desc || !r->desc->paint) break;
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT crc;
            GetClientRect(hwnd, &crc);
            vb6_uc_paintInto(hwnd, r, hdc,
                             (int32_t)(crc.right - crc.left),
                             (int32_t)(crc.bottom - crc.top), 1);
            EndPaint(hwnd, &ps);
            return 0;
        }
        /* 账 #492: 被父窗口/兄弟窗口的 PRF_CHILDREN 合成时走这里。目标 DC 里
           (对透明 UC 来说) 已经铺好了下面那一层, 这里只把本控件自己画上去。*/
        case WM_PRINTCLIENT: {
            if (!r || !r->ready || !r->desc || !r->desc->paint) break;
            RECT crc;
            GetClientRect(hwnd, &crc);
            vb6_uc_paintInto(hwnd, r, (HDC)wParam,
                             (int32_t)(crc.right - crc.left),
                             (int32_t)(crc.bottom - crc.top), 0);
            return 0;
        }
        case WM_ERASEBKGND:
            if (r && r->ready) {
                /* 账 #492: 透明 UC 不擦背景 —— 一擦就是一块 ambient 灰, WM_PAINT
                   里的合成会覆盖它, 中间那一帧在屏幕上就是灰闪。*/
                if (!r->desc || !r->desc->transparent) {
                    // 用容器底色填充, 避免重绘闪烁 (固定白色会在深色 UI 上闪白)
                    RECT rc; GetClientRect(hwnd, &rc);
                    COLORREF cref = (vb6_Ambient_BackColor & 0x80000000L)
                                        ? GetSysColor(vb6_Ambient_BackColor & 0xFF)
                                        : (COLORREF)vb6_Ambient_BackColor;
                    HBRUSH b = CreateSolidBrush(cref);
                    FillRect((HDC)wParam, &rc, b);
                    DeleteObject(b);
                }
            }
            return 1;
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC: {
            // czUI fix: 设计器子控件 (txtEmbed 等) 的 BackColor/ForeColor 落在
            // VB6_BackColor/ForeColor 窗口属性上 (vb6_SetControlBackColor), 这里
            // 应用之 — 否则深色 UI 上子编辑框是默认白底黑字。
            HWND child = (HWND)lParam;
            if (!child) break;
            HDC hdc = (HDC)wParam;
            COLORREF fg = (COLORREF)vb6_GetControlForeColor((void*)child);
            COLORREF bg = (COLORREF)vb6_GetControlBackColor((void*)child);
            if (bg & 0x80000000L) bg = GetSysColor(bg & 0xFF);
            SetTextColor(hdc, fg);
            SetBkColor(hdc, bg);
            // 刷子缓存进窗口属性, 避免每条消息泄漏 GDI 句柄
            HBRUSH br = (HBRUSH)GetPropW(child, L"VB6_BgBrush");
            if (!br) {
                br = CreateSolidBrush(bg);
                SetPropW(child, L"VB6_BgBrush", (HANDLE)br);
            }
            return (LRESULT)br;
        }
        case WM_DESTROY:
            // Fix <vbeclipse> rev10 (C3_UC_TRACE): 临时 —— 定位
            // "ShowPerspective 报 No perspective id found" 是否因为本控件的
            // WM_DESTROY 提前跑过 UserControl_Terminate (源码里它会
            // `Set m_Perspectives = Nothing`)。
            if (getenv("C3_UC_TRACE")) {
                fprintf(stderr, "[UC] WM_DESTROY hwnd=%p me=%p hasTerminate=%d\n",
                        (void*)hwnd, r ? r->me : NULL,
                        (r && r->desc && r->desc->terminate) ? 1 : 0);
                fflush(stderr);
            }
            if (r && r->desc && r->desc->terminate) {
                vb6_UCSaved saved;
                vb6_uc_push(r, &saved);
                r->desc->terminate(r->me);
                vb6_uc_pop(&saved);
            }
            break;
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// Fix 112b: GDI+ 进程级初始化. 图表控件的绘制走 GdipCreateFromHDC, 而
// Charts 2020 自带的 ManageGDIToken 在打完补丁后会 GdiplusShutdown 掉它自己
// 启动的 token —— RTL 这里保一个常驻 token, 保证绘制始终可用.
// gdiplus.h 是 C++ 专用头, 这里按桩文件的做法用 LoadLibrary + GetProcAddress.
typedef struct vb6_GdiplusStartupInput {
    uint32_t GdiplusVersion;
    void*    DebugEventCallback;
    int32_t  SuppressBackgroundThread;
    int32_t  SuppressExternalCodecs;
} vb6_GdiplusStartupInput;

void vb6_uc_gdiplusInit(void) {
    static int done = 0;
    if (done) return;
    done = 1;
    // czUI fix: 环境字体默认名 — Bag 重放 ReadProperty("Font", Ambient.Font)
    // 会把此对象设为控件字体; Name=NULL 时所有 GDI+ 文字静默消失
    if (!vb6_UserControl_FontObj.Name)
        vb6_UserControl_FontObj.Name = SysAllocString(L"Segoe UI");
    HMODULE mod = LoadLibraryA("gdiplus.dll");
    if (!mod) return;
    long (__stdcall *pStartup)(ULONG_PTR*, const void*, void*) =
        (long (__stdcall *)(ULONG_PTR*, const void*, void*))GetProcAddress(mod, "GdiplusStartup");
    if (!pStartup) return;
    vb6_GdiplusStartupInput si;
    memset(&si, 0, sizeof(si));
    si.GdiplusVersion = 1;
    ULONG_PTR token = 0;
    pStartup(&token, &si, NULL);   /* 常驻 token, 不配对 Shutdown */
}

void vb6_uc_registerClass(HINSTANCE hInst) {
    static int done = 0;
    if (done) return;
    WNDCLASSW wc;
    memset(&wc, 0, sizeof(wc));
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;  // czUI fix: UserControl_DblClick 需要
    wc.lpfnWndProc = vb6_uc_wndproc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"VB6_UserControlHost";
    RegisterClassW(&wc);
    done = 1;
}

#ifdef __cplusplus
} // extern "C"
#endif
