// driver_codegen_module_loop.inc - runCodeGeneration 函数体片段：逐模块代码生成主循环
// 由 src/driver/driver.cpp 拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 371~462 行，逐行未改。


    for (size_t i = 0; i < modules_.size(); i++) {
        auto& module = modules_[i];
        auto& analyzer = analyzers_[i];

        // 确定输出基名
        std::string baseName;
        if (modules_.size() == 1 && !options.outputFile.empty()) {
            // 单文件: 用输出文件名作为基名
            std::filesystem::path p(utf8ToPath(options.outputFile));
            baseName = pathToUtf8(p.stem());
        } else {
            // Fix 013: 多文件: 用 module.moduleName (VB_Name) 作为基名
            // 确保输出文件名与 #include 指令、跨模块 sourceModule 一致
            baseName = module->moduleName;
        }

        // 收集跨模块include需求（从符号表获取外部模块名）
        auto externalModules = analyzer->symbolTable().getExternalModuleNames();

        // Fix 075: 多模块项目中，所有模块的 isMultiModule_ 必须为 true，
        // 否则函数定义名不带模块前缀而跨模块调用带模块前缀，导致 LNK2019。
        // 将项目中所有其他模块名也加入 externalModules，确保 isMultiModule_=true。
        if (modules_.size() > 1) {
            for (size_t j = 0; j < modules_.size(); j++) {
                if (j != i) {
                    externalModules.insert(modules_[j]->moduleName);
                }
            }
        }

        // 调用C代码生成器
        // Fix 023: 传入 void* 字段表供 cgen_expr.cpp fallback 路径查询
        CCodeGen cgen(*diag_, analyzer->symbolTable(), analyzer->typeSystem(),
                      &classVoidFieldMap, &classTypedFieldMap, &variantReturnFuncs, options.verbose);
        cgen.setFormModuleNames(formModuleNames);  // Fix 086: 跨模块窗体默认实例引用
        cgen.setModulePublicConsts(&modulePublicConsts);  // Fix 086: 跨模块常量内联
        cgen.setStaticModuleVarNames(&dupModuleVarNames);  // Fix 093b: 撞名模块级变量转 static
        cgen.setTrimIncludes(options.trimIncludes);  // opt4: 裁剪未实际引用的跨模块include
        if (options.trimIncludes) {
            // opt4: 基于AST实际引用收集外部模块, 供 include 裁剪
            std::unordered_set<std::string> refdModules;
            ExternalRefCollector collector(analyzer->symbolTable(), externalModules, refdModules);
            traverseAST(*module, collector);
            cgen.setTrimModules(refdModules);
        }

        // 传入模块基名和外部模块列表
        // P6.6: 传递ActiveX DLL模式信息
        // P7: 传递窗体描述 (仅.frm模块有效)
        const FrmFormDesc* frmDescPtr = nullptr;
        auto frmIt = frmFiles_.find(module->moduleName);
        if (frmIt != frmFiles_.end()) {
            frmDescPtr = &frmIt->second.form;
        }
        bool ok = cgen.generate(*module, baseName, externalModules,
                                options.isDll, options.dllProgId, frmDescPtr);
        if (!ok) return false;

        // 写 .h 文件
        std::string hPath = outputDir + "/" + baseName + ".h";
        {
            std::ofstream ofs(hPath, std::ios::out | std::ios::trunc);
            if (!ofs) {
                std::cerr << "C3: 无法写入文件: " << hPath << std::endl;
                return false;
            }
            ofs << cgen.headerCode();
        }

        // 写 .c 文件
        std::string cPath = outputDir + "/" + baseName + ".c";
        {
            std::ofstream ofs(cPath, std::ios::out | std::ios::trunc);
            if (!ofs) {
                std::cerr << "C3: 无法写入文件: " << cPath << std::endl;
                return false;
            }
            ofs << cgen.sourceCode();
        }

        if (options.verbose) {
            std::cout << "C3: 生成 " << hPath << " (" << cgen.headerCode().size() << " bytes)" << std::endl;
            std::cout << "C3: 生成 " << cPath << " (" << cgen.sourceCode().size() << " bytes)" << std::endl;
        }

        // --emit-c 模式: 输出C代码后结束
        if (options.emitC) {
            std::cout << cgen.headerCode() << std::endl;
            std::cout << cgen.sourceCode() << std::endl;
        }
    }
