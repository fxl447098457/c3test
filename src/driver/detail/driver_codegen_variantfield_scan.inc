// driver_codegen_variantfield_scan.inc - runCodeGeneration 函数体片段：
// 账 #305 §B168 工程级「类 → vb6_VARIANT 字段」表 (第五十九刀)
// 在函数体内被 #include, 非独立编译单元。位置: typedfield_scan 之后 —— 与
// classVoidFieldMap / classTypedFieldMap 同一趟预扫描, 同一批模块清单。

    // 账 #305 §B168: className(小写) → 该类结构体里 C 类型为 vb6_VARIANT 的**标量**字段集
    // (原名小写 + "m_" 前缀小写两种拼法都记, 与 classVoidFieldMap_ 同格).
    //
    // 为什么要有它: 赋值站判「这个左值到底是不是 Variant 容器」的四条既有判据 —— 裸名
    // knownVariantVars_ / 本模块 classVariantMembers_ / 文本 `VB6_SA_*(vb6_VARIANT,` /
    // UDT 字段类型 —— 一条都认不出 `<别的对象>.<公共 Variant 字段>`, 于是整句**完全不装箱**
    // 裸发 (实测 use3.bas + holder2.cls):
    //   h.PV = 7    → `h->PV = 7;`                      → cl C2440 "无法从 int32_t 转换为 vb6_VARIANT"
    //   h.PV = "s"  → `h->PV = vb6_BSTR_FromStr(L"s");` → 同形 C2440
    // 类模块**内**的 `Me.PV = x` 走的是同一条路 (发 `me->PV = (*x);`, 既不装箱也不回收
    // 上一份带权值)。C2440 是硬错误 ⇒ 整棵工程编不过, 语料实测 94 个写点 / 28 份文件。
    //
    // 为什么这张表必须是**工程级**: 字段的 C 类型由**那个类自己的声明**决定, 与谁在写它
    // 无关; 后端每模块一张 SymbolTable, 既看不见别类的字段, 也会被同名成员遮蔽
    // (说明见 driver_codegen_voidfield_scan.inc 的 projectClassNames 注释)。
    // 键取**小写**类名 (比 void/typed 两张表严一点): 调用侧交来的类名来自
    // variableTypeName / knownClassVars_, 拼法与模块名的大小写不保证一致, 而字段名
    // 本来就是小写存的 —— 这张表答"有没有", 不该由拼法决定。
    //
    // 判据 = "类模块发 struct 时这条字段写的是不是 vb6_VARIANT", 与 mapTypeRef 同源
    // (isVoidFieldType 用的也是这份内置类型表):
    //   无 As → VB6 默认 Variant (Fix 089j 口径), C 字段是 vb6_VARIANT;
    //   SimpleTypeRef 且 resolveTypeName == Variant → vb6_VARIANT;
    //   其余都不是槽: 数组 (Public PA(2) As Variant 的 C 字段是 vb6_SafeArray1D*,
    //   元素赋值是另一格账) / String·定长串 (BSTR) / 项目类·COM·未知对象名
    //   (void* 或类指针, 即 Fix 090bx 那批漏网名字) / UDT (vb6_type_x)。
    std::unordered_map<std::string, std::set<std::string>> classVariantFieldMap;
    auto isVariantSlotFieldType = [](const VariableDecl& var,
                                     const TypeSystem& typeSys) -> bool {
        if (var.isDynamicArray || !var.dimensions.empty()) return false;
        if (!var.asType) return true;
        if (var.asType->kind != ASTNodeKind::SimpleTypeRef) return false;
        return typeSys.resolveTypeName(
                   static_cast<const SimpleTypeRef*>(var.asType.get())->name)
               == Vb6Type::Variant;
    };
    for (size_t i = 0; i < modules_.size(); i++) {
        const auto& module = modules_[i];
        // 类模块和窗体模块有结构体字段; 标准模块无 struct, 不需要扫描。
        if (!module->isClassModule && !module->isFormModule) continue;
        if (module->moduleName.empty()) continue;
        std::set<std::string> variantFields;
        for (const auto& decl : module->declarations) {
            if (decl->kind != ASTNodeKind::VariableDecl) continue;
            const auto& var = static_cast<const VariableDecl&>(*decl);
            if (!isVariantSlotFieldType(var, analyzers_[i]->typeSystem())) continue;
            std::string oLower = Symbol::toLower(var.name);
            variantFields.insert(oLower);
            variantFields.insert("m_" + oLower);
        }
        if (!variantFields.empty()) {
            classVariantFieldMap[Symbol::toLower(module->moduleName)]
                = std::move(variantFields);
        }
    }
