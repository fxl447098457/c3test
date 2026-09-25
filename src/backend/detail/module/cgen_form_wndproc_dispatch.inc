// cgen_form_wndproc_dispatch.inc - emitFormFramework 函数体片段：WndProc 后半 —— 焦点 / 滚动 / 键盘 / 关闭 / 清理 / default
// 由 src/backend/module/cgen_form.cpp 拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 922~1272 行，逐行未改。

    // P18-B: GotFocus/LostFocus events (EN_SETFOCUS/EN_KILLFOCUS/CBN_SETFOCUS/CBN_KILLFOCUS)
    {
        ctrlId = 100;
        for (const auto& ctrl : frmDesc.formControl.children) {
            std::string gotFn = cProcName(ctrl.controlName + "_GotFocus", AccessLevel::Private);
            std::string lostFn = cProcName(ctrl.controlName + "_LostFocus", AccessLevel::Private);
            bool hasGot = symTab_.lookup(ctrl.controlName + "_GotFocus") != nullptr;
            bool hasLost = symTab_.lookup(ctrl.controlName + "_LostFocus") != nullptr;
            bool hasValidate = symTab_.lookup(ctrl.controlName + "_Validate") != nullptr;
            if (hasGot || hasLost) {
                if (ctrl.controlType == FrmControlType::TextBox) {
                    // EN_SETFOCUS=256, EN_KILLFOCUS=512
                    if (hasGot) c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 256) { extern void " + gotFn + "(); " + gotFn + "(); }");
                    if (hasLost) { if (hasValidate) c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 512 && !GetPropW((HWND)lParam, L\"VB6_ValidateCancel\")) { extern void " + lostFn + "(); " + lostFn + "(); }"); else c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 512) { extern void " + lostFn + "(); " + lostFn + "(); }"); }
                } else if (ctrl.controlType == FrmControlType::ComboBox) {
                    // CBN_SETFOCUS=1024, CBN_KILLFOCUS=2048
                    if (hasGot) c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 1024) { extern void " + gotFn + "(); " + gotFn + "(); }");
                    if (hasLost) { if (hasValidate) c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 2048 && !GetPropW((HWND)lParam, L\"VB6_ValidateCancel\")) { extern void " + lostFn + "(); " + lostFn + "(); }"); else c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 2048) { extern void " + lostFn + "(); " + lostFn + "(); }"); }
                } else if (ctrl.controlType == FrmControlType::ListBox) {
                    // LBN_SETFOCUS=4, LBN_KILLFOCUS=5
                    if (hasGot) c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 4) { extern void " + gotFn + "(); " + gotFn + "(); }");
                    if (hasLost) { if (hasValidate) c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 5 && !GetPropW((HWND)lParam, L\"VB6_ValidateCancel\")) { extern void " + lostFn + "(); " + lostFn + "(); }"); else c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 5) { extern void " + lostFn + "(); " + lostFn + "(); }"); }
                } else if (ctrl.controlType == FrmControlType::CommandButton ||
                           ctrl.controlType == FrmControlType::CheckBox ||
                           ctrl.controlType == FrmControlType::OptionButton) {
                    // BN_SETFOCUS=6, BN_KILLFOCUS=7 (button class notifications via WM_COMMAND)
                    if (hasGot) c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 6) { extern void " + gotFn + "(); " + gotFn + "(); }");
                    if (hasLost) { if (hasValidate) c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 7 && !GetPropW((HWND)lParam, L\"VB6_ValidateCancel\")) { extern void " + lostFn + "(); " + lostFn + "(); }"); else c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 7) { extern void " + lostFn + "(); " + lostFn + "(); }"); }
                }
                // Label: no GotFocus/LostFocus in VB6 (windowless control)
                // PictureBox: GotFocus/LostFocus handled via subclassing (P18-F)
            }
            ctrlId++;
        }
    }
    c_.emitLine("break;");
    c_.dedent();
    c_.emitLine("}");


    // WM_CLOSE: 调用Form_Unload判断是否允许关闭
    c_.emitLine("case WM_CLOSE: {");
    c_.indent();
    {
        std::string formUnloadFn = cProcName("Form_Unload", AccessLevel::Private);
        auto* formUnloadSym2 = symTab_.lookup("Form_Unload");
        if (formUnloadSym2) {
            // 生成trampoline: 桥接 int(*)(void) 回调和 void(int16_t*) VB6签名
            std::string trampoline = formUnloadFn + "_trampoline";
            // P18-B: Form_QueryUnload - check if close is allowed before Unload
            {
                std::string queryUnloadFn = cProcName("Form_QueryUnload", AccessLevel::Private);
                auto* quSym = symTab_.lookup("Form_QueryUnload");
                if (quSym) {
                    c_.emitLine("{ int16_t vb6_cancel = 0; int16_t vb6_unloadmode = 0; /* 0=vbFormControlMenu */");
                    c_.emitLine("extern void " + queryUnloadFn + "(int16_t*, int16_t*); " + queryUnloadFn + "(&vb6_cancel, &vb6_unloadmode);");
                    c_.emitLine("if (vb6_cancel != 0) return 0; /* Cancel close */ }");
                }
            }
            c_.emitLine("vb6_SetFormUnloadCallback((void*)" + trampoline + ");");
            c_.emitLine("if (vb6_QueryFormUnload() == 0) {");
            c_.indent();
            c_.emitLine("DestroyWindow(hwnd);");
            c_.dedent();
            c_.emitLine("}");
        } else {
            c_.emitLine("DestroyWindow(hwnd);");
        }
    }
    c_.emitLine("break;");
    c_.dedent();
    c_.emitLine("}");
    // WM_DESTROY: PostQuitMessage (如果是主窗体)
    c_.emitLine("case WM_DESTROY: {");
    c_.indent();
    c_.emitLine("vb6_Forms_Unregister((void*)hwnd);");
    // P18-F: 移除所有控件子类化
    {
        auto needsSubclass = [&](const FrmControl& ctrl) -> bool {
            return (ctrl.controlType == FrmControlType::PictureBox ||
                    ctrl.controlType == FrmControlType::Frame ||
                    ctrl.controlType == FrmControlType::Label ||
                    ctrl.controlType == FrmControlType::Image)
                   && (symTab_.lookup(ctrl.controlName + "_GotFocus") ||
                       symTab_.lookup(ctrl.controlName + "_LostFocus"))
                || symTab_.lookup(ctrl.controlName + "_MouseEnter")
                || symTab_.lookup(ctrl.controlName + "_MouseLeave")
                || symTab_.lookup(ctrl.controlName + "_MouseDown")
                || symTab_.lookup(ctrl.controlName + "_MouseUp")
                || symTab_.lookup(ctrl.controlName + "_MouseMove")
                || symTab_.lookup(ctrl.controlName + "_KeyPress")
                || symTab_.lookup(ctrl.controlName + "_KeyDown")
                || symTab_.lookup(ctrl.controlName + "_KeyUp")
              || symTab_.lookup(ctrl.controlName + "_Validate")
                || symTab_.lookup(ctrl.controlName + "_MouseHover");
        };
        std::unordered_set<std::string> subEmitted;
        for (const auto& ctrl : frmDesc.formControl.children) {
            std::string ctrlNameLower = ctrl.controlName;
            std::transform(ctrlNameLower.begin(), ctrlNameLower.end(), ctrlNameLower.begin(), ::tolower);
            if (subEmitted.count(ctrlNameLower)) continue;
            subEmitted.insert(ctrlNameLower);
            if (!needsSubclass(ctrl)) continue;
            if (knownControlArrays_.count(ctrlNameLower)) {
                c_.emitLine("{ int vb6_si; for (vb6_si = 0; vb6_si < VB6_CTRLARR_MAX; vb6_si++) {");
                c_.emitLine("  void* vb6_sub_hwnd = vb6_CtrlArr_GetAt(&vb6_arr_" + cIdent(ctrl.controlName) + ", vb6_si);");
                c_.emitLine("  if (vb6_sub_hwnd) vb6_RemoveControlSubclass(vb6_sub_hwnd); } }");
            } else {
                c_.emitLine("vb6_RemoveControlSubclass((void*)vb6_hwnd_" + cIdent(ctrl.controlName) + ");");
            }
        }
        // P0-5修复: Frame容器内子控件
        for (const auto& ctrl : frmDesc.formControl.children) {
            if (ctrl.controlType != FrmControlType::Frame) continue;
            for (const auto& child : ctrl.children) {
                std::string childNameLower = child.controlName;
                std::transform(childNameLower.begin(), childNameLower.end(), childNameLower.begin(), ::tolower);
                if (subEmitted.count(childNameLower)) continue;
                subEmitted.insert(childNameLower);
                if (!needsSubclass(child)) continue;
                c_.emitLine("vb6_RemoveControlSubclass((void*)vb6_hwnd_" + cIdent(child.controlName) + ");");
            }
        }
    }
    // M12-FIX: Cleanup Form.Picture and Form.Icon from .frx
    {
        auto picIt = frmDesc.formControl.properties.find("Picture");
        if (picIt != frmDesc.formControl.properties.end() && picIt->second.type == FrmValueType::FrxReference && frxLoaded) {
            c_.emitLine("{ HANDLE vb6_bg = GetPropW(hwnd, L\"VB6_Picture\");");
            c_.emitLine("  if (vb6_bg) { DeleteObject(vb6_bg); RemovePropW(hwnd, L\"VB6_Picture\"); } }");
        }
        auto iconIt = frmDesc.formControl.properties.find("Icon");
        if (iconIt != frmDesc.formControl.properties.end() && iconIt->second.type == FrmValueType::FrxReference && frxLoaded) {
            c_.emitLine("{ HANDLE vb6_ico = (HANDLE)SendMessageW(hwnd, WM_GETICON, ICON_BIG, 0);");
            c_.emitLine("  if (vb6_ico) DestroyIcon((HICON)vb6_ico); }");
        }
    }
    // Fix 144: 只有**启动窗体**的 WM_DESTROY 才 PostQuitMessage.
    // 此前无条件投 WM_QUIT: 用 Show vbModal 打开的子窗体一旦销毁就把主消息循环
    // 一起结束掉 — 症状是"点按钮打开子窗体后整个程序退出" (退出码 0).
    // Startup="Sub Main" 时由 main 返回结束进程, 窗体都不投.
    {
        bool postQuit = true;
        if (!startupObject_.empty()) {
            std::string so = Symbol::toLower(startupObject_);
            std::string fn = Symbol::toLower(formName);
            std::string modLower = Symbol::toLower(moduleName_);
            postQuit = (so == fn || so == modLower);
            if (so == "sub main") postQuit = false;
        }
        if (postQuit) {
            c_.emitLine("PostQuitMessage(0);  /* Fix 144: 仅启动窗体结束消息循环 */");
        }
    }
    c_.emitLine("break;");
    c_.dedent();
    c_.emitLine("}");

    // P14.4.1b: WM_HSCROLL / WM_VSCROLL - ScrollBar事件
    c_.emitLine("case WM_HSCROLL:");
    c_.emitLine("case WM_VSCROLL: {");
    c_.indent();
    bool hasScrollBar = false;
    for (const auto& ctrl : frmDesc.formControl.children) {
        if (ctrl.controlType == FrmControlType::HScrollBar || ctrl.controlType == FrmControlType::VScrollBar) { hasScrollBar = true; break; }
    }
    if (hasScrollBar) {
    c_.emitLine("{ void* scrollHwnd = (void*)lParam; int scrollCode = LOWORD(wParam);");
    for (const auto& ctrl : frmDesc.formControl.children) {
        if (ctrl.controlType == FrmControlType::HScrollBar || ctrl.controlType == FrmControlType::VScrollBar) {
            std::string ctrlHwnd = "vb6_hwnd_" + cIdent(ctrl.controlName);
            std::string scrollFn = cProcName(ctrl.controlName + "_Scroll", AccessLevel::Private);
            std::string changeFn = cProcName(ctrl.controlName + "_Change", AccessLevel::Private);
            bool hasScroll = symTab_.lookup(ctrl.controlName + "_Scroll") != nullptr;
            bool hasChange = symTab_.lookup(ctrl.controlName + "_Change") != nullptr;
            c_.emitLine("if (scrollHwnd == (void*)" + ctrlHwnd + ") {");
            c_.indent();
            if (hasScroll) c_.emitLine("if (scrollCode == 5 || scrollCode == 4) { extern void " + scrollFn + "(); " + scrollFn + "(); }");
            if (hasChange) c_.emitLine("else { extern void " + changeFn + "(); " + changeFn + "(); }");
            c_.dedent();
            c_.emitLine("}");
        }
    }
    c_.emitLine("}");
    }
    // P16: WithEvents HScrollBar/VScrollBar事件分发
    for (const auto& we : knownWithEventsCtrlVars_) {
        if (we.second == FrmControlType::HScrollBar || we.second == FrmControlType::VScrollBar) {
            std::string varName = knownWithEventsCtrlOrigNames_.count(we.first) ? knownWithEventsCtrlOrigNames_[we.first] : cIdent(we.first);
            auto* scrollSym = symTab_.lookup(we.first + "_scroll");
            auto* changeSym = symTab_.lookup(we.first + "_change");
            c_.emitLine("if (" + varName + " != NULL && (void*)" + varName + " == scrollHwnd) {");
            c_.indent();
            if (scrollSym) {
                std::string scrollFn = cProcName(scrollSym->name, AccessLevel::Private);
                c_.emitLine("if (scrollCode == 5 || scrollCode == 4) { extern void " + scrollFn + "(); " + scrollFn + "(); }");
            }
            if (changeSym) {
                std::string changeFn = cProcName(changeSym->name, AccessLevel::Private);
                c_.emitLine("else { extern void " + changeFn + "(); " + changeFn + "(); }");
            }
            c_.dedent();
            c_.emitLine("}");
        }
    }    c_.emitLine("break;");
    c_.dedent();
    c_.emitLine("}");

    // P14.4.1d: WM_SIZE -> Form_Resize() (仅当处理器存在时生成)
    {
        std::string resizeFn = cProcName("Form_Resize", AccessLevel::Private);
        if (symTab_.lookup(formName + "_Resize") || symTab_.lookup("Form_Resize")) {
            c_.emitLine("case WM_SIZE: {");
            c_.indent();
            // Fix 112: Form_Load 由 PostMessage 延迟执行, 而首次 WM_SIZE 在
            // CreateWindow 期间就到 — 若照搬 VB6 的 Load→Resize 顺序, 这里会在
            // Form_Load 之前触发 Form_Resize, 模块级对象 (如 cResizer) 还是
            // Nothing (空指针) · 故 Load 完成前跳过, 由 0x7FF0 分支补一次 Resize.
            c_.emitLine("if (!vb6_formLoaded_" + cIdent(formName) + ") break;");
            c_.emitLine("{ extern void " + resizeFn + "(); " + resizeFn + "(); }");
            c_.emitLine("break;");
            c_.dedent();
            c_.emitLine("}");
        }
    }

    // P14.4.1e: WM_KEYDOWN/WM_CHAR -> Form_KeyDown/KeyPress (仅当处理器存在时)
    {
        std::string keyDownFn = cProcName("Form_KeyDown", AccessLevel::Private);
        if (symTab_.lookup(formName + "_KeyDown") || symTab_.lookup("Form_KeyDown")) {
            c_.emitLine("case WM_KEYDOWN: {");
            c_.indent();
            c_.emitLine("{ int16_t vb6_keyCode = (int16_t)wParam; int16_t vb6_shift = 0; if (GetKeyState(VK_SHIFT) & 0x8000) vb6_shift |= 1; if (GetKeyState(VK_CONTROL) & 0x8000) vb6_shift |= 2; if (GetKeyState(VK_MENU) & 0x8000) vb6_shift |= 4;");
            c_.emitLine("extern void " + keyDownFn + "(int16_t*, int16_t*); " + keyDownFn + "(&vb6_keyCode, &vb6_shift); }");
            c_.emitLine("break;");
            c_.dedent();
            c_.emitLine("}");
        }
    }
    {
        std::string keyPressFn = cProcName("Form_KeyPress", AccessLevel::Private);
        if (symTab_.lookup(formName + "_KeyPress") || symTab_.lookup("Form_KeyPress")) {
            c_.emitLine("case WM_CHAR: {");
            c_.indent();
            c_.emitLine("{ int16_t vb6_keyAscii = (int16_t)wParam;");
            c_.emitLine("extern void " + keyPressFn + "(int16_t*); " + keyPressFn + "(&vb6_keyAscii); }");
            c_.emitLine("break;");
            c_.dedent();
            c_.emitLine("}");
        }
    }
    // P18-B: Form_KeyUp (WM_KEYUP)
    {
        std::string keyUpFn = cProcName("Form_KeyUp", AccessLevel::Private);
        if (symTab_.lookup(formName + "_KeyUp") || symTab_.lookup("Form_KeyUp")) {
            c_.emitLine("case WM_KEYUP: {");
            c_.indent();
            c_.emitLine("{ int16_t vb6_keyCode = (int16_t)wParam; int16_t vb6_shift = 0; if (GetKeyState(VK_SHIFT) & 0x8000) vb6_shift |= 1; if (GetKeyState(VK_CONTROL) & 0x8000) vb6_shift |= 2; if (GetKeyState(VK_MENU) & 0x8000) vb6_shift |= 4;");
            c_.emitLine("extern void " + keyUpFn + "(int16_t*, int16_t*); " + keyUpFn + "(&vb6_keyCode, &vb6_shift); }");
            c_.emitLine("break;");
            c_.dedent();
            c_.emitLine("}");
        }
    }
    // P18-B: Form_MouseDown/MouseUp/MouseMove (WM_LBUTTONDOWN/UP/MOVE)
    {
        std::string mouseDownFn = cProcName("Form_MouseDown", AccessLevel::Private);
        std::string mouseUpFn = cProcName("Form_MouseUp", AccessLevel::Private);
        std::string mouseMoveFn = cProcName("Form_MouseMove", AccessLevel::Private);
        bool hasMouseDown = symTab_.lookup(formName + "_MouseDown") || symTab_.lookup("Form_MouseDown");
        bool hasMouseUp = symTab_.lookup(formName + "_MouseUp") || symTab_.lookup("Form_MouseUp");
        bool hasMouseMove = symTab_.lookup(formName + "_MouseMove") || symTab_.lookup("Form_MouseMove");
        if (hasMouseDown) {
            c_.emitLine("case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN: {");
            c_.indent();
            c_.emitLine("{ int16_t vb6_button = 0; int16_t vb6_shift = 0;");
            c_.emitLine("if (msg == WM_LBUTTONDOWN) vb6_button = 1;");
            c_.emitLine("else if (msg == WM_RBUTTONDOWN) vb6_button = 2;");
            c_.emitLine("else if (msg == WM_MBUTTONDOWN) vb6_button = 4;");
            c_.emitLine("if (wParam & MK_SHIFT) vb6_shift |= 1; if (wParam & MK_CONTROL) vb6_shift |= 2; if (GetKeyState(VK_MENU) & 0x8000) vb6_shift |= 4;");
            c_.emitLine("float vb6_x = (float)(int16_t)LOWORD(lParam); float vb6_y = (float)(int16_t)HIWORD(lParam);");
            c_.emitLine("extern void " + mouseDownFn + "(int16_t*, int16_t*, float*, float*); " + mouseDownFn + "(&vb6_button, &vb6_shift, &vb6_x, &vb6_y); }");
            c_.emitLine("break;");
            c_.dedent();
            c_.emitLine("}");
        }
        if (hasMouseUp) {
            c_.emitLine("case WM_LBUTTONUP: case WM_RBUTTONUP: case WM_MBUTTONUP: {");
            c_.indent();
            c_.emitLine("{ int16_t vb6_button = 0; int16_t vb6_shift = 0;");
            c_.emitLine("if (msg == WM_LBUTTONUP) vb6_button = 1;");
            c_.emitLine("else if (msg == WM_RBUTTONUP) vb6_button = 2;");
            c_.emitLine("else if (msg == WM_MBUTTONUP) vb6_button = 4;");
            c_.emitLine("if (wParam & MK_SHIFT) vb6_shift |= 1; if (wParam & MK_CONTROL) vb6_shift |= 2; if (GetKeyState(VK_MENU) & 0x8000) vb6_shift |= 4;");
            c_.emitLine("float vb6_x = (float)(int16_t)LOWORD(lParam); float vb6_y = (float)(int16_t)HIWORD(lParam);");
            c_.emitLine("extern void " + mouseUpFn + "(int16_t*, int16_t*, float*, float*); " + mouseUpFn + "(&vb6_button, &vb6_shift, &vb6_x, &vb6_y); }");
            c_.emitLine("break;");
            c_.dedent();
            c_.emitLine("}");
        }
        if (hasMouseMove) {
            c_.emitLine("case WM_MOUSEMOVE: {");
            c_.indent();
            c_.emitLine("{ int16_t vb6_button = 0; int16_t vb6_shift = 0;");
            c_.emitLine("if (wParam & MK_LBUTTON) vb6_button |= 1; if (wParam & MK_RBUTTON) vb6_button |= 2; if (wParam & MK_MBUTTON) vb6_button |= 4;");
            c_.emitLine("if (wParam & MK_SHIFT) vb6_shift |= 1; if (wParam & MK_CONTROL) vb6_shift |= 2; if (GetKeyState(VK_MENU) & 0x8000) vb6_shift |= 4;");
            c_.emitLine("float vb6_x = (float)(int16_t)LOWORD(lParam); float vb6_y = (float)(int16_t)HIWORD(lParam);");
            c_.emitLine("extern void " + mouseMoveFn + "(int16_t*, int16_t*, float*, float*); " + mouseMoveFn + "(&vb6_button, &vb6_shift, &vb6_x, &vb6_y); }");
            c_.emitLine("break;");
            c_.dedent();
            c_.emitLine("}");
        }
    }
    // P18-B: Form_DblClick (WM_LBUTTONDBLCLK)
    {
        std::string dblClickFn = cProcName(formName + "_DblClick", AccessLevel::Private);
        if (symTab_.lookup(formName + "_DblClick") || symTab_.lookup("Form_DblClick")) {
            c_.emitLine("case WM_LBUTTONDBLCLK: {");
            c_.indent();
            c_.emitLine("extern void " + dblClickFn + "(); " + dblClickFn + "();");
            c_.emitLine("break;");
            c_.dedent();
            c_.emitLine("}");
        }
    }
    // P18-B: WM_ACTIVATE -> Form_Activate/Deactivate
    {
        std::string activateFn = cProcName(formName + "_Activate", AccessLevel::Private);
        std::string deactivateFn = cProcName(formName + "_Deactivate", AccessLevel::Private);
        bool hasActivate = symTab_.lookup(formName + "_Activate") || symTab_.lookup("Form_Activate");
        bool hasDeactivate = symTab_.lookup(formName + "_Deactivate") || symTab_.lookup("Form_Deactivate");
        if (hasActivate || hasDeactivate) {
            c_.emitLine("case WM_ACTIVATE: {");
            c_.indent();
            if (hasActivate) {
                c_.emitLine("if (LOWORD(wParam) != WA_INACTIVE) { extern void " + activateFn + "(); " + activateFn + "(); }");
            }
            if (hasDeactivate) {
                c_.emitLine("if (LOWORD(wParam) == WA_INACTIVE) { extern void " + deactivateFn + "(); " + deactivateFn + "(); }");
            }
            c_.emitLine("break;");
            c_.dedent();
            c_.emitLine("}");
        }
    }
    // P24-Timer: WM_TIMER分发 (窗口关联定时器触发)
    c_.emitLine("case WM_TIMER: {");
    c_.indent();
    c_.emitLine("vb6_DispatchTimer((int)wParam);");
    c_.emitLine("break;");
    c_.dedent();
    c_.emitLine("}");

    // default: DefWindowProc (P7.7: MDI窗体使用DefFrameProc/DefMDIChildProc)
    // Fix 190: 必须用 W 版。窗口类现在由 RegisterClassExW 注册, 窗体的 Unicode
    // 标志随之打开 —— DefWindowProcA 会把 WM_SETTEXT/WM_GETTEXT 的 **宽字符串**
    // 当 ANSI 字节串处理, 于是标题/文本只剩第一个字符 (实测 Form1 -> "F",
    // 探针窗口 "PROBE" -> "P"; 同文件的控件子类过程本来就用 DefWindowProcW)。
    c_.emitLine("default:");
    c_.indent();
    if (isMDIForm) {
        c_.emitLine("return DefFrameProcW(hwnd, (HWND)vb6_GetMDIClient(hwnd), msg, wParam, lParam);");
    } else if (isMDIChild) {
        c_.emitLine("return DefMDIChildProcW(hwnd, msg, wParam, lParam);");
    } else {
        c_.emitLine("return DefWindowProcW(hwnd, msg, wParam, lParam);");
    }
    c_.dedent();

    c_.dedent();
    c_.emitLine("}");  // switch
    c_.emitLine("return 0;");
    c_.dedent();
    c_.emitLine("}");  // WndProc
    c_.emitBlank();
