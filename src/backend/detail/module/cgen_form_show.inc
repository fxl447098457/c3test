// cgen_form_show.inc - emitFormFramework 函数体片段：Show 函数
// 由 src/backend/module/cgen_form.cpp 拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 1938~1996 行，逐行未改。

    // --- Show函数 (P7.7: 支持MDIForm/MDIChild) ---
    c_.emitLine("// === " + formName + " Show ===");
    c_.emitLine("void " + showFn + "(void* hMDIClient) {");
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
        c_.emitLine("vb6_RegisterFormClass(\"" + clsName + "\", (void*)" + wndProc + ", hInst, 0);");
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
        c_.emitLine("vb6_RegisterFormClass(\"" + clsName + "\", (void*)" + wndProc + ", hInst, 0);");
        c_.emitLine("vb6_hwnd_" + cIdent(formName) + " = vb6_CreateFormWindow(");
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
        c_.emitLine("hInst, NULL);");
        c_.dedent();
    }
    c_.emitLine("vb6_ShowForm(vb6_hwnd_" + cIdent(formName) + ", 0);");
    c_.dedent();
    c_.emitLine("}");  // Show
    c_.emitBlank();