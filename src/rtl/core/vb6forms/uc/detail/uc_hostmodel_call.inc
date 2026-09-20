// vb6forms_uc 拆分片段 uc_hostmodel_call (函数体)
// 内容 = 拆分前 vb6forms_uc.c 第 1424~1493 行，逐行未改
// 在函数体内 #include；路径一律用 basename

    obj = vb6_UC_UnwrapHost(obj);  // czUI fix: 包装器透明解包
    vb6_VARIANT* out = (vb6_VARIANT*)outV;
    (void)argc;
    if (!obj || !name) return 0;
    vb6_ho_setVariantEmpty(out);
    if (vb6_uc_isFont(obj)) return 1;
    if (vb6_uc_isColl(obj)) {   // Fix 112c: Collection 方法
        if (_wcsicmp(name, L"Add") == 0 && argc >= 1) {
            BSTR k = (argc >= 2) ? vb6_ho_variantToBstr((vb6_VARIANT*)argv[1]) : NULL;
            /* Fix 134: VB6 Add(Item[, Key, Before, After]) 的位置插入 */
            if (argc >= 3 && argv[2]) {
                vb6_VARIANT* b = (vb6_VARIANT*)argv[2];
                if (b->vt != vb6_vtEmpty) {
                    int32_t pos = vb6_ho_variantToLong(b);
                    if (k) { vb6_Collection_AddKeyedAt(obj, argv[0], k, pos); }
                    else   { vb6_Collection_AddAt(obj, argv[0], pos); }
                    return 1;
                }
            }
            if (argc >= 4 && argv[3]) {
                vb6_VARIANT* a = (vb6_VARIANT*)argv[3];
                if (a->vt != vb6_vtEmpty) {
                    int32_t pos = vb6_ho_variantToLong(a) + 1;
                    if (k) { vb6_Collection_AddKeyedAt(obj, argv[0], k, pos); }
                    else   { vb6_Collection_AddAt(obj, argv[0], pos); }
                    return 1;
                }
            }
            if (k) vb6_Collection_AddKeyed(obj, argv[0], k);
            else   vb6_Collection_Add(obj, argv[0]);
        } else if (_wcsicmp(name, L"Item") == 0 && argc >= 1) {
            vb6_VARIANT* a = (vb6_VARIANT*)argv[0];
            BSTR k = vb6_ho_variantToBstr(a);
            if (k) vb6_Collection_ItemByKey(obj, k, out);
            else   vb6_Collection_Item(obj, vb6_ho_variantToLong(a), out);
        } else if (_wcsicmp(name, L"Remove") == 0 && argc >= 1) {
            vb6_VARIANT* a = (vb6_VARIANT*)argv[0];
            BSTR k = vb6_ho_variantToBstr(a);
            if (k) vb6_Collection_RemoveByKey(obj, k);
            else   vb6_Collection_Remove(obj, vb6_ho_variantToLong(a));
        }
        return 1;
    }
    if (vb6_uc_isControls(obj)) {
        if (_wcsicmp(name, L"Item") == 0 && argc >= 1) {
            vb6_VARIANT* a = (vb6_VARIANT*)argv[0];
            void* it = vb6_UC_ControlsItem(obj, vb6_ho_variantToLong(a));
            vb6_ho_setVariantDispatch(out, it);
        }
        return 1;
    }
    if (!vb6_ho_find(obj) && !vb6_uc_findByHwnd(obj) && !IsWindow((HWND)obj)) return 0;

    if (_wcsicmp(name, L"Refresh") == 0) {
        InvalidateRect((HWND)obj, NULL, TRUE); UpdateWindow((HWND)obj);
        return 1;
    }
    if (_wcsicmp(name, L"Cls") == 0) {
        HDC dc = GetDC((HWND)obj);
        if (dc) { RECT rc; GetClientRect((HWND)obj, &rc);
                  HBRUSH b = CreateSolidBrush(RGB(255,255,255));
                  FillRect(dc, &rc, b); DeleteObject(b); ReleaseDC((HWND)obj, dc); }
        return 1;
    }
    if (_wcsicmp(name, L"SetFocus") == 0) { SetFocus((HWND)obj); return 1; }
    if (_wcsicmp(name, L"ZOrder") == 0 || _wcsicmp(name, L"Move") == 0 ||
        _wcsicmp(name, L"Show") == 0 || _wcsicmp(name, L"Hide") == 0 ||
        _wcsicmp(name, L"Print") == 0 || _wcsicmp(name, L"Line") == 0) {
        return 1;
    }
    return 1;   // 宿主对象: 未知方法一律空实现
