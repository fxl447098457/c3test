// cgen_base_generate_crossmod.inc - generate 函数体片段：跨模块注册与定义顺序（Fix 010r-11 / 010r-12 / P8.7）
// 由 src/backend/cgen_base.cpp 拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 315~384 行，逐行未改。

    // Fix 010r-11: 注册外部模块的类实例变量到 knownClassVars_
    // 遍历符号表中所有外部 Variable 符号, 检查 variableTypeName 是否是类名
    // 如 ToolsStr As New cToolsStr → variableTypeName = "cToolsStr"
    // 这样在 MemberAccessExpr 中, ToolsStr.HasStr 能正确分发为 vb6_cToolsStr_HasStr(ToolsStr, ...)
    for (const auto& [key, sym] : symTab_.moduleScope()->symbols()) {
        if (sym->isExternal && sym->kind == SymbolKind::Variable && !sym->variableTypeName.empty()) {
            std::string varLower = sym->name;
            std::transform(varLower.begin(), varLower.end(), varLower.begin(), ::tolower);
            auto* clsSym = lookupModuleDotted(sym->variableTypeName);
            if (clsSym && clsSym->kind == SymbolKind::Class && !clsSym->isInterface) {
                knownClassVars_[varLower] = clsSym->name;
                // P14.3.1: Dim As New 自动实例化 — 外部 As New 变量也需要注册
                // 检查源Symbol是否有 As New 标志 (通过 knownNewVars_ 传递)
                // 对于 Public x As New ClassName, 在 consuming 模块中也需要 auto-instantiate
                // 暂时不注册 knownNewVars_, 因为 As New 信息没有传递到外部 Symbol
            }
            // Fix 089: 注册外部模块的 COM/内置对象模块级变量 (C 类型 void*)
            // 此前的注册只覆盖了"项目类实例变量" (knownClassVars_). 对 .bas 标准模块中
            // Public timers As New Collection / Public x As Object 这类跨模块变量,
            // consuming 类模块的 knownObjectVars_ 为空, 导致 timers.Add(...) 落入通用
            // fallback 生成结构成员调用 (void* 上 .member) → C2224. 本分支将可判定为
            // COM 对象 (sym->type == Object) 的外部变量注册到对应 COM 跟踪集合:
            //   - ComClass / 接口 → knownTypedComVars_ / knownIfaceVars_ (与 cgen_decl
            //     visit(VariableDecl) 的本地注册规则一致)
            //   - 内置对象 (Collection/ErrObject 等) / 其它 Object → knownObjectVars_
            //     (后期绑定 COM dispatch, vb6_ComCall)
            else if (sym->type == Vb6Type::Object) {
                if (clsSym && clsSym->kind == SymbolKind::ComClass) {
                    knownTypedComVars_[varLower] = clsSym;
                } else if (clsSym && clsSym->kind == SymbolKind::Class && clsSym->isInterface) {
                    knownIfaceVars_[varLower] = clsSym->name;
                } else {
                    knownObjectVars_.insert(varLower);
                }
            }
        }
    }

    // Fix 010r-12: Enum和Type(UDT)定义必须在跨模块#include之前生成
    // 原因: 类模块(如cTlsSocket)的.h需要标准模块(如mdTlsThunks)的UDT完整定义
    // 作为类结构体值成员. 若UDT定义在#include之后, 循环引用会导致UDT不可见.
    // UDT不引用类类型(只含基本类型/其他UDT/指针), 可安全地先输出.
    // 1. Enum定义 (必须最先, 因为常量可能引用枚举值)
    for (auto& decl : module.declarations) {
        if (decl->kind == ASTNodeKind::EnumDecl) {
            visit(static_cast<EnumDecl&>(*decl));
        }
    }

    // 2. Type(UDT)定义 — 必须在跨模块#include之前, 确保循环引用时UDT定义先于include可见
    for (auto& decl : module.declarations) {
        if (decl->kind == ASTNodeKind::TypeDecl) {
            visit(static_cast<TypeDecl&>(*decl));
        }
    }

    // P8.7 + Fix 010r-12: 跨模块#include策略
    // - 类模块引用任何模块(类或标准) → .h (类结构体需要完整UDT定义和类类型)
    // - 标准模块引用类模块 → .h (函数参数需要完整类类型)
    // - 标准模块引用标准模块 → .c (避免.h之间循环依赖)
    // UDT定义已在上方输出, 循环#include时UDT定义总是先于include可见
    for (const auto& extMod : externalModules) {
        auto* extSym = symTab_.lookup(extMod);
        bool extIsClass = extSym && extSym->kind == SymbolKind::Class;
        if (extIsClass || isClassModule_) {
            h_.emitLine("#include \"" + extMod + ".h\"");
        }
    }
    h_.emitBlank();

