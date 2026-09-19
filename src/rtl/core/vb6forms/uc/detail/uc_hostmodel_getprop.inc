// vb6forms_uc 拆分片段 uc_hostmodel_getprop (函数体)
// 内容 = 拆分前 vb6forms_uc.c 第 1304~1387 行，逐行未改
// 在函数体内 #include；路径一律用 basename

    vb6_VARIANT* out = (vb6_VARIANT*)outV;
    if (!obj || !name) return 0;
    vb6_ho_setVariantEmpty(out);

    if (vb6_uc_isControls(obj)) {
        if (_wcsicmp(name, L"Count") == 0) { vb6_ho_setVariantLong(out, vb6_UC_ControlsCount(obj)); return 1; }
        return 1;  // 其它成员当作无值, 避免 C 崩溃
    }
    if (vb6_uc_isColl(obj)) {   // Fix 112c: RTL Collection
        if (_wcsicmp(name, L"Count") == 0) { vb6_ho_setVariantLong(out, vb6_Collection_Count(obj)); return 1; }
        return 1;
    }
    if (vb6_uc_isFont(obj)) return vb6_ho_getFontMember(obj, name, out);

    vb6_UCRec* r = vb6_uc_findByHwnd(obj);
    vb6_HostObjRec* h = vb6_ho_find(obj);
    if (!r && !h && !IsWindow((HWND)obj)) return 0;

    int32_t isForm = r ? 0 : vb6_ho_isForm(obj);
    int32_t scaleMode = r ? (r->desc ? r->desc->scaleMode : 1) : 1;

    if (_wcsicmp(name, L"hwnd") == 0 || _wcsicmp(name, L"hWnd") == 0) {
        vb6_ho_setVariantLong(out, (int32_t)(intptr_t)obj); return 1;
    }
    if (_wcsicmp(name, L"Name") == 0) {
        vb6_ho_setVariantBstr(out, vb6_BSTR_FromStr(h && h->name[0] ? h->name : (r && r->ctrlName[0] ? r->ctrlName : L"")));
        return 1;
    }
    if (_wcsicmp(name, L"Index") == 0) {
        vb6_ho_setVariantLong(out, h ? h->index : (r ? r->index : -1)); return 1;
    }
    if (_wcsicmp(name, L"ScaleMode") == 0) { vb6_ho_setVariantLong(out, scaleMode); return 1; }
    if (_wcsicmp(name, L"ScaleWidth") == 0 || _wcsicmp(name, L"ScaleHeight") == 0 ||
        _wcsicmp(name, L"Width") == 0   || _wcsicmp(name, L"Height") == 0 ||
        _wcsicmp(name, L"Left") == 0    || _wcsicmp(name, L"Top") == 0) {
        int32_t w = 0, hh = 0;
        if (_wcsicmp(name, L"ScaleWidth") == 0 || _wcsicmp(name, L"ScaleHeight") == 0) {
            vb6_ho_clientTwips(obj, &w, &hh);
        } else if (isForm) {
            RECT rc; GetWindowRect((HWND)obj, &rc);
            w = (rc.right - rc.left) * 15; hh = (rc.bottom - rc.top) * 15;
            if (_wcsicmp(name, L"Left") == 0 || _wcsicmp(name, L"Top") == 0) {
                w = rc.left * 15; hh = rc.top * 15;
            }
        } else {
            vb6_ho_ctrlRect(obj, NULL, NULL, &w, &hh);
            if (_wcsicmp(name, L"Left") == 0 || _wcsicmp(name, L"Top") == 0) {
                int32_t l = 0, t = 0; vb6_ho_ctrlRect(obj, &l, &t, NULL, NULL);
                w = l; hh = t;
            }
        }
        int32_t val = (_wcsicmp(name, L"ScaleWidth") == 0 || _wcsicmp(name, L"Width") == 0 ||
                       _wcsicmp(name, L"Left") == 0) ? w : hh;
        // ScaleWidth/ScaleHeight 需按对象自身 ScaleMode 换算
        if (_wcsicmp(name, L"ScaleWidth") == 0 || _wcsicmp(name, L"ScaleHeight") == 0) {
            if (scaleMode == 3) val = val / 15;   // Pixel
        }
        vb6_ho_setVariantLong(out, val); return 1;
    }
    if (_wcsicmp(name, L"Visible") == 0)  { vb6_ho_setVariantLong(out, IsWindowVisible((HWND)obj) ? -1 : 0); return 1; }
    if (_wcsicmp(name, L"Enabled") == 0)  { vb6_ho_setVariantLong(out, IsWindowEnabled((HWND)obj) ? -1 : 0); return 1; }
    if (_wcsicmp(name, L"hDC") == 0)      { vb6_ho_setVariantLong(out, (int32_t)(intptr_t)GetDC((HWND)obj)); return 1; }
    if (_wcsicmp(name, L"Count") == 0)    { vb6_ho_setVariantLong(out, 0); return 1; }
    if (_wcsicmp(name, L"Controls") == 0) {
        vb6_UCControls* c = vb6_uc_newControls(obj);
        vb6_ho_setVariantDispatch(out, c); return 1;
    }
    if (_wcsicmp(name, L"Caption") == 0 || _wcsicmp(name, L"Text") == 0) {
        char buf[256]; int n = GetWindowTextA((HWND)obj, buf, sizeof(buf));
        wchar_t wbuf[256];
        for (int i = 0; i < n && i < 255; i++) wbuf[i] = (wchar_t)(unsigned char)buf[i];
        wbuf[n < 255 ? n : 255] = 0;
        vb6_ho_setVariantBstr(out, vb6_BSTR_FromStr(wbuf)); return 1;
    }
    // 字体属性 (Font / TitleFont / *Font) → 返回 Font 代理
    {
        size_t ln = wcslen(name);
        if (_wcsicmp(name, L"Font") == 0 ||
            (ln > 4 && _wcsicmp(name + ln - 4, L"Font") == 0)) {
            vb6_UCFontRec* fr = (vb6_UCFontRec*)vb6_UC_NewFont();
            vb6_ho_setVariantDispatch(out, fr); return 1;
        }
    }
    return 1;   // 宿主对象: 未知属性一律返回 Empty (不崩)
