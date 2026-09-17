// cgen_form_ctrl_registry.inc - emitFormFramework 函数体片段：控件名映射 / 控件数组检测 与 .h 声明
// 由 src/backend/module/cgen_form.cpp 拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 145~261 行，逐行未改。

    // --- P7.5+P7.6: 填充已知控件名映射 + 检测控件数组 ---
    {
        std::string formNameLower = formName;
        std::transform(formNameLower.begin(), formNameLower.end(), formNameLower.begin(), ::tolower);
        knownFormName_ = formNameLower;
        knownFormControls_[formNameLower] = FrmControlType::Form;
        knownFormControlOriginalNames_[formNameLower] = formName;

        // P7.6: 先扫描控件数组 (同名控件出现多次 = 数组)
        // Fix 086: 递归注册嵌套控件 (Menu子项如 Window > WindowTopMost).
        // 此前仅注册顶层控件, 嵌套菜单成员访问 (.Checked 等) 误判为
        // Module.X → 生成 vb6_<菜单名>_<属性> 未声明标识符 (C2065).
        std::unordered_set<std::string> ctrlArraySeen;
        std::function<void(const FrmControl&)> registerCtrlRec;
        registerCtrlRec = [&](const FrmControl& ctrl) {
            std::string ctrlNameLower = ctrl.controlName;
            std::transform(ctrlNameLower.begin(), ctrlNameLower.end(), ctrlNameLower.begin(), ::tolower);
            if (!ctrlNameLower.empty()) {
                if (!ctrlArraySeen.insert(ctrlNameLower).second) {
                    // 同名控件出现多次 = 控件数组
                    knownControlArrays_[ctrlNameLower] = true;
                }
                knownFormControls_[ctrlNameLower] = ctrl.controlType;
                knownFormControlOriginalNames_[ctrlNameLower] = ctrl.controlName;
            }
            for (const auto& child : ctrl.children) {
                registerCtrlRec(child);
            }
        };
        for (const auto& ctrl : frmDesc.formControl.children) {
            registerCtrlRec(ctrl);
        }

        // P7.6: 构建控件ID→Index映射 (WM_COMMAND事件分发用)
        {
            int tempId = 100;
            for (const auto& ctrl : frmDesc.formControl.children) {
                std::string ctrlNameLower = ctrl.controlName;
                std::transform(ctrlNameLower.begin(), ctrlNameLower.end(), ctrlNameLower.begin(), ::tolower);
                if (knownControlArrays_.count(ctrlNameLower) && ctrl.index >= 0) {
                    auto& idxMap = controlIdToIndexMap_[ctrlNameLower];
                    int idSlot = tempId - 100;
                    while ((int)idxMap.size() <= idSlot) idxMap.push_back(-1);
                    idxMap[idSlot] = ctrl.index;
                }
                tempId++;
            }
        }
    }

    // --- .h文件: 声明 ---
    h_.emitLine("// P7: Win32 Form - " + formName);
    h_.emitBlank();

    // Fix 010c: 窗体/控件句柄变量移到.c文件, 避免跨窗体.h文件中static变量重定义(C2374)
    // 不同窗体可能有同名控件(如Text1), static变量在.h中被多次包含会导致重定义

    // 窗体句柄变量
    c_.emitLine("static void* vb6_hwnd_" + cIdent(formName) + " = NULL;");
    // Fix 086: 跨模块窗体默认实例访问器 — 其他模块 (cLogs 等) 引用
    // FLogs.Visible / Unload FLogs 时通过 vb6_form_hwnd_<Form>() 获取窗体HWND
    h_.emitLine("void* vb6_form_hwnd_" + cIdent(formName) + "(void);");
    c_.emitLine("void* vb6_form_hwnd_" + cIdent(formName) + "(void) { return vb6_hwnd_" + cIdent(formName) + "; }");
    // Fix 093a: 跨模块窗体默认实例 Show — 其他模块 `Frm.Show` 转发到本函数
    h_.emitLine("void vb6_form_show_" + cIdent(formName) + "(void* hMDIClient);");

    // 控件句柄变量 (P7.6: 数组控件使用vb6_CtrlArr, 非数组使用void*)
    {
        std::unordered_set<std::string> emitted;
        for (const auto& ctrl : frmDesc.formControl.children) {
            std::string ctrlNameLower = ctrl.controlName;
            std::transform(ctrlNameLower.begin(), ctrlNameLower.end(), ctrlNameLower.begin(), ::tolower);
            if (emitted.count(ctrlNameLower)) continue;
            if (!FrmParser::controlTypeToWin32Class(ctrl.controlType)) {
                // P7.9: WebBrowser needs HWND declaration though no Win32 class
                if (ctrl.controlType == FrmControlType::WebBrowser) {
                    c_.emitLine("static void* vb6_hwnd_" + cIdent(ctrl.controlName) + " = NULL;");
                    emitted.insert(ctrlNameLower);
                }
                // Fix 086: Timer控件无Win32窗口, 但窗体代码引用 vb6_hwnd_<Timer>
                // (Timer1.Enabled/Interval 走控件属性路径). 声明为NULL静态变量,
                // RTL 的 vb6_SetTimerEnabled/vb6_SetTimerInterval 对 NULL 是安全空操作.
                if (ctrl.controlType == FrmControlType::Timer) {
                    c_.emitLine("static void* vb6_hwnd_" + cIdent(ctrl.controlName) + " = NULL;");
                    emitted.insert(ctrlNameLower);
                }
                // ActiveX控件 (ImageList等): 生成IDispatch*变量, 运行时CoCreateInstance
                if (ctrl.controlType == FrmControlType::ImageList ||
                    ctrl.controlType == FrmControlType::Toolbar ||
                    ctrl.controlType == FrmControlType::StatusBar ||
                    ctrl.controlType == FrmControlType::CommonDialog) {
                    c_.emitLine("static void* vb6_com_" + cIdent(ctrl.controlName) + " = NULL;  /* IDispatch* */");
                    emitted.insert(ctrlNameLower);
                }
                continue;
            }
            if (knownControlArrays_.count(ctrlNameLower)) {
                c_.emitLine("static vb6_CtrlArr vb6_arr_" + cIdent(ctrl.controlName) + ";");
            } else {
                c_.emitLine("static void* vb6_hwnd_" + cIdent(ctrl.controlName) + " = NULL;");
            }
            emitted.insert(ctrlNameLower);
        }
    }

    c_.emitLine("static int vb6_formLoading_" + cIdent(formName) + " = 0;");

    c_.emitBlank();

    // WndProc前向声明
    h_.emitLine("LRESULT CALLBACK " + wndProc + "(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);");
    // 创建函数前向声明
    h_.emitLine("void " + createFn + "(void* hwnd, void* hInstance);");
    // Show函数前向声明
    h_.emitLine("void " + showFn + "(void* hMDIClient);");
    h_.emitBlank();
