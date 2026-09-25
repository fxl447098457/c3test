// cgen_form_wndproc_create.inc - emitFormFramework 函数体片段：WndProc 前半 —— WM_CREATE / 延迟 Form_Load / WM_COMMAND / WithEvents 派发
// 由 src/backend/module/cgen_form.cpp 拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 598~921 行，逐行未改。

    // WndProc
    c_.emitLine("// === " + formName + " WndProc ===");
    c_.emitLine("LRESULT CALLBACK " + wndProc + "(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {");
    c_.indent();
    c_.emitLine("switch (msg) {");

    // Fix 143d: WM_PAINT — 无窗口 (windowless) OCX 控件不会自己收 WM_PAINT,
    // 必须由宿主在 BeginPaint/EndPaint 之间调用 IViewObject::Draw 让控件绘制.
    // 不生成这个 case 时, 第三方 OCX (如 NewTab01.ocx) 激活成功但窗体上空白.
    c_.indent();
    c_.emitLine("case WM_PAINT: {");
    c_.indent();
    c_.emitLine("PAINTSTRUCT _vb6_ps;");
    c_.emitLine("HDC _vb6_hdc = BeginPaint(hwnd, &_vb6_ps);");
    if (std::getenv("C3_OCX_NO_PAINT") == nullptr) {
        c_.emitLine("vb6_OcxHost_PaintAll((void*)hwnd, (void*)_vb6_hdc);  /* Fix 143d: 无窗口/窗口化 OCX 绘制 */");
    }
    // Form_Paint 事件 (若有)
    if (symTab_.lookup(formName + "_Paint")) {
        c_.emitLine("{ extern void " + cProcName(formName + "_Paint", AccessLevel::Private) + "(); "
                    + cProcName(formName + "_Paint", AccessLevel::Private) + "(); }");
    }
    c_.emitLine("EndPaint(hwnd, &_vb6_ps);");
    c_.emitLine("break;");
    c_.dedent();
    c_.emitLine("}");
    c_.dedent();

    // WM_CREATE: 创建控件
    c_.indent();
    c_.emitLine("case WM_CREATE: {");
    c_.indent();
    c_.emitLine("CREATESTRUCTA* cs = (CREATESTRUCTA*)lParam;");
    c_.emitLine(createFn + "((void*)hwnd, (void*)cs->hInstance);");
    c_.emitLine("vb6_Forms_Register((void*)hwnd);");

    // P24: Form.Icon from .frx - load and set window icon
    {
        auto iconIt = frmDesc.formControl.properties.find("Icon");
        if (iconIt != frmDesc.formControl.properties.end() && iconIt->second.type == FrmValueType::FrxReference && frxLoaded) {
            std::string varName = "vb6_frx_icon_" + cIdent(formName);
            c_.emitLine("{ void* vb6_icon = vb6_LoadIconFromMemory(" + varName + ", " + varName + "_size);");
            c_.emitLine("  if (vb6_icon) { SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)vb6_icon);");
            c_.emitLine("    SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)vb6_icon); } }");
        }
    }
    // P24: Form.Picture from .frx - load and store as window property (for WM_PAINT)
    {
        auto picIt2 = frmDesc.formControl.properties.find("Picture");
        if (picIt2 != frmDesc.formControl.properties.end() && picIt2->second.type == FrmValueType::FrxReference && frxLoaded) {
            std::string varName = "vb6_frx_bgpic_" + cIdent(formName);
            c_.emitLine("{ void* vb6_bgpic = vb6_LoadPictureFromMemory(" + varName + ", " + varName + "_size);");
            c_.emitLine("  if (vb6_bgpic) SetPropW(hwnd, L\"VB6_Picture\", (HANDLE)vb6_bgpic); }");
        }
    }

    // P20-40: 从.frm属性初始化Form属性
    {
        auto kpIt = frmDesc.formControl.properties.find("KeyPreview");
        if (kpIt != frmDesc.formControl.properties.end() && kpIt->second.intValue != 0) {
            c_.emitLine("vb6_SetKeyPreview((void*)hwnd, -1);");
        }
        auto cbIt = frmDesc.formControl.properties.find("ControlBox");
        if (cbIt != frmDesc.formControl.properties.end() && cbIt->second.intValue == 0) {
            c_.emitLine("vb6_SetControlBox((void*)hwnd, 0);");
        }
        auto mbIt = frmDesc.formControl.properties.find("MaxButton");
        if (mbIt != frmDesc.formControl.properties.end() && mbIt->second.intValue == 0) {
            c_.emitLine("vb6_SetMaxButton((void*)hwnd, 0);");
        }
        auto mnIt = frmDesc.formControl.properties.find("MinButton");
        if (mnIt != frmDesc.formControl.properties.end() && mnIt->second.intValue == 0) {
            c_.emitLine("vb6_SetMinButton((void*)hwnd, 0);");
        }
        auto wsIt = frmDesc.formControl.properties.find("WindowState");
        if (wsIt != frmDesc.formControl.properties.end() && wsIt->second.intValue != 0) {
            c_.emitLine("vb6_SetWindowState((void*)hwnd, " + std::to_string((int)wsIt->second.intValue) + ");");
        }
    }

    // M22-fix: 在Form_Load之前先赋值vb6_hwnd_<Form>，这样Form_Load中
    // Me.Caption / Me.Text 等才能通过vb6_hwnd_引用到正确的HWND
    // CreateFormWindow还未返回，vb6_hwnd_尚未被赋值
    c_.emitLine("vb6_hwnd_" + cIdent(formName) + " = (void*)hwnd;  /* early assign for Form_Load */");

    // Fix 058: Form_Load 用 PostMessage 延迟执行, 避免在 WM_CREATE 中
    // 执行 COM 操作 (如 Controls.Add) 导致消息死锁
    // 调用VB6 Form_Load事件 (仅当存在时调用)
    std::string formLoadFn = cProcName("Form_Load", AccessLevel::Private);
    auto* formLoadSym = symTab_.lookup("Form_Load");
    if (formLoadSym) {
        c_.emitLine("PostMessageW(hwnd, 0x7FF0, 0, 0);  /* defer Form_Load */");
    }
    // P24-Timer: 在WM_CREATE中注册Timer (此时hwnd可用)
    for (const auto& ctrl : frmDesc.formControl.children) {
        if (ctrl.controlType == FrmControlType::Timer) {
            int interval = 1000;
            auto pIt = ctrl.properties.find("Interval");
            if (pIt != ctrl.properties.end()) interval = (int)pIt->second.intValue;
            int enabled = 1;
            pIt = ctrl.properties.find("Enabled");
            if (pIt != ctrl.properties.end()) enabled = (int)pIt->second.intValue;
            std::string timerFn = cProcName(ctrl.controlName + "_Timer", AccessLevel::Private);
            if (enabled) {
                c_.emitLine("{ extern void " + timerFn + "(); vb6_SetTimer((void*)hwnd, " + std::to_string(interval) + ", (void*)" + timerFn + "); }");
            }
        }
    }
    c_.emitLine("break;");
    c_.dedent();
    c_.emitLine("}");

    // M12-FIX: Form.Picture background via WM_ERASEBKGND (single display at 0,0)
    {
        auto picIt = frmDesc.formControl.properties.find("Picture");
        if (picIt != frmDesc.formControl.properties.end() && picIt->second.type == FrmValueType::FrxReference && frxLoaded) {
            c_.emitLine("case WM_ERASEBKGND: {");
            c_.indent();
            c_.emitLine("HANDLE vb6_bg = GetPropW(hwnd, L\"VB6_Picture\");");
            c_.emitLine("if (vb6_bg && GetObjectType((HGDIOBJ)vb6_bg) == OBJ_BITMAP) {");
            c_.indent();
            c_.emitLine("HDC hdc = (HDC)(WPARAM)wParam;");
            c_.emitLine("RECT rc; GetClientRect(hwnd, &rc);");
            c_.emitLine("HBRUSH hBr = (HBRUSH)(COLOR_BTNFACE+1); FillRect(hdc, &rc, hBr);");
            c_.emitLine("HDC memDC = CreateCompatibleDC(hdc);");
            c_.emitLine("HBITMAP hBmp = (HBITMAP)vb6_bg;");
            c_.emitLine("BITMAP bm; GetObjectW(hBmp, sizeof(bm), &bm);");
            c_.emitLine("HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, hBmp);");
            c_.emitLine("BitBlt(hdc, 0, 0, bm.bmWidth, bm.bmHeight, memDC, 0, 0, SRCCOPY);");
            c_.emitLine("SelectObject(memDC, oldBmp);");
            c_.emitLine("DeleteDC(memDC);");
            c_.emitLine("return 1;");
            c_.dedent();
            c_.emitLine("}");
            c_.emitLine("break;");
            c_.dedent();
            c_.emitLine("}");
        }
    }

    // Fix 058: 处理延迟的 Form_Load (WM_USER+0x100 = 0x7FF0)
    if (formLoadSym) {
        c_.emitLine("case 0x7FF0: {");
        c_.indent();
        c_.emitLine("{ /* Form_Load (deferred from WM_CREATE) */ extern void " + formLoadFn + "(); " + formLoadFn + "(); }");
        // Fix 112: Load 完成后再触发首次 Form_Resize (与 VB6 的 Load→Resize 顺序一致)
        c_.emitLine("vb6_formLoaded_" + cIdent(formName) + " = 1;");
        {
            std::string resizeFnLoad = cProcName("Form_Resize", AccessLevel::Private);
            if (symTab_.lookup(formName + "_Resize") || symTab_.lookup("Form_Resize")) {
                c_.emitLine("{ extern void " + resizeFnLoad + "(); " + resizeFnLoad + "(); }");
            }
        }
        c_.emitLine("break;");
        c_.dedent();
        c_.emitLine("}");
    }

    // WM_COMMAND: 按钮点击等 (P7.6: 支持控件数组Index参数)
    c_.emitLine("case WM_COMMAND: {");
    c_.indent();
    c_.emitLine("if (vb6_formLoading_" + cIdent(formName) + ") break;  /* block events during form init */");
    c_.emitLine("int id = LOWORD(wParam);");
    c_.emitLine("int code = HIWORD(wParam);");

    // 为每个CommandButton/CheckBox/OptionButton生成WM_COMMAND处理
    // Fix 142: 抽为 lambda, 顶层控件(ID 100+)与容器内子控件(ID 200+)共用同一派发逻辑
    auto emitCommandDispatch142 = [&](const FrmControl& ctrl, int cid) {
        ctrlId = cid;
        std::string ctrlNameLower = ctrl.controlName;
        std::transform(ctrlNameLower.begin(), ctrlNameLower.end(), ctrlNameLower.begin(), ::tolower);
        bool isArrayCtrl = knownControlArrays_.count(ctrlNameLower) > 0;

        if (ctrl.controlType == FrmControlType::CommandButton ||
            ctrl.controlType == FrmControlType::CheckBox ||
            ctrl.controlType == FrmControlType::OptionButton) {
            if (symTab_.lookup(ctrl.controlName + "_Click")) {
                std::string clickFn = cProcName(ctrl.controlName + "_Click", AccessLevel::Private);
                c_.emitLine("if (id == " + std::to_string(ctrlId) + ") {");
                c_.indent();
                if (isArrayCtrl) {
                    // 控件数组: 事件处理带Index参数
                    std::string idxVar = "vb6_idx_" + std::to_string(ctrl.index >= 0 ? ctrl.index : 0);
                    c_.emitLine("{ int16_t " + idxVar + " = " + std::to_string(ctrl.index >= 0 ? ctrl.index : 0) + "; extern void " + clickFn + "(int16_t*); " + clickFn + "(&" + idxVar + "); }");
                } else {
                    c_.emitLine("{ extern void " + clickFn + "(); " + clickFn + "(); }");
                }
                c_.dedent();
                c_.emitLine("}");
            }
        }
        else if (ctrl.controlType == FrmControlType::TextBox) {
            // P14.4.1: EN_CHANGE (code=0x0300=768) -> _Change() (仅当处理器存在时)
            if (symTab_.lookup(ctrl.controlName + "_Change")) {
                std::string changeFn = cProcName(ctrl.controlName + "_Change", AccessLevel::Private);
                c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 768) {");
                c_.indent();
                c_.emitLine("{ extern void " + changeFn + "(); " + changeFn + "(); }");
                c_.dedent();
                c_.emitLine("}");
            }
        } else if (ctrl.controlType == FrmControlType::ListBox) {
            // P14.4.1: LBN_SELCHANGE (code=1) -> _Click() (仅当处理器存在时)
            if (symTab_.lookup(ctrl.controlName + "_Click")) {
                std::string clickFn = cProcName(ctrl.controlName + "_Click", AccessLevel::Private);
                c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 1) {");
                c_.indent();
                c_.emitLine("{ extern void " + clickFn + "(); " + clickFn + "(); }");
                c_.dedent();
                c_.emitLine("}");
            }
            // LBN_DBLCLK (code=2) -> _DblClick() (仅当处理器存在时)
            if (symTab_.lookup(ctrl.controlName + "_DblClick")) {
                std::string dblClickFn = cProcName(ctrl.controlName + "_DblClick", AccessLevel::Private);
                c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 2) {");
                c_.indent();
                c_.emitLine("{ extern void " + dblClickFn + "(); " + dblClickFn + "(); }");
                c_.dedent();
                c_.emitLine("}");
            }
        } else if (ctrl.controlType == FrmControlType::DriveListBox) {
            // C29-1b: 换盘 = CBN_SELCHANGE(1) → Drive1_Change；VB6 同一条通知也发 Click，
            // 所以两个处理器各自挂一次（有哪个就发哪个，缺一不报错）。
            if (symTab_.lookup(ctrl.controlName + "_Change")) {
                std::string drvChgFn = cProcName(ctrl.controlName + "_Change", AccessLevel::Private);
                c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 1) {");
                c_.indent();
                c_.emitLine("{ extern void " + drvChgFn + "(); " + drvChgFn + "(); }");
                c_.dedent();
                c_.emitLine("}");
            }
            if (symTab_.lookup(ctrl.controlName + "_Click")) {
                std::string drvClkFn = cProcName(ctrl.controlName + "_Click", AccessLevel::Private);
                c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 1) {");
                c_.indent();
                c_.emitLine("{ extern void " + drvClkFn + "(); " + drvClkFn + "(); }");
                c_.dedent();
                c_.emitLine("}");
            }
        } else if (ctrl.controlType == FrmControlType::DirListBox) {
            // C29-1b: 双击列表项 = 下钻一层。VB6 的 Dir1_Change 是"Path 真的变了"才发,
            // 所以先让 RTL 试下钻、返回 1 才调处理器 —— 双击卷标行/空项不惊动事件。
            if (symTab_.lookup(ctrl.controlName + "_Change")) {
                std::string dirChgFn = cProcName(ctrl.controlName + "_Change", AccessLevel::Private);
                std::string dirHwnd = ctrlHwndExprForInit(ctrl);
                c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 2) {");
                c_.indent();
                c_.emitLine("if (vb6_DirListBoxDescendSelected((void*)" + dirHwnd + ")) {");
                c_.indent();
                c_.emitLine("{ extern void " + dirChgFn + "(); " + dirChgFn + "(); }");
                c_.dedent();
                c_.emitLine("}");
                c_.dedent();
                c_.emitLine("}");
            }
            if (symTab_.lookup(ctrl.controlName + "_Click")) {
                std::string dirClkFn = cProcName(ctrl.controlName + "_Click", AccessLevel::Private);
                c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 1) {");
                c_.indent();
                c_.emitLine("{ extern void " + dirClkFn + "(); " + dirClkFn + "(); }");
                c_.dedent();
                c_.emitLine("}");
            }
        } else if (ctrl.controlType == FrmControlType::FileListBox) {
            // C29-1b: 选中文件 = LBN_SELCHANGE(1) → Click；双击 = LBN_DBLCLK(2) → DblClick
            if (symTab_.lookup(ctrl.controlName + "_Click")) {
                std::string fileClkFn = cProcName(ctrl.controlName + "_Click", AccessLevel::Private);
                c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 1) {");
                c_.indent();
                c_.emitLine("{ extern void " + fileClkFn + "(); " + fileClkFn + "(); }");
                c_.dedent();
                c_.emitLine("}");
            }
            if (symTab_.lookup(ctrl.controlName + "_DblClick")) {
                std::string fileDblFn = cProcName(ctrl.controlName + "_DblClick", AccessLevel::Private);
                c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 2) {");
                c_.indent();
                c_.emitLine("{ extern void " + fileDblFn + "(); " + fileDblFn + "(); }");
                c_.dedent();
                c_.emitLine("}");
            }
        } else if (ctrl.controlType == FrmControlType::ComboBox) {
            // P14.4.1: CBN_SELCHANGE (code=1) -> _Click() (仅当处理器存在时)
            if (symTab_.lookup(ctrl.controlName + "_Click")) {
                std::string clickFn = cProcName(ctrl.controlName + "_Click", AccessLevel::Private);
                c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 1) {");
                c_.indent();
                c_.emitLine("{ extern void " + clickFn + "(); " + clickFn + "(); }");
                c_.dedent();
                c_.emitLine("}");
            }
            // CBN_EDITCHANGE (code=5) -> _Change() (仅当处理器存在时)
            if (symTab_.lookup(ctrl.controlName + "_Change")) {
                std::string changeFn = cProcName(ctrl.controlName + "_Change", AccessLevel::Private);
                c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 5) {");
                c_.indent();
                c_.emitLine("{ extern void " + changeFn + "(); " + changeFn + "(); }");
                c_.dedent();
                c_.emitLine("}");
            }
            // CBN_DROPDOWN (code=7) -> _DropDown() (仅当处理器存在时)
            if (symTab_.lookup(ctrl.controlName + "_DropDown")) {
                std::string dropDownFn = cProcName(ctrl.controlName + "_DropDown", AccessLevel::Private);
                c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 7) {");
                c_.indent();
                c_.emitLine("{ extern void " + dropDownFn + "(); " + dropDownFn + "(); }");
                c_.dedent();
                c_.emitLine("}");
            }
            // CBN_CLOSEUP (code=8) -> _CloseUp() (仅当处理器存在时)
            if (symTab_.lookup(ctrl.controlName + "_CloseUp")) {
                std::string closeUpFn = cProcName(ctrl.controlName + "_CloseUp", AccessLevel::Private);
                c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 8) {");
                c_.indent();
                c_.emitLine("{ extern void " + closeUpFn + "(); " + closeUpFn + "(); }");
                c_.dedent();
                c_.emitLine("}");
            }
        }
    };

    ctrlId = 100;
    for (const auto& ctrl : frmDesc.formControl.children) {
        emitCommandDispatch142(ctrl, ctrlId);
        ctrlId++;
    }
    // Fix 142: 容器(Frame/PictureBox)内子控件 — ID 从 200 起, DFS 顺序与
    // cgen_form_frame_menu.inc 的创建循环完全一致 (跳过 Menu 与无 Win32 类控件).
    {
        int childCmdId142 = 200;
        auto walkCmdChildren142 = [&](auto&& self, const FrmControl& container) -> void {
            for (const auto& child : container.children) {
                if (child.controlType == FrmControlType::Menu) continue;
                const char* childClass142 = FrmParser::controlTypeToWin32Class(child.controlType);
                if (!childClass142) continue;
                emitCommandDispatch142(child, childCmdId142);
                childCmdId142++;
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
                walkCmdChildren142(walkCmdChildren142, ctrl);
            }
        }
    }

    // P16: WithEvents控件事件分发 (按HWND匹配, 在标准命名handler之后)
    // 对于每个WithEvents控件变量, 检查其HWND是否匹配lParam, 是则调用对应handler
    if (!knownWithEventsCtrlVars_.empty()) {
        c_.emitLine("if ((HWND)lParam != NULL) {");
        c_.indent();
        c_.emitLine("HWND hCtrlWE = (HWND)lParam;");
        for (const auto& we : knownWithEventsCtrlVars_) {
            std::string varLower = we.first;
            FrmControlType ctrlType = we.second;
            std::string varName = knownWithEventsCtrlOrigNames_.count(varLower) ? knownWithEventsCtrlOrigNames_[varLower] : cIdent(varLower);  // 用原始大小写变量名
            // 根据控件类型生成不同的事件分发
            if (ctrlType == FrmControlType::CommandButton ||
                ctrlType == FrmControlType::CheckBox ||
                ctrlType == FrmControlType::OptionButton) {
                // BN_CLICKED (code=0) → _Click()
                auto* clickSym = symTab_.lookup(varLower + "_click");
                if (clickSym) {
                    std::string clickFn = cProcName(clickSym->name, AccessLevel::Private);
                    c_.emitLine("if (code == 0 && " + varName + " != NULL && " + varName + " == hCtrlWE) {");
                    c_.indent();
                    c_.emitLine("{ extern void " + clickFn + "(); " + clickFn + "(); }");
                    c_.dedent();
                    c_.emitLine("}");
                }
            } else if (ctrlType == FrmControlType::TextBox) {
                // EN_CHANGE (code=768) → _Change()
                auto* changeSym = symTab_.lookup(varLower + "_change");
                if (changeSym) {
                    std::string changeFn = cProcName(changeSym->name, AccessLevel::Private);
                    c_.emitLine("if (code == 768 && " + varName + " != NULL && " + varName + " == hCtrlWE) {");
                    c_.indent();
                    c_.emitLine("{ extern void " + changeFn + "(); " + changeFn + "(); }");
                    c_.dedent();
                    c_.emitLine("}");
                }
            } else if (ctrlType == FrmControlType::ListBox) {
                // LBN_SELCHANGE (code=1) → _Click()
                auto* clickSym = symTab_.lookup(varLower + "_click");
                if (clickSym) {
                    std::string clickFn = cProcName(clickSym->name, AccessLevel::Private);
                    c_.emitLine("if (code == 1 && " + varName + " != NULL && " + varName + " == hCtrlWE) {");
                    c_.indent();
                    c_.emitLine("{ extern void " + clickFn + "(); " + clickFn + "(); }");
                    c_.dedent();
                    c_.emitLine("}");
                }
                // LBN_DBLCLK (code=2) → _DblClick()
                auto* dblClickSym = symTab_.lookup(varLower + "_dblclick");
                if (dblClickSym) {
                    std::string dblClickFn = cProcName(dblClickSym->name, AccessLevel::Private);
                    c_.emitLine("if (code == 2 && " + varName + " != NULL && " + varName + " == hCtrlWE) {");
                    c_.indent();
                    c_.emitLine("{ extern void " + dblClickFn + "(); " + dblClickFn + "(); }");
                    c_.dedent();
                    c_.emitLine("}");
                }
            } else if (ctrlType == FrmControlType::ComboBox) {
                // CBN_SELCHANGE (code=1) → _Click()
                auto* clickSym = symTab_.lookup(varLower + "_click");
                if (clickSym) {
                    std::string clickFn = cProcName(clickSym->name, AccessLevel::Private);
                    c_.emitLine("if (code == 1 && " + varName + " != NULL && " + varName + " == hCtrlWE) {");
                    c_.indent();
                    c_.emitLine("{ extern void " + clickFn + "(); " + clickFn + "(); }");
                    c_.dedent();
                    c_.emitLine("}");
                }
                // CBN_EDITCHANGE (code=5) → _Change()
                auto* changeSym = symTab_.lookup(varLower + "_change");
                if (changeSym) {
                    std::string changeFn = cProcName(changeSym->name, AccessLevel::Private);
                    c_.emitLine("if (code == 5 && " + varName + " != NULL && " + varName + " == hCtrlWE) {");
                    c_.indent();
                    c_.emitLine("{ extern void " + changeFn + "(); " + changeFn + "(); }");
                    c_.dedent();
                    c_.emitLine("}");
                }
            }
            // P24-Timer: WM_TIMER由WndProc分发 (不再由消息循环拦截)
            // HScrollBar/VScrollBar: 在WM_HSCROLL/WM_VSCROLL中处理(下方)
        }
        c_.dedent();
        c_.emitLine("}");  /* close if ((HWND)lParam != NULL) */
    }
    // P7.8: 菜单项点击事件派发 (Menu控件ID从1000开始)
    {
        int menuId = 1000;
        for (const auto& ctrl : frmDesc.formControl.children) {
            if (ctrl.controlType == FrmControlType::Menu) {
                emitMenuClickDispatch(ctrl, menuId);
            }
        }
    }
