// cgen_form_frame_menu.inc - emitFormFramework 函数体片段：Frame 子控件 / 控件子类化安装 / Win32 菜单构建
// 由 src/backend/module/cgen_form.cpp 拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 1773~1937 行，逐行未改。

    // P14.4.2: 容器控件(Frame/PictureBox) - 创建容器的子控件(以容器 HWND 为父窗口)
    // 关键: .frm 中子控件坐标已是"相对容器的缇值", 且 vb6_CreateControl 内部会做缇→像素换算,
    //       因此这里必须直传原始缇值 —— 既不能减去容器偏移, 也不能再乘 15/10.
    {
        int childCtrlId = 200;  // 容器子控件ID起始值(避免与顶层 100-199 冲突)
        auto emitChildControls = [&](auto&& self, const FrmControl& container,
                                     const std::string& containerHwnd) -> void {
            for (const auto& child : container.children) {
                if (child.controlType == FrmControlType::Menu) continue;
                const char* childClass = FrmParser::controlTypeToWin32Class(child.controlType);
                if (!childClass) continue;
                int cLeft = 0, cTop = 0, cWidth = 2000, cHeight = 300;
                auto clIt = child.properties.find("Left");
                if (clIt != child.properties.end()) cLeft = (int)clIt->second.intValue;
                auto ctIt = child.properties.find("Top");
                if (ctIt != child.properties.end()) cTop = (int)ctIt->second.intValue;
                auto cwIt = child.properties.find("Width");
                if (cwIt != child.properties.end()) cWidth = (int)cwIt->second.intValue;
                auto chIt = child.properties.find("Height");
                if (chIt != child.properties.end()) cHeight = (int)chIt->second.intValue;
                long childStyle = 0x40000000L | 0x10000000L;  // WS_CHILD | WS_VISIBLE
                childStyle |= controlTypeStyleBits(child);  // 类型样式与顶层控件一致 (SS_NOTIFY/BS_GROUPBOX 等)
                std::string childCaption = child.controlName;
                auto ccIt = child.properties.find("Caption");
                if (ccIt != child.properties.end() && ccIt->second.type == FrmValueType::String) {
                    std::string raw = ccIt->second.rawText;
                    if (raw.size() >= 2 && raw.front() == 0x22 && raw.back() == 0x22) childCaption = raw.substr(1, raw.size() - 2);
                }
                auto ctIt2 = child.properties.find("Text");
                if (ctIt2 != child.properties.end() && ctIt2->second.type == FrmValueType::String) {
                    std::string raw = ctIt2->second.rawText;
                    if (raw.size() >= 2 && raw.front() == 0x22 && raw.back() == 0x22) childCaption = raw.substr(1, raw.size() - 2);
                }
                if (controlTypeClearsCaption(child)) childCaption = "";
                c_.emitLine("vb6_hwnd_" + cIdent(child.controlName) + " = vb6_CreateControl("
                    + "\"" + std::string(childClass) + "\", \"" + childCaption + "\","
                    + std::to_string(childStyle) + "L, 0L,"
                    + std::to_string(cLeft) + ", " + std::to_string(cTop) + ", "
                    + std::to_string(cWidth) + ", " + std::to_string(cHeight) + ","
                    + std::to_string(childCtrlId) + ", " + containerHwnd + ", hInstance);");
                // 设计期 BorderStyle / Alignment (容器子控件)
                emitDesignerStyleProps(child, "vb6_hwnd_" + cIdent(child.controlName));
                emitControlFrxProps(child);  // Fix 182: 容器子控件同样加载 .frx 设计期图片/文本/列表
                // 注册控件名到knownFormControls_(供属性访问)
                {
                    std::string childNameLower = child.controlName;
                    std::transform(childNameLower.begin(), childNameLower.end(), childNameLower.begin(), ::tolower);
                    knownFormControls_[childNameLower] = child.controlType;
                    knownFormControlOriginalNames_[childNameLower] = child.controlName;
                }
                childCtrlId++;
                // 递归处理多层容器(Frame/PictureBox 内再嵌容器)
                if (!child.children.empty() &&
                    (child.controlType == FrmControlType::Frame ||
                     child.controlType == FrmControlType::PictureBox)) {
                    self(self, child, "vb6_hwnd_" + cIdent(child.controlName));
                }
            }
        };
        for (const auto& ctrl : frmDesc.formControl.children) {
            if (ctrl.children.empty()) continue;
            if (ctrl.controlType != FrmControlType::Frame &&
                ctrl.controlType != FrmControlType::PictureBox) continue;
            emitChildControls(emitChildControls, ctrl, "vb6_hwnd_" + cIdent(ctrl.controlName));
        }
    }

    // P18-F: 安装控件子类化 (在所有控件创建之后)
    {
        // 收集需要子类化的控件名(去重)
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
        // 顶层控件
        for (const auto& ctrl : frmDesc.formControl.children) {
            std::string ctrlNameLower = ctrl.controlName;
            std::transform(ctrlNameLower.begin(), ctrlNameLower.end(), ctrlNameLower.begin(), ::tolower);
            if (subEmitted.count(ctrlNameLower)) continue;
            subEmitted.insert(ctrlNameLower);
            if (!needsSubclass(ctrl)) continue;
            std::string subProcName = "vb6_ctrl_subproc_" + cIdent(ctrl.controlName);
            if (knownControlArrays_.count(ctrlNameLower)) {
                // P0-4修复: 控件数组遍历所有元素安装子类化
                c_.emitLine("{ int vb6_si; for (vb6_si = 0; vb6_si < VB6_CTRLARR_MAX; vb6_si++) {");
                c_.emitLine("  void* vb6_sub_hwnd = vb6_CtrlArr_GetAt(&vb6_arr_" + cIdent(ctrl.controlName) + ", vb6_si);");
                c_.emitLine("  if (vb6_sub_hwnd) vb6_InstallControlSubclass(vb6_sub_hwnd, (void*)" + subProcName + "); } }");
            } else {
                c_.emitLine("vb6_InstallControlSubclass((void*)vb6_hwnd_" + cIdent(ctrl.controlName) + ", (void*)" + subProcName + ");");
            }
        }
        // P0-5修复: 容器(Frame/PictureBox)内的子控件 (含多层嵌套)
        {
            auto subclassContainerChildren = [&](auto&& self, const FrmControl& container) -> void {
                for (const auto& child : container.children) {
                    std::string childNameLower = child.controlName;
                    std::transform(childNameLower.begin(), childNameLower.end(), childNameLower.begin(), ::tolower);
                    if (!subEmitted.count(childNameLower)) {
                        subEmitted.insert(childNameLower);
                        if (needsSubclass(child)) {
                            std::string subProcName = "vb6_ctrl_subproc_" + cIdent(child.controlName);
                            c_.emitLine("vb6_InstallControlSubclass((void*)vb6_hwnd_" + cIdent(child.controlName) + ", (void*)" + subProcName + ");");
                        }
                    }
                    if (!child.children.empty() &&
                        (child.controlType == FrmControlType::Frame ||
                         child.controlType == FrmControlType::PictureBox)) {
                        self(self, child);
                    }
                }
            };
            for (const auto& ctrl : frmDesc.formControl.children) {
                if (!ctrl.children.empty() &&
                    (ctrl.controlType == FrmControlType::Frame ||
                     ctrl.controlType == FrmControlType::PictureBox)) {
                    subclassContainerChildren(subclassContainerChildren, ctrl);
                }
            }
        }
    }

    // Fix 142: 容器控件转发子控件 WM_COMMAND 到窗体 WndProc.
    // 顶层与嵌套 Frame/PictureBox 均安装 (含仅作为容器的控件).
    {
        auto emitForward142 = [&](auto&& self, const FrmControl& ctrl) -> void {
            if ((ctrl.controlType == FrmControlType::Frame ||
                 ctrl.controlType == FrmControlType::PictureBox) && !ctrl.children.empty()) {
                std::string nmLower = ctrl.controlName;
                std::transform(nmLower.begin(), nmLower.end(), nmLower.begin(), ::tolower);
                if (!knownControlArrays_.count(nmLower)) {
                    c_.emitLine("vb6_ForwardChildCommands((void*)vb6_hwnd_" + cIdent(ctrl.controlName) + ");");
                }
            }
            for (const auto& child : ctrl.children) self(self, child);
        };
        for (const auto& ctrl : frmDesc.formControl.children) {
            emitForward142(emitForward142, ctrl);
        }
    }

    // P7.8: 菜单构建 (VB.Menu控件不创建窗口, 用Win32菜单API)
    {
        int menuId = 1000;  // 菜单项ID起始值 (控件ID用100-999)
        bool hasMenu = false;
        for (const auto& ctrl : frmDesc.formControl.children) {
            if (ctrl.controlType == FrmControlType::Menu) {
                hasMenu = true;
                break;
            }
        }
        if (hasMenu) {
            c_.emitBlank();
            c_.emitLine("// P7.8: Menu construction");
            c_.emitLine("HMENU hMenuBar = CreateMenu();");

            for (const auto& ctrl : frmDesc.formControl.children) {
                if (ctrl.controlType != FrmControlType::Menu) continue;

                // Top-level menu item -> popup menu
                std::string caption = ctrl.controlName;
                auto capIt = ctrl.properties.find("Caption");
                if (capIt != ctrl.properties.end() && capIt->second.type == FrmValueType::String) {
                    std::string raw = capIt->second.rawText;
                    if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"') {
                        caption = raw.substr(1, raw.size() - 2);
                    }
                }

                if (ctrl.children.empty()) {
                    c_.emitLine("AppendMenuW(hMenuBar, MF_STRING, " + std::to_string(menuId) + ", L\"" + escapeWideCString(caption) + "\");");
                    menuId++;
                } else {
                    std::string popupVar = "hPopup_" + cIdent(ctrl.controlName);
                    c_.emitLine("HMENU " + popupVar + " = CreatePopupMenu();");

                    for (const auto& child : ctrl.children) {
                        emitMenuItem(popupVar, child, menuId);
                    }

                    c_.emitLine("AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)" + popupVar + ", L\"" + escapeWideCString(caption) + "\");");
                }
            }

            c_.emitLine("SetMenu((HWND)hwnd, hMenuBar);");
            c_.emitBlank();
        }
    }

    c_.emitLine("vb6_formLoading_" + cIdent(formName) + " = 0;  /* allow events after form init */");
    c_.dedent();
    c_.emitLine("}");  // CreateControls
    c_.emitBlank();
