// cgen_base_generate_entry.inc - generate 函数体片段：入口点生成
// 由 src/backend/cgen_base.cpp 拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 955~1101 行，逐行未改。

    // 生成入口点 (类模块不生成main; 多模块工程中仅有Sub Main的模块生成main)
    // P6.6: ActiveX DLL入口点统一由dll_entry.c生成, 不在各模块.c中生成
    if (!isClassModule_) {
        if (isDll_) {
            // P6.6: DLL模式下, 入口点由driver额外生成的dll_entry.c提供
            // 这里只生成vb6_Init/vb6_Exit的调用桩 (供dll_entry.c中的DllMain使用)
            // 不再调用emitActiveXDll()以避免符号重复定义
        } else {
            c_.emitBlank();
            c_.emitLine("// === 入口点 ===");
            bool hasMain = false;
        for (auto& decl : module.declarations) {
            if (decl->kind == ASTNodeKind::SubDecl) {
                auto& sub = static_cast<SubDecl&>(*decl);
                if (sub.name == "Main" && sub.access == AccessLevel::Public) {
                    hasMain = true;
                    break;
                }
            }
        }
        // Fix 142: 多模块工程中只有启动对象所在模块能生成进程入口点.
        // 此前四个窗体模块各生成一个 WinMain → LNK2005 "WinMain 已经在 X.obj 中定义".
        // 判定:
        //   Startup="Sub Main"  → 只有含 Public Sub Main 的模块 (hasMain) 生成;
        //   Startup="frmMain"   → 只有该窗体模块生成 (moduleName 或窗体名匹配);
        //   无 Startup 信息     → 单模块照旧; 多模块由 driver 兜底取第一个窗体.
        bool isStartupModule = true;
        if (isMultiModule_ && !startupObject_.empty()) {
            std::string so = startupObject_;
            std::transform(so.begin(), so.end(), so.begin(), ::tolower);
            if (so == "sub main") {
                isStartupModule = hasMain;
            } else {
                std::string modLower = module.moduleName;
                std::transform(modLower.begin(), modLower.end(), modLower.begin(), ::tolower);
                std::string frmLower = frmDesc ? frmDesc->formName : std::string();
                std::transform(frmLower.begin(), frmLower.end(), frmLower.begin(), ::tolower);
                isStartupModule = (so == modLower || (!frmLower.empty() && so == frmLower));
            }
        }

        // 单模块: 允许用第一个Public Sub作为入口
        // 多模块: 只有启动模块 (Sub Main / 启动窗体) 才生成main
        bool shouldGenMain = hasMain && isStartupModule;
        if (!shouldGenMain && (!isMultiModule_ || isStartupModule)) {
            // 单模块模式: 无Sub Main时自动找首个Public Sub
    for (auto& decl : module.declarations) {
        if (currentClassIsInterface) continue;  // P6.4: 接口类不生成方法实现体
        if (decl->kind == ASTNodeKind::SubDecl) {
                    auto& sub = static_cast<SubDecl&>(*decl);
                    if (sub.access == AccessLevel::Public) {
                        shouldGenMain = true;
                        // P7: 窗体模块生成WinMain, 标准模块生成main
                        if (module.isFormModule && frmDesc) {
                            // 窗体模块: WinMain → 显示窗体 → 消息循环
                            std::string formName = frmDesc->formName;
                            c_.emitLine("int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nCmdShow) {");
                            c_.indent();
                            c_.emitLine("(void)hPrevInst; (void)lpCmdLine; (void)nCmdShow;");
            c_.emitLine("vb6_Init();");
            if (!modInitFuncName.empty()) c_.emitLine(modInitFuncName + "();");
            // Fix 080: 多模块工程中, 调用所有外部模块的初始化函数
            if (isMultiModule_) {
                for (const auto& extMod : externalModules_) {
                    std::string extInitFunc = "vb6_mod_" + cIdent(extMod) + "_init";
                    c_.emitLine(extInitFunc + "();");
                }
            }
            c_.emitLine("vb6_SetAppInstance((void*)hInst);");
            c_.emitLine("vb6_form_show_" + cIdent(formName) + "(NULL, 0);  /* Show form modeless, NULL=hMDIClient */");
                            c_.emitLine("int ret = vb6_MessageLoop();");
                            c_.emitLine("vb6_Exit();");
                            c_.emitLine("return ret;");
                            c_.dedent();
                            c_.emitLine("}");
                        } else {
                            c_.emitLine("int main(int argc, char* argv[]) {");
                            c_.indent();
            c_.emitLine("vb6_Init();");
            if (!modInitFuncName.empty()) c_.emitLine(modInitFuncName + "();");
            // Fix 080: 多模块工程中, 调用所有外部模块的初始化函数
            if (isMultiModule_) {
                for (const auto& extMod : externalModules_) {
                    std::string extInitFunc = "vb6_mod_" + cIdent(extMod) + "_init";
                    c_.emitLine(extInitFunc + "();");
                }
            }
            c_.emitLine(cProcName(sub.name, sub.access) + "();");
            c_.emitLine("vb6_Exit();");
                            c_.emitLine("return 0;");
                            c_.dedent();
                            c_.emitLine("}");
                        }
                        break;
                    }
                }
            }
        }
        if (shouldGenMain && hasMain) {
            // P7: 窗体模块生成WinMain, 标准模块生成main
            if (module.isFormModule) {
                c_.emitLine("int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nCmdShow) {");
                c_.indent();
                c_.emitLine("(void)hPrevInst; (void)lpCmdLine; (void)nCmdShow;");
                c_.emitLine("vb6_Init();");
                // Fix 080: 多模块工程中, 调用所有外部模块的初始化函数
                if (isMultiModule_) {
                    for (const auto& extMod : externalModules_) {
                        std::string extInitFunc = "vb6_mod_" + cIdent(extMod) + "_init";
                        c_.emitLine(extInitFunc + "();");
                    }
                }
                if (!modInitFuncName.empty()) c_.emitLine(modInitFuncName + "();");
                c_.emitLine("vb6_SetAppInstance((void*)hInst);");
                c_.emitLine(cProcName("Main", AccessLevel::Public) + "();");
                c_.emitLine("int ret = vb6_MessageLoop();");
                c_.emitLine("vb6_Exit();");
                c_.emitLine("return ret;");
                c_.dedent();
                c_.emitLine("}");
            } else {
                c_.emitLine("int main(int argc, char* argv[]) {");
                c_.indent();
            c_.emitLine("vb6_Init();");
            if (!modInitFuncName.empty()) c_.emitLine(modInitFuncName + "();");
            // Fix 080: 多模块工程中, 调用所有外部模块的初始化函数
            if (isMultiModule_) {
                for (const auto& extMod : externalModules_) {
                    std::string extInitFunc = "vb6_mod_" + cIdent(extMod) + "_init";
                    c_.emitLine(extInitFunc + "();");
                }
            }
            c_.emitLine(cProcName("Main", AccessLevel::Public) + "();");
                c_.emitLine("vb6_Exit();");
                c_.emitLine("return 0;");
                c_.dedent();
                c_.emitLine("}");
            }
        } else if (!shouldGenMain) {
            // Fix 142: 非启动模块不生成入口点 (否则多窗体工程 LNK2005)
            if (isStartupModule && module.isFormModule && frmDesc) {
                // P7: 空窗体也要生成WinMain, 自动显示窗体
                std::string formName = frmDesc->formName;
                c_.emitLine("int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nCmdShow) {");
                c_.indent();
                c_.emitLine("(void)hPrevInst; (void)lpCmdLine; (void)nCmdShow;");
                c_.emitLine("vb6_Init();");
                if (!modInitFuncName.empty()) c_.emitLine(modInitFuncName + "();");
                // Fix 080: 多模块工程中, 调用所有外部模块的初始化函数
                if (isMultiModule_) {
                    for (const auto& extMod : externalModules_) {
                        std::string extInitFunc = "vb6_mod_" + cIdent(extMod) + "_init";
                        c_.emitLine(extInitFunc + "();");
                    }
                }
                c_.emitLine("vb6_SetAppInstance((void*)hInst);");
                c_.emitLine("vb6_form_show_" + cIdent(formName) + "(NULL, 0);  /* Show form modeless, NULL=hMDIClient */");
                c_.emitLine("int ret = vb6_MessageLoop();");
                c_.emitLine("vb6_Exit();");
                c_.emitLine("return ret;");
                c_.dedent();
                c_.emitLine("}");
            } else {
                c_.emitLine("// No entry point (library module)");
            }
        }
        }  // end else (EXE mode)
    }

