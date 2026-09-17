// driver_codegen_dup_module_vars.inc - runCodeGeneration 函数体片段：Fix 093b 跨模块同名模块级变量
// 由 src/driver/driver.cpp 拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 342~370 行，逐行未改。

    // P6.4+: 预扫描 — 构建 接口 → 唯一实现类名 映射.
    // 遍历所有类模块的 Class 符号的 implementsNames, 反向记录每个接口被哪些
    // 类实现. 恰好一个实现类 → 记录其规范名; 多于一个 → 置空 (cgen 依赖唯一
    // 实现类的场景报错而非静默误编). 供 Set 接口引用赋值类型转换与 ByVal
    // 接口形参回调重建 (SUBCLASSPROC ABI) 使用.
    std::map<std::string, std::string> ifaceImplementersMap;
    for (size_t i = 0; i < modules_.size(); i++) {
        if (!modules_[i]->isClassModule) continue;
        Symbol* clsSym = analyzers_[i]->symbolTable().lookupModule(modules_[i]->moduleName);
        if (!clsSym || clsSym->kind != SymbolKind::Class) continue;
        if (clsSym->implementsNames.empty()) continue;
        for (const auto& iface : clsSym->implementsNames) {
            std::string ifLower = iface;
            std::transform(ifLower.begin(), ifLower.end(), ifLower.begin(), ::tolower);
            auto it = ifaceImplementersMap.find(ifLower);
            if (it == ifaceImplementersMap.end()) {
                ifaceImplementersMap[ifLower] = clsSym->name;
            } else if (it->second != clsSym->name) {
                it->second.clear();  // 多实现类 → 不可推断
            }
        }
    }

    // P6.4+: 预扫描 — 类模块默认实例 (Attribute VB_PredeclaredId = True).
    // VB6 会为这类类模块生成隐藏的全局默认实例, frm 里裸写 cTT.CreateToolTip(...)
    // 就是调用它. 记录 小写类名 → 类规范名, 供 cgen 解析裸类名成员访问并生成
    // vb6_cls_X_Default() 惰性单例访问器.
    std::map<std::string, std::string> defaultInstanceClasses;
    for (size_t i = 0; i < modules_.size(); i++) {
        if (!modules_[i]->isClassModule) continue;
        for (const auto& attr : modules_[i]->attributes) {
            std::string aName = attr->attrName;
            std::transform(aName.begin(), aName.end(), aName.begin(), ::tolower);
            if (aName != "vb_predeclaredid") continue;
            bool isTrue = false;
            if (attr->value && attr->value->kind == ASTNodeKind::LiteralExpr) {
                auto& lit = static_cast<const LiteralExpr&>(*attr->value);
                if (lit.literalKind == LiteralKind::Boolean) isTrue = lit.boolValue;
                else if (lit.literalKind == LiteralKind::Integer) isTrue = (lit.intValue != 0);
            }
            if (isTrue) {
                std::string lower = modules_[i]->moduleName;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                defaultInstanceClasses[lower] = modules_[i]->moduleName;
            }
            break;
        }
    }

    // Fix 093b: 预扫描 — 找出"跨模块同名模块级变量".
    // VB6 允许不同模块各自声明同名模块级变量 (模块内引用绑定自身副本, 跨模块引用
    // 必须写 "模块名.变量名"), 但 C 语言下两个模块会生成同名全局符号 → LNK2005.
    // 本工程实例:
    //   Tools.bas:  Public Delay As New cDelay   /  mDelay.bas: Public Delay As New cDelay
    //   FToastDrawer.frm: Dim Sad/m_Theme/m_State/PosVal/TopVal/m_ParentToast/m_TagName
    //   FToastCenter.frm: 同名 Dim 一组
    // 命中的名字在生成端强制 static 内部链接 (见 CCodeGen::isPublicModuleDecl):
    // 各模块保留自己的副本, 语义与 VB6 一致, 且不再产生重复符号.
    std::unordered_map<std::string, std::unordered_set<std::string>> moduleVarOwners;
    for (size_t i = 0; i < modules_.size(); i++) {
        // 类模块的成员变量是结构体字段 (me->x), 不产生全局符号; 不能参与撞名判定
        // (否则 cVBMAN.cls 的 Public ToolsUtf8 会让 Tools.bas 的 Public ToolsUtf8
        //  被误判为撞名 → 强制 static → 跨模块引用 cHttpServer.c 等 C2065).
        if (modules_[i]->isClassModule) continue;
        std::string modLower = modules_[i]->moduleName;
        std::transform(modLower.begin(), modLower.end(), modLower.begin(), ::tolower);
        for (const auto& decl : modules_[i]->declarations) {
            if (decl->kind != ASTNodeKind::VariableDecl) continue;
            auto& vd = static_cast<const VariableDecl&>(*decl);
            std::string lower = vd.name;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            moduleVarOwners[lower].insert(modLower);
        }
    }
    std::unordered_set<std::string> dupModuleVarNames;
    for (const auto& [varName, owners] : moduleVarOwners) {
        if (owners.size() > 1) dupModuleVarNames.insert(varName);
    }
