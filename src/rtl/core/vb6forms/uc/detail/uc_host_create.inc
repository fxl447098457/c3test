// vb6forms_uc 拆分片段 uc_host_create (函数体)
// 内容 = 拆分前 vb6forms_uc.c 第 634~738 行，逐行未改
// 在函数体内 #include；路径一律用 basename

    vb6_uc_trace("create.begin", typeName, NULL);
    const vb6_UserControlDesc* desc = vb6_uc_findDesc(typeName);
    if (!desc) return NULL;
    if (g_uc_recCount >= VB6_UC_MAX_INST) return NULL;

    vb6_uc_registerClass((HINSTANCE)hInstance);
    vb6_uc_gdiplusInit();

    vb6_UCRec* r = &g_uc_recs[g_uc_recCount];
    memset(r, 0, sizeof(*r));
    r->desc = desc;
    r->parent = (HWND)hParent;
    r->enabled = -1;
    r->index = index;
    // Fix 119: 装入 cgen 预先设定的实例字体 (.frm BeginProperty Font)
    if (g_uc_pendingFont) {
        r->font = g_uc_pendingFont;
        g_uc_pendingFont = NULL;
    }
    r->scaleWidth = vb6_TwipToX(width);
    r->scaleHeight = vb6_TwipToY(height);
    r->extLeft = left;
    r->extTop = top;
    if (ctrlName) {
        size_t n = strlen(ctrlName);
        if (n >= VB6_UC_NAME_LEN) n = VB6_UC_NAME_LEN - 1;
        for (size_t i = 0; i < n; i++) r->ctrlName[i] = (wchar_t)ctrlName[i];
        r->ctrlName[n] = 0;
    }

    HWND hwnd = CreateWindowExW(0, L"VB6_UserControlHost", L"",
                                // Fix 124: WS_CLIPSIBLINGS 必不可少 —— 没有它, 一个窗口
                                // 重绘时会画到同层兄弟控件上, 于是 z 序在下的不透明面板
                                // (Charts 2020 的 LabelPlus1 白底) 每次重绘都会把上层控件
                                // (三个 ucProgressCircular 圆环) 擦掉。离屏 dump 是各控件
                                // 单独画进自己的 DIB 再合成, 体现不出这个裁剪问题 ——
                                // 这就是"dump 里有、软件里看不到"的根因。
                                WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                                vb6_TwipToX(left), vb6_TwipToY(top),
                                vb6_TwipToX(width), vb6_TwipToY(height),
                                (HWND)hParent, NULL, (HINSTANCE)hInstance, r);
    vb6_uc_trace("window.created", typeName, hwnd);
    if (!hwnd) return NULL;
    r->hwnd = hwnd;
    g_uc_recCount++;

    // Fix 122: 修正 VB6 z 序 (先声明者在上)
    vb6_uc_fixZOrder(hwnd);

    // 登记: 供 Controls 枚举 / TypeName / 宿主对象分派
    if (g_hoCount < VB6_UC_MAX_OBJ) {
        vb6_HostObjRec* h = &g_ho[g_hoCount++];
        memset(h, 0, sizeof(*h));
        h->hwnd = (void*)hwnd;
        h->isForm = 0;
        h->index = index;
        h->instance = r->me;
        for (size_t i = 0; r->ctrlName[i] && i < VB6_UC_NAME_LEN - 1; i++) h->name[i] = r->ctrlName[i];
        {
            const char* p = desc->typeName;
            const char* dot = strrchr(desc->typeName, '.');
            if (dot) p = dot + 1;
            size_t n = strlen(p);
            if (n >= VB6_UC_NAME_LEN) n = VB6_UC_NAME_LEN - 1;
            for (size_t i = 0; i < n; i++) h->typeName[i] = (wchar_t)(unsigned char)p[i];
            h->typeName[n] = 0;
        }
    }

    // 实例化 + 初始化 (换入宿主状态, 让 UserControl_Initialize 读到正确的 ScaleMode)
    {
        vb6_UCSaved saved;
        vb6_uc_push(r, &saved);
        r->me = desc->create ? desc->create() : NULL;
        if (r->me && desc->init) desc->init(r->me);
        r->ready = 1;   // Fix 112: 之后窗口消息 (Resize/Show/Paint) 才允许进入实例
        vb6_uc_trace("instance.ready", desc->typeName, r->me);
        vb6_uc_pop(&saved);
        if (g_hoCount) g_ho[g_hoCount - 1].instance = r->me;
    }

    ShowWindow(hwnd, SW_SHOW);
    // Fix 112: 窗口创建期间被 ready=0 拦下的 Resize/Show 在此补发
    if (desc->resize) {
        RECT rc; GetClientRect(hwnd, &rc);
        r->scaleWidth = rc.right;
        r->scaleHeight = rc.bottom;
        vb6_UCSaved saved;
        vb6_uc_push(r, &saved);
        vb6_uc_trace("resize.begin", desc->typeName, r->me);
        desc->resize(r->me);
        vb6_uc_trace("resize.end", desc->typeName, r->me);
        vb6_uc_pop(&saved);
    }
    if (desc->show) {
        vb6_UCSaved saved;
        vb6_uc_push(r, &saved);
        vb6_uc_trace("show.begin", desc->typeName, r->me);
        desc->show(r->me);
        vb6_uc_trace("show.end", desc->typeName, r->me);
        vb6_uc_pop(&saved);
    }
    vb6_uc_trace("create.end", typeName, r->me);
    InvalidateRect(hwnd, NULL, FALSE);
    return (void*)hwnd;
