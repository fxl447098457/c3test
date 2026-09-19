// uc_host_window.c - vb6forms_uc 拆分片：宿主窗口过程 + 类注册 + GDI+ 初始化
//
// 内容 = 拆分前 vb6forms_uc.c 第 459~565 / 567~607 行，纯搬移零重排无行为改动
// 跨族共享符号见 vb6forms_uc_internal.h

#include "vb6forms_uc_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// 宿主窗口过程
// ============================================================

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
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
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
        case WM_PAINT: {
            if (!r || !r->ready) break;
            if (!r->desc || !r->desc->paint) break;
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            // Fix 113h: 调试捕获 (见上方 vb6_UCDib 说明) — C3_UC_DUMPDIR 未设置时
            // 走原路径 (直接绘制到窗口 DC).
            const char* dumpDir113h = getenv("C3_UC_DUMPDIR");
            vb6_UCDib dib113h;
            int useDib113h = 0;
            if (dumpDir113h && *dumpDir113h) {
                RECT crc;
                GetClientRect(hwnd, &crc);
                int cw113h = (int)(crc.right - crc.left);
                int ch113h = (int)(crc.bottom - crc.top);
                if (cw113h > 0 && ch113h > 0) {
                    vb6_uc_dibCreate(&dib113h, hdc, cw113h, ch113h);
                    if (dib113h.memDC && dib113h.bits) {
                        RECT full113h = { 0, 0, cw113h, ch113h };
                        HBRUSH wb113h = CreateSolidBrush(RGB(255, 255, 255));
                        FillRect(dib113h.memDC, &full113h, wb113h);
                        DeleteObject(wb113h);
                        useDib113h = 1;
                    }
                }
            }
            r->hdc = useDib113h ? dib113h.memDC : hdc;
            vb6_UCSaved saved;
            vb6_uc_push(r, &saved);
            vb6_uc_trace("paint.begin", r->desc->typeName, r->me);
            r->desc->paint(r->me);
            vb6_uc_trace("paint.end", r->desc->typeName, r->me);
            vb6_uc_pop(&saved);
            if (useDib113h) {
                BitBlt(hdc, 0, 0, dib113h.w, dib113h.h, dib113h.memDC, 0, 0, SRCCOPY);
                char path113h[1024];
                _snprintf(path113h, sizeof(path113h), "%s\\%s_%d_%p.bmp", dumpDir113h,
                          r->desc->typeName, (int)++g_uc_dumpSeq, hwnd);
                path113h[sizeof(path113h) - 1] = '\0';
                vb6_uc_dibSaveBmp(&dib113h, path113h);
                vb6_uc_dibDestroy(&dib113h);
                // Fix 123: 同步输出整窗合成图 (含 z 序/相对位置)
                vb6_uc_dumpFormComposite((HWND)GetAncestor(hwnd, GA_ROOT), dumpDir113h);
            }
            r->hdc = NULL;
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            if (r && r->ready) {
                // 用控件 BackColor 填充, 避免闪烁/黑底
                RECT rc; GetClientRect(hwnd, &rc);
                HBRUSH b = CreateSolidBrush(RGB(255, 255, 255));
                FillRect((HDC)wParam, &rc, b);
                DeleteObject(b);
            }
            return 1;
        case WM_DESTROY:
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
    wc.style = CS_HREDRAW | CS_VREDRAW;
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
