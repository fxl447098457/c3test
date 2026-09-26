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

    // ============================================================
    // P20-42: WM_NOTIFY —— C3 此前**一个 WM_NOTIFY 分支都没有**。
    // comctl32 的通用控件 (SysTabControl32 等) 不用 WM_COMMAND 报事件, 一律走
    // WM_NOTIFY: lParam 指向 NMHDR{hwndFrom, idFrom, code}。
    // 这里只接 SSTab 的 TCN_SELCHANGE (code = -551): 交给 RTL 换算成"切换前的
    // 页号"并刷新子控件可见性, 再把该值作为参数调 <控件名>_Click。
    // ============================================================
    {
        bool anyNotify = false;
        for (const auto& ctrl : frmDesc.formControl.children) {
            if (ctrl.controlType == FrmControlType::SSTab) {
                if (!symTab_.lookup(ctrl.controlName + "_Click")) continue;
                anyNotify = true;
                break;
            }
            // C29-7: ListView 的 ItemClick / ColumnClick 也走这条 WM_NOTIFY。
            // 两个事件共用一个 case 分支, 只要有一个处理器存在就得把分支建起来。
            if (ctrl.controlType == FrmControlType::ListView) {
                if (!symTab_.lookup(ctrl.controlName + "_ItemClick") &&
                    !symTab_.lookup(ctrl.controlName + "_ColumnClick")) continue;
                anyNotify = true;
                break;
            }
            // C29-8c: TreeView 的三条通知事件同理 (NodeClick / Expand / Collapse)。
            if (ctrl.controlType == FrmControlType::TreeView) {
                if (!symTab_.lookup(ctrl.controlName + "_NodeClick") &&
                    !symTab_.lookup(ctrl.controlName + "_Expand") &&
                    !symTab_.lookup(ctrl.controlName + "_Collapse")) continue;
                anyNotify = true;
                break;
            }
            // C29-4: StatusBar 的 PanelClick / PanelDblClick 同理。
            if (ctrl.controlType == FrmControlType::StatusBar) {
                if (!symTab_.lookup(ctrl.controlName + "_PanelClick") &&
                    !symTab_.lookup(ctrl.controlName + "_PanelDblClick")) continue;
                anyNotify = true;
                break;
            }
        }
        if (anyNotify) {
            c_.emitLine("case WM_NOTIFY: {");
            c_.indent();
            c_.emitLine("if (vb6_formLoading_" + cIdent(formName) + ") break;  /* block events during form init */");
            c_.emitLine("NMHDR* pNM42 = (NMHDR*)lParam;");
            c_.emitLine("if (!pNM42) break;");
            for (const auto& ctrl : frmDesc.formControl.children) {
                if (ctrl.controlType != FrmControlType::SSTab) continue;
                auto* clickSym = symTab_.lookup(ctrl.controlName + "_Click");
                if (!clickSym) continue;
                std::string clickFn = cProcName(clickSym->name, AccessLevel::Private);
                std::string ctlNLower = ctrl.controlName;
                std::transform(ctlNLower.begin(), ctlNLower.end(), ctlNLower.begin(), ::tolower);
                std::string hwndVar = knownControlArrays_.count(ctlNLower)
                    ? ("vb6_CtrlArr_GetAt(&vb6_arr_" + cIdent(ctrl.controlName) + ", 0)")
                    : ("vb6_hwnd_" + cIdent(ctrl.controlName));
                // TCN_SELCHANGE = -551 (TCN_FIRST=-550, TCN_SELCHANGE=TCN_FIRST-1)
                c_.emitLine("if (pNM42->code == -551 && (void*)pNM42->hwndFrom == "
                            + hwndVar + ") {");
                c_.indent();
                c_.emitLine("extern void " + clickFn + "(int16_t*);");
                c_.emitLine("int16_t vb6_prev42 = (int16_t)vb6_SSTab_OnSelChange((void*)"
                            + hwndVar + ");");
                // 页号没变就不触发: 重建标签项 (Tabs/TabVisible 改动后) 时 comctl32
                // 会自发一条 prev==cur 的噪声 TCN_SELCHANGE, VB6 语义里"没切换"不算 Click。
                c_.emitLine("if (vb6_prev42 != (int16_t)vb6_SSTab_GetTab((void*)"
                            + hwndVar + ")) {");
                c_.indent();
                c_.emitLine(clickFn + "(&vb6_prev42);");
                c_.dedent();
                c_.emitLine("}");
                c_.dedent();
                c_.emitLine("}");
            }
            // ---- C29-7: ListView 的 ItemClick / ColumnClick ----
            // 两个事件都从 WM_NOTIFY 来; RTL 的 vb6_ListView_OnNotify 把通知换算成
            // **1 基下标**, 再按下标取成员对象回调 —— VB6 的事件参数是**对象**:
            //   Sub ListView1_ItemClick(ByVal Item As ListItem)
            //   Sub ListView1_ColumnClick(ByVal ColumnHeader As ColumnHeader)
            // 生成的回调形参因此是 void** (对象指针的地址)。
            for (const auto& ctrl : frmDesc.formControl.children) {
                if (ctrl.controlType != FrmControlType::ListView) continue;
                auto* icSym = symTab_.lookup(ctrl.controlName + "_ItemClick");
                auto* ccSym = symTab_.lookup(ctrl.controlName + "_ColumnClick");
                if (!icSym && !ccSym) continue;
                std::string lvNameLower = ctrl.controlName;
                std::transform(lvNameLower.begin(), lvNameLower.end(), lvNameLower.begin(), ::tolower);
                std::string lvHwndVar = knownControlArrays_.count(lvNameLower)
                    ? ("vb6_CtrlArr_GetAt(&vb6_arr_" + cIdent(ctrl.controlName) + ", 0)")
                    : ("vb6_hwnd_" + cIdent(ctrl.controlName));
                if (icSym) {
                    // LVN_ITEMACTIVATE = LVN_FIRST - 14 = -114 (双击/回车激活一行)
                    std::string icFn = cProcName(icSym->name, AccessLevel::Private);
                    c_.emitLine("if (pNM42->code == -114 && (void*)pNM42->hwndFrom == "
                                + lvHwndVar + ") {");
                    c_.indent();
                    c_.emitLine("extern void " + icFn + "(void*);");
                    c_.emitLine("int32_t vb6_lvIdx7 = vb6_ListView_OnNotify((void*)"
                                + lvHwndVar + ", -114, (void*)lParam);");
                    c_.emitLine("if (vb6_lvIdx7 > 0) {");
                    c_.indent();
                    c_.emitLine("void* vb6_lvItem7 = vb6_ListView_ListItemAt((void*)"
                                + lvHwndVar + ", vb6_lvIdx7);");
                    c_.emitLine("if (vb6_lvItem7) " + icFn + "(vb6_lvItem7);");
                    c_.dedent();
                    c_.emitLine("}");
                    c_.dedent();
                    c_.emitLine("}");
                }
                if (ccSym) {
                    // LVN_COLUMNCLICK = LVN_FIRST - 8 = -108 (点列头)
                    std::string ccFn = cProcName(ccSym->name, AccessLevel::Private);
                    c_.emitLine("if (pNM42->code == -108 && (void*)pNM42->hwndFrom == "
                                + lvHwndVar + ") {");
                    c_.indent();
                    c_.emitLine("extern void " + ccFn + "(void*);");
                    c_.emitLine("int32_t vb6_lvCol7 = vb6_ListView_OnNotify((void*)"
                                + lvHwndVar + ", -108, (void*)lParam);");
                    c_.emitLine("if (vb6_lvCol7 > 0) {");
                    c_.indent();
                    c_.emitLine("void* vb6_lvHdr7 = vb6_ListView_ColumnHeaderAt((void*)"
                                + lvHwndVar + ", vb6_lvCol7);");
                    c_.emitLine("if (vb6_lvHdr7) " + ccFn + "(vb6_lvHdr7);");
                    c_.dedent();
                    c_.emitLine("}");
                    c_.dedent();
                    c_.emitLine("}");
                }
            }
            // ---- C29-4: StatusBar 的 PanelClick / PanelDblClick ----
            // 原生 status bar 把**1 基面板号放在 NMHDR.idFrom**。RTL 的
            // vb6_StatusBar_OnNotify 校验 hwndFrom 并把 idFrom 折成返回值;
            // 再拿它造 Panel 对象 (vb6_StatusBar_PanelAt) 回调 —— VB6 的参数是对象:
            //   Sub StatusBar1_PanelClick(ByVal Panel As Panel)
            // 形状与上面 ListView 那两条完全同构。
            for (const auto& ctrl : frmDesc.formControl.children) {
                if (ctrl.controlType != FrmControlType::StatusBar) continue;
                auto* pcSym = symTab_.lookup(ctrl.controlName + "_PanelClick");
                auto* pdSym = symTab_.lookup(ctrl.controlName + "_PanelDblClick");
                if (!pcSym && !pdSym) continue;
                std::string sbNameLower = ctrl.controlName;
                std::transform(sbNameLower.begin(), sbNameLower.end(), sbNameLower.begin(), ::tolower);
                std::string sbHwndVar = knownControlArrays_.count(sbNameLower)
                    ? ("vb6_CtrlArr_GetAt(&vb6_arr_" + cIdent(ctrl.controlName) + ", 0)")
                    : ("vb6_hwnd_" + cIdent(ctrl.controlName));
                // (code, handler) 对: NM_CLICK=-2 → PanelClick, NM_DBLCLK=-6 → PanelDblClick
                for (auto evt : { std::make_pair(-2, pcSym), std::make_pair(-6, pdSym) }) {
                    if (!evt.second) continue;
                    std::string fn = cProcName(evt.second->name, AccessLevel::Private);
                    c_.emitLine("if (pNM42->code == " + std::to_string(evt.first)
                                + " && (void*)pNM42->hwndFrom == " + sbHwndVar + ") {");
                    c_.indent();
                    c_.emitLine("extern void " + fn + "(void*);");
                    c_.emitLine("int32_t vb6_sbPnl4 = vb6_StatusBar_OnNotify((void*)"
                                + sbHwndVar + ", " + std::to_string(evt.first)
                                + ", (void*)lParam);");
                    c_.emitLine("if (vb6_sbPnl4 > 0) {");
                    c_.indent();
                    c_.emitLine("void* vb6_sbPanel4 = vb6_StatusBar_PanelAt((void*)"
                                + sbHwndVar + ", vb6_sbPnl4);");
                    c_.emitLine("if (vb6_sbPanel4) " + fn + "(vb6_sbPanel4);");
                    c_.dedent();
                    c_.emitLine("}");
                    c_.dedent();
                    c_.emitLine("}");
                }

            }
            // ---- C29-8c: TreeView 的 NodeClick / Expand / Collapse ----
            // TVN_SELCHANGEDW(-451) 走 NodeClick；TVN_ITEMEXPANDEDW(-455) 按 action 分给
            // Expand / Collapse。通知负载 NMTREEVIEWW 里的 itemNew.hItem 由 RTL 折成 1 基
            // 节点序号，再拿 vb6_TreeView_NodeAt 造出 Node 对象回调 —— VB6 的处理器参数就
            // 是这个对象:
            //   Sub TreeView1_NodeClick(ByVal Node As Node)
            //   Sub TreeView1_Expand(ByVal Node As Node)   /   _Collapse 同形
            // 形状与上面 ListView / StatusBar 那两条完全同构 (共用同一个 anyNotify 分支)。
            for (const auto& ctrl : frmDesc.formControl.children) {
                if (ctrl.controlType != FrmControlType::TreeView) continue;
                auto* ncSym = symTab_.lookup(ctrl.controlName + "_NodeClick");
                auto* exSym = symTab_.lookup(ctrl.controlName + "_Expand");
                auto* coSym = symTab_.lookup(ctrl.controlName + "_Collapse");
                if (!ncSym && !exSym && !coSym) continue;
                std::string tvNameLower = ctrl.controlName;
                std::transform(tvNameLower.begin(), tvNameLower.end(), tvNameLower.begin(), ::tolower);
                std::string tvHwndVar = knownControlArrays_.count(tvNameLower)
                    ? ("vb6_CtrlArr_GetAt(&vb6_arr_" + cIdent(ctrl.controlName) + ", 0)")
                    : ("vb6_hwnd_" + cIdent(ctrl.controlName));
                // wantExp == 999 = 这条不看 action (NodeClick)；否则 -1 = 展开、0 = 折回。
                struct TvEvt { int32_t code; Symbol* sym; const char* tag; int32_t wantExp; };
                TvEvt tvEvts[] = {
                    { -451, ncSym, "nc8c", 999 },
                    { -455, exSym, "ex8c",  -1 },
                    { -455, coSym, "co8c",   0 } };
                for (const TvEvt& ev : tvEvts) {
                    if (!ev.sym) continue;
                    std::string fn = cProcName(ev.sym->name, AccessLevel::Private);
                    // ev.tag 是 const char* —— 与字面量直接相加就是"两个指针相加" (C2110)，
                    // 先把中段包成 std::string 再拼。
                    std::string vIdx = std::string("vb6_tvIdx8c_") + ev.tag;
                    std::string vNode = std::string("vb6_tvNode8c_") + ev.tag;
                    c_.emitLine("if (pNM42->code == " + std::to_string(ev.code)
                                + " && (void*)pNM42->hwndFrom == " + tvHwndVar + ") {");
                    c_.indent();
                    c_.emitLine("extern void " + fn + "(void*);");
                    c_.emitLine("int32_t " + vIdx + " = vb6_TreeView_NotifyNodeIndex((void*)"
                                + tvHwndVar + ", (void*)lParam);");
                    if (ev.wantExp == 999)
                        c_.emitLine("if (" + vIdx + " > 0) {");
                    else
                        c_.emitLine("if (" + vIdx + " > 0 && vb6_TreeView_NotifyExpanded("
                                    + "(void*)lParam) == " + std::to_string(ev.wantExp) + ") {");
                    c_.indent();
                    c_.emitLine("void* " + vNode + " = vb6_TreeView_NodeAt((void*)"
                                + tvHwndVar + ", " + vIdx + ");");
                    c_.emitLine("if (" + vNode + ") " + fn + "(" + vNode + ");");
                    c_.dedent();
                    c_.emitLine("}");
                    c_.dedent();
                    c_.emitLine("}");
                }
            }
            c_.emitLine("break;");
            c_.dedent();
            c_.emitLine("}");
        }
    }

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
    // P20-43: Form_Terminate —— VB6 语义是"窗体实例被销毁时最后触发"。
    // 挂在 WM_DESTROY 里、PostQuitMessage 之前, 这是窗体生命周期的最后一站。
    {
        auto* termSym43 = symTab_.lookup(formName + "_Terminate");
        if (!termSym43) termSym43 = symTab_.lookup("Form_Terminate");
        if (termSym43) {
            std::string termFn43 = cProcName(termSym43->name, AccessLevel::Private);
            c_.emitLine("{ extern void " + termFn43 + "(); " + termFn43 + "(); }");
        }
    }
    // P20-44: 窗体销毁时撤销该窗体控件的 OLE 放置目标登记 (RevokeDragDrop 对
    // 已销毁的 hwnd 无害, 但不清表会留悬垂 hwnd 在 g_targets 里)。
    c_.emitLine("vb6_OLEDrop_RevokeAll();");
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
        // P20-43: Form_Click —— VB6 在"按下并抬起"时触发。WM_LBUTTONUP 与 MouseUp
        // 是同一条消息, **不能各自发一个 case** (C2196 case 值重复), 必须并进同一组:
        // 有 MouseUp 处理器就共用它那组 case, 在里面补一次 Click 调用。
        auto* clickSym43 = symTab_.lookup(formName + "_Click");
        if (!clickSym43) clickSym43 = symTab_.lookup("Form_Click");
        bool hasFormClick43 = clickSym43 != nullptr;
        std::string formClickFn43 = hasFormClick43 ? cProcName(clickSym43->name, AccessLevel::Private) : std::string();
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
        if (hasMouseUp || hasFormClick43) {
            c_.emitLine("case WM_LBUTTONUP: case WM_RBUTTONUP: case WM_MBUTTONUP: {");
            c_.indent();
            c_.emitLine("{ int16_t vb6_button = 0; int16_t vb6_shift = 0;");
            c_.emitLine("if (msg == WM_LBUTTONUP) vb6_button = 1;");
            c_.emitLine("else if (msg == WM_RBUTTONUP) vb6_button = 2;");
            c_.emitLine("else if (msg == WM_MBUTTONUP) vb6_button = 4;");
            c_.emitLine("if (wParam & MK_SHIFT) vb6_shift |= 1; if (wParam & MK_CONTROL) vb6_shift |= 2; if (GetKeyState(VK_MENU) & 0x8000) vb6_shift |= 4;");
            c_.emitLine("float vb6_x = (float)(int16_t)LOWORD(lParam); float vb6_y = (float)(int16_t)HIWORD(lParam);");
            if (hasMouseUp) {
                c_.emitLine("extern void " + mouseUpFn + "(int16_t*, int16_t*, float*, float*); " + mouseUpFn + "(&vb6_button, &vb6_shift, &vb6_x, &vb6_y); }");
            }
            // P20-43: 同一条消息里补 Form_Click (有处理器才发)
            if (hasFormClick43) {
                c_.emitLine("{ extern void " + formClickFn43 + "(); " + formClickFn43 + "(); }");
            }
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
        // P20-43: 调用名必须取**符号真名**。此前写死 formName + "_DblClick",
        // 而用户写的是 `Form_DblClick` → 定义落在 vb6_Form_DblClick, 调用却发
        // vb6_<窗体名>_DblClick → LNK2019 (与 Activate 同一处缺陷)。
        auto* dblSym43 = symTab_.lookup(formName + "_DblClick");
        if (!dblSym43) dblSym43 = symTab_.lookup("Form_DblClick");
        std::string dblClickFn = dblSym43 ? cProcName(dblSym43->name, AccessLevel::Private) : std::string();
        if (dblSym43) {
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
        // P20-43: 同 DblClick —— 调用名取符号真名。写死 formName + "_Activate"
        // 会让 `Form_Activate` 的定义 (vb6_Form_Activate) 连不上 → LNK2019。
        auto* actSym43 = symTab_.lookup(formName + "_Activate");
        if (!actSym43) actSym43 = symTab_.lookup("Form_Activate");
        auto* deactSym43 = symTab_.lookup(formName + "_Deactivate");
        if (!deactSym43) deactSym43 = symTab_.lookup("Form_Deactivate");
        bool hasActivate = actSym43 != nullptr;
        bool hasDeactivate = deactSym43 != nullptr;
        if (hasActivate || hasDeactivate) {
            c_.emitLine("case WM_ACTIVATE: {");
            c_.indent();
            if (hasActivate) {
                std::string activateFn = cProcName(actSym43->name, AccessLevel::Private);
                c_.emitLine("if (LOWORD(wParam) != WA_INACTIVE) { extern void " + activateFn + "(); " + activateFn + "(); }");
            }
            if (hasDeactivate) {
                std::string deactivateFn = cProcName(deactSym43->name, AccessLevel::Private);
                c_.emitLine("if (LOWORD(wParam) == WA_INACTIVE) { extern void " + deactivateFn + "(); " + deactivateFn + "(); }");
            }
            c_.emitLine("break;");
            c_.dedent();
            c_.emitLine("}");
        }
    }
    // ============================================================
    // P20-43: 补齐窗体常用事件 (调用名一律取**符号真名**, 与 Activate 同一修法)。
    // ============================================================
    // Form_Paint (WM_PAINT) —— **已有**: create.inc 的 WM_PAINT 分支里
    // (BeginPaint/EndPaint 之间, 还捎带 OCX 绘制), 那边的命名 bug 已一并修掉,
    // 这里不能再发一个 case, 否则 C2196 case 值重复。
    // Form_GotFocus / Form_LostFocus (WM_SETFOCUS / WM_KILLFOCUS)
    {
        auto* gotSym43 = symTab_.lookup(formName + "_GotFocus");
        if (!gotSym43) gotSym43 = symTab_.lookup("Form_GotFocus");
        auto* lostSym43 = symTab_.lookup(formName + "_LostFocus");
        if (!lostSym43) lostSym43 = symTab_.lookup("Form_LostFocus");
        if (gotSym43) {
            std::string gotFn43 = cProcName(gotSym43->name, AccessLevel::Private);
            c_.emitLine("case WM_SETFOCUS: {");
            c_.indent();
            c_.emitLine("{ extern void " + gotFn43 + "(); " + gotFn43 + "(); }");
            c_.emitLine("break;");
            c_.dedent();
            c_.emitLine("}");
        }
        if (lostSym43) {
            std::string lostFn43 = cProcName(lostSym43->name, AccessLevel::Private);
            c_.emitLine("case WM_KILLFOCUS: {");
            c_.indent();
            c_.emitLine("{ extern void " + lostFn43 + "(); " + lostFn43 + "(); }");
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
