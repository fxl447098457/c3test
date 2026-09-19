// vb6forms_uc 拆分片段 uc_debug_composite (函数体)
// 内容 = 拆分前 vb6forms_uc.c 第 371~457 行，逐行未改
// 在函数体内 #include；路径一律用 basename

// ============================================================
// Fix 123: 整窗合成 dump (调试用)
// ============================================================
// 逐个控件 dump 只能看到"控件自己画了什么", 看不到**叠放次序**(z 序) 与相对位置。
// VB6 的 z 序规则是"先声明者在上", 而 Win32 默认"后创建者在上"(见 Fix 122),
// 一旦反了就会出现"圆环被 LabelPlus 白面板盖住"这类问题而单控件 dump 一切正常。
// 这里按窗口管理器的真实 z 序 (自下而上) 把本级所有 UserControl 宿主依次绘制并
// 贴进一张窗体大小的 DIB, 输出 FORM_*.bmp —— 即在无桌面会话下也能看到"用户所见"。
static void vb6_uc_dumpFormComposite(HWND root, const char* dumpDir) {
    if (!root || !dumpDir || !*dumpDir) return;
    RECT crc;
    if (!GetClientRect(root, &crc)) return;
    int cw = (int)(crc.right - crc.left), ch = (int)(crc.bottom - crc.top);
    if (cw <= 0 || ch <= 0 || cw > 4096 || ch > 4096) return;

    HDC refDC = GetDC(root);
    vb6_UCDib form;
    vb6_uc_dibCreate(&form, refDC, cw, ch);
    if (refDC) ReleaseDC(root, refDC);
    if (!form.memDC || !form.bits) { vb6_uc_dibDestroy(&form); return; }
    RECT full = { 0, 0, cw, ch };
    HBRUSH bg = CreateSolidBrush(RGB(229, 229, 229));   // Form2 BackColor = &H00E5E5E5
    FillRect(form.memDC, &full, bg);
    DeleteObject(bg);

    // 收集本级所有 UserControl 宿主 (窗口 z 序: GW_CHILD = 最上层)
    HWND kids[VB6_UC_MAX_INST];
    int nk = 0;
    for (HWND h = GetWindow(root, GW_CHILD); h && nk < VB6_UC_MAX_INST;
         h = GetWindow(h, GW_HWNDNEXT)) {
        kids[nk++] = h;
    }
    // 自下而上绘制 (数组尾部 = 最下层)
    for (int i = nk - 1; i >= 0; i--) {
        vb6_UCRec* r = vb6_uc_findByHwnd((void*)kids[i]);
        if (!r || !r->ready || !r->desc || !r->desc->paint) continue;
        RECT kc;
        GetClientRect(kids[i], &kc);
        int kw = (int)(kc.right - kc.left), kh = (int)(kc.bottom - kc.top);
        if (kw <= 0 || kh <= 0) continue;
        POINT pt = { 0, 0 };
        MapWindowPoints(kids[i], root, &pt, 1);

        vb6_UCDib cd;
        vb6_uc_dibCreate(&cd, form.memDC, kw, kh);
        if (!cd.memDC || !cd.bits) { vb6_uc_dibDestroy(&cd); continue; }
        RECT kfull = { 0, 0, kw, kh };
        HBRUSH wb = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(cd.memDC, &kfull, wb);
        DeleteObject(wb);

        r->hdc = cd.memDC;
        vb6_UCSaved saved;
        vb6_uc_push(r, &saved);
        r->desc->paint(r->me);
        vb6_uc_pop(&saved);
        r->hdc = NULL;

        // 手动"白=透明"合并 (避免额外链接 msimg32): 与 VB6 windowless 控件一致 —
        // 控件未绘制之处露出容器底色 (例如三个圆环之间的 LabelPlus 白面板)。
        {
            unsigned char* cd8 = (unsigned char*)cd.bits;
            for (int y = 0; y < kh; y++) {
                int dy = pt.y + y;
                if (dy < 0 || dy >= form.h) continue;
                // DIB 为 bottom-up: 内存行 = h-1-y
                unsigned char* frow = (unsigned char*)form.bits
                                    + (size_t)(form.h - 1 - dy) * form.stride;
                unsigned char* crow = cd8 + (size_t)(cd.h - 1 - y) * cd.stride;
                for (int x = 0; x < kw; x++) {
                    int dx = pt.x + x;
                    if (dx < 0 || dx >= form.w) continue;
                    unsigned char b = crow[x * 3], g = crow[x * 3 + 1], rr = crow[x * 3 + 2];
                    if (rr >= 250 && g >= 250 && b >= 250) continue;   // 近白 → 透明
                    frow[dx * 3] = b; frow[dx * 3 + 1] = g; frow[dx * 3 + 2] = rr;
                }
            }
        }
        vb6_uc_dibDestroy(&cd);
    }

    char path[1024];
    _snprintf(path, sizeof(path), "%s\\FORM_%p.bmp", dumpDir, root);
    path[sizeof(path) - 1] = '\0';
    vb6_uc_dibSaveBmp(&form, path);
    vb6_uc_dibDestroy(&form);
}
