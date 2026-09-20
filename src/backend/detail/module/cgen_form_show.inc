// cgen_form_show.inc - emitFormFramework 函数体片段：Show 函数
// 由 src/backend/module/cgen_form.cpp 拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 1938~1996 行，逐行未改。

    // --- Show函数 (P7.7: 支持MDIForm/MDIChild) ---
    // Fix 146: Owner — 非启动窗体以"当前活动窗体"为 Owner.
    // Owner 为 NULL 时模态窗体无法禁用调用者: 主窗体仍可点击, 于是能在模态
    // 循环打开期间重入打开第二个模态窗体 (嵌套 → 崩溃). VB6 的 Form.Show
    // 语义就是把调用窗体设为 Owner.
    std::string ownerExpr = "NULL";
    if (!startupObject_.empty()) {
        std::string so146 = Symbol::toLower(startupObject_);
        std::string fn146 = Symbol::toLower(formName);
        std::string md146 = Symbol::toLower(moduleName_);
        if (so146 != fn146 && so146 != md146 && so146 != "sub main") {
            ownerExpr = "vb6_Forms_GetActive()";
        }
    }
    c_.emitLine("// === " + formName + " Show ===");
    c_.emitLine("void " + showFn + "(void* hMDIClient, int modal) {");
    c_.indent();
    c_.emitLine("(void)hMDIClient;");
    c_.emitLine("void* hInst = vb6_GetAppInstance();");
    if (isMDIForm) {
        c_.emitLine("vb6_RegisterMDIFormClass(\"" + clsName + "\", (void*)" + wndProc + ", hInst, 0);");
        c_.emitLine("vb6_hwnd_" + cIdent(formName) + " = vb6_CreateMDIFormWindow(");
        c_.indent();
        c_.emitLine("\"" + clsName + "\", \"" + caption + "\",");
        if (startupPos == 2) {
            // M22-Issue2: StartUpPosition=2 CenterScreen
            c_.emitLine("-1, -1,  /* M22-Issue2: CenterScreen, RTL handles pixel calc */");
        } else if (startupPos == 3) {
            c_.emitLine("CW_USEDEFAULT, CW_USEDEFAULT,");
        } else {
            c_.emitLine("0, 0,");
        }
        c_.emitLine(std::to_string(clientWidth) + ", " + std::to_string(clientHeight) + ",");
        c_.emitLine("hInst);");
        c_.dedent();
    } else if (isMDIChild) {
        if (formBackColor >= 0) {
            // czUI fix: 窗体级 BackColor → 类背景刷 + Ambient.BackColor
            c_.emitLine("vb6_RegisterFormClassBg(\"" + clsName + "\", (void*)" + wndProc + ", hInst, 0, " + std::to_string(formBackColor) + ");");
        } else {
            c_.emitLine("vb6_RegisterFormClass(\"" + clsName + "\", (void*)" + wndProc + ", hInst, 0);");
        }
        c_.emitLine("vb6_hwnd_" + cIdent(formName) + " = vb6_CreateMDIChildWindow(");
        c_.indent();
        c_.emitLine("\"" + clsName + "\", \"" + caption + "\",");
        if (startupPos == 2) {
            // M22-Issue2: StartUpPosition=2 CenterScreen
            c_.emitLine("-1, -1,  /* M22-Issue2: CenterScreen, RTL handles pixel calc */");
        } else if (startupPos == 3) {
            c_.emitLine("CW_USEDEFAULT, CW_USEDEFAULT,");
        } else {
            c_.emitLine("0, 0,");
        }
        c_.emitLine(std::to_string(clientWidth) + ", " + std::to_string(clientHeight) + ",");
        c_.emitLine("hMDIClient, hInst);");
        c_.dedent();
    } else {
        if (formBackColor >= 0) {
            // czUI fix: 窗体级 BackColor → 类背景刷 + Ambient.BackColor
            c_.emitLine("vb6_RegisterFormClassBg(\"" + clsName + "\", (void*)" + wndProc + ", hInst, 0, " + std::to_string(formBackColor) + ");");
        } else {
            c_.emitLine("vb6_RegisterFormClass(\"" + clsName + "\", (void*)" + wndProc + ", hInst, 0);");
        }
        c_.emitLine("vb6_hwnd_" + cIdent(formName) + " = vb6_CreateFormWindowB(");
        c_.indent();
        c_.emitLine("\"" + clsName + "\", \"" + caption + "\",");
        if (startupPos == 2) {
            // M22-Issue2: StartUpPosition=2 CenterScreen
            c_.emitLine("-1, -1,  /* M22-Issue2: CenterScreen, RTL handles pixel calc */");
        } else if (startupPos == 3) {
            c_.emitLine("CW_USEDEFAULT, CW_USEDEFAULT,");
        } else {
            c_.emitLine("0, 0,");
        }
        c_.emitLine(std::to_string(clientWidth) + ", " + std::to_string(clientHeight) + ",");
        c_.emitLine("hInst, " + ownerExpr + ", " + std::to_string(formBorderStyle) + ");");
        c_.dedent();
    }
    c_.emitLine("vb6_ShowForm(vb6_hwnd_" + cIdent(formName) + ", modal);");
    c_.dedent();
    c_.emitLine("}");  // Show
    c_.emitBlank();