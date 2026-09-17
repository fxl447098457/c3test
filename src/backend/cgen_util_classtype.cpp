#include "backend/cgen.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <functional>

namespace vb6c3 {

// --- cgen_util_classtype.cpp: 类/UDT 类型推断与字段 C 类型 ---


std::string CCodeGen::inferClassTypeOfExpr(const ASTNode& expr) const {
    switch (expr.kind) {
        case ASTNodeKind::IdentifierExpr: {
            // base case: 变量 → knownClassVars_
            auto& id = static_cast<const IdentifierExpr&>(expr);
            std::string lower = Symbol::toLower(id.name);
            auto it = knownClassVars_.find(lower);
            if (it != knownClassVars_.end()) return it->second;
            if (std::getenv("C3_DBG110") && lower == "usercontrol") {
                const Symbol* s2 = symTab_.lookup(id.name);
                std::fprintf(stderr, "[DBG110] inferClassTypeOfExpr(UserControl) mod=%s sym=%p kind=%d vtn=%s\n",
                             moduleName_.c_str(), (const void*)s2, s2 ? (int)s2->kind : -1,
                             s2 ? s2->variableTypeName.c_str() : "<null>");
            }
            // Fix 084g: 局部变量/参数声明为 As ClassName (如 Dim Response As cHttpServerResponse)
            // 不在 knownClassVars_ (跨模块类变量表) 中, 从符号表 variableTypeName 推断类名
            const Symbol* sym = symTab_.lookup(id.name);
            if (sym && (sym->kind == SymbolKind::Variable || sym->kind == SymbolKind::Parameter)
                && !sym->variableTypeName.empty()) {
                const Symbol* clsSym = symTab_.lookup(sym->variableTypeName);
                if (clsSym && clsSym->kind == SymbolKind::Class) {
                    return sym->variableTypeName;
                }
            }
            // Fix 084z-2: 当前模块的属性 (Property Get) 返回类 — 属性符号在模块
            // 作用域符号表中 (如 cTlsRemaster.pvSocket() As cTlsSocket). 无参属性
            // 可推断类实例, 用于 pvSocket.SyncReceiveArray(...) 的方法/形参签名解析
            // (否则 Fix 033 把属性名当模块名, 回退 lookupModule 命中 storageKey
            // 同名冲突的错误类 cWinsock.SyncReceiveArray → C2197 参数过多).
            if (sym && sym->kind == SymbolKind::PropertyGet && !sym->variableTypeName.empty()) {
                const Symbol* clsSym = symTab_.lookup(sym->variableTypeName);
                if (clsSym && clsSym->kind == SymbolKind::Class) {
                    return sym->variableTypeName;
                }
            }
            return "";
        }
        case ASTNodeKind::IndexOrCallExpr: {
            // recursive case: 类方法调用 obj.Method(args) → 返回类
            auto& call = static_cast<const IndexOrCallExpr&>(expr);
            if (!call.callee) return "";
            if (call.callee->kind == ASTNodeKind::IdentifierExpr) {
                // Fix 085c: 模块内裸函数调用返回类实例 (如 cAsyncSocket 内
                // pvToSocket(idx) As cAsyncSocket), 后续 .frNotifyGetHostByName(...)
                // 链式调用需要知道返回类以拆成 vb6_cAsyncSocket_frNotify...(this,...).
                // 此前仅支持 MemberAccessExpr/WithMemberExpr callee, 裸函数推断断链 →
                // 生成 (ret).Method(...) 非法字段访问 (C2039: 不是 vb6_cls_X 的成员).
                auto& id = static_cast<const IdentifierExpr&>(*call.callee);
                const Symbol* fn = symTab_.lookupModule(id.name);
                if (fn && fn->kind == SymbolKind::Function
                    && !fn->variableTypeName.empty()) {
                    const Symbol* clsSym = symTab_.lookupModule(fn->variableTypeName);
                    if (clsSym && clsSym->kind == SymbolKind::Class) return fn->variableTypeName;
                }
                return "";
            }
            if (call.callee->kind != ASTNodeKind::MemberAccessExpr
                && call.callee->kind != ASTNodeKind::WithMemberExpr) return "";
            // Fix 085b: With 块内方法链 .Data(...).CalculateCRC16(...) — callee 是
            // WithMemberExpr, 基类是 With 栈顶对象类, 不能按 MemberAccessExpr 解析
            // (否则 With 链方法在推断中断链, 生成 (ret).Method(...) 非法字段调用).
            if (call.callee->kind == ASTNodeKind::WithMemberExpr) {
                auto& wm = static_cast<const WithMemberExpr&>(*call.callee);
                if (withObjectInfoStack_.empty()) return "";
                const auto& winfo = withObjectInfoStack_.back();
                if (winfo.kind != WithObjKind::ClassInstance || winfo.className.empty()) return "";
                return getClassMethodReturnType(winfo.className, wm.memberName);
            }
            auto& ma = static_cast<const MemberAccessExpr&>(*call.callee);
            if (!ma.object) return "";
            // 递归推断对象表达式的类名
            std::string baseClassName = inferClassTypeOfExpr(*ma.object);
            if (baseClassName.empty()) return "";
            // 查找该方法的返回类型
            return getClassMethodReturnType(baseClassName, ma.memberName);
        }
        // Fix 037: MeExpr → 类模块内 me 即当前类
        case ASTNodeKind::MeExpr: {
            if (isClassModule_) return moduleName_;
            return "";
        }
        // Fix 037b: MemberAccessExpr → 递归推断 object 的类类型, 再从
        // classTypedFieldMap_ 查找字段的类类型 (仅项目类字段, 非 COM).
        // 用于链式访问 ctx.Request.QueryString(idx) 中 Request 的类型推断:
        //   ctx (cHttpServerContext) → Request (cHttpServerRequest) → QueryString (COM:Dictionary)
        case ASTNodeKind::MemberAccessExpr: {
            auto& ma = static_cast<const MemberAccessExpr&>(expr);
            if (!ma.object) return "";
            std::string baseClassName = inferClassTypeOfExpr(*ma.object);
            if (baseClassName.empty()) return "";
            if (classTypedFieldMap_) {
                auto it = classTypedFieldMap_->find(baseClassName);
                if (it != classTypedFieldMap_->end()) {
                    std::string memLower = Symbol::toLower(ma.memberName);
                    auto itF = it->second.find(memLower);
                    if (itF != it->second.end()) {
                        // 仅返回项目类字段类型 (COM: 前缀的不是项目类)
                        if (itF->second.compare(0, 4, "COM:") == 0) return "";
                        return itF->second;
                    }
                }
            }
            // Fix 084z: 属性 Get 回退 — 成员是属性(返回类实例)而非数据字段时
            // (如 pvSocket 是 cTlsReMaster 的 Property Get, 不在字段表中),
            // 用 getClassMethodReturnType 推断返回类. 否则调用方类型推断失败
            // → fallback 到错误类解析参数, 生成 C2197/C2198 (参数过多/过少:
            // SyncReceiveArray 声明6参却按 cWinsock 的12参展开, Connect 声明
            // 11参却按 cWinsock 的5参展开).
            return getClassMethodReturnType(baseClassName, ma.memberName);
        }
        // Fix 037: WithMemberExpr → 当前 With 块 tempVar 的类类型 (仅 ClassInstance kind)
        case ASTNodeKind::WithMemberExpr: {
            if (withObjectInfoStack_.empty() || withObjectVars_.empty()) return "";
            const auto& info = withObjectInfoStack_.back();
            if (info.kind == WithObjKind::ClassInstance && !info.className.empty()) {
                // Fix 090s: .X 若是 With 目标类的 typed 项目类字段 (如
                // With HttpSvr: .Router.Reg → .Router 字段 As cHttpServerRouter),
                // 返回字段的类, 供外层 .Reg/.Encode 等成员/方法按字段类解析
                // (findClassMemberCallParams/resolveClassMemberCall 用对类, 否则
                // 形参表空 → .Router.Reg "Test" 实参全丢 C2198). 与 MemberAccessExpr
                // 分支 (classTypedFieldMap_ 查询) 对齐.
                auto& wmRef = static_cast<const WithMemberExpr&>(expr);
                if (classTypedFieldMap_) {
                    auto it = classTypedFieldMap_->find(info.className);
                    if (it != classTypedFieldMap_->end()) {
                        std::string memLower = Symbol::toLower(wmRef.memberName);
                        auto itF = it->second.find(memLower);
                        if (itF != it->second.end()) {
                            // COM:/void* 字段 (COM: 前缀) 不是项目类 → 返回空让外层
                            // 走 COM dispatch; 项目类字段返回类名
                            if (itF->second.compare(0, 4, "COM:") == 0) return "";
                            return itF->second;
                        }
                    }
                }
                // .X 非数据字段 (方法/属性等) → 维持原行为: With 目标类自身
                // (Fix 085b 在调用链推断处对 callee=WithMemberExpr 已按方法返回类
                // 特判, 此处不做方法返回类型推断以免误伤 String/Long 属性场景)
                return info.className;
            }
            return "";
        }
        default:
            return "";
    }
}


// Fix 037: 递归推断表达式的 UDT C 类型标识符 (如 "vb6_type_UcsBuffer").
// 支持 IdentifierExpr (knownUdtVars_ 直查) 和 MemberAccessExpr (嵌套 UDT 字段递归).
// 返回空串表示非 UDT 表达式.
std::string CCodeGen::inferUdtTypeOfExpr(const ASTNode& expr) const {
    switch (expr.kind) {
        case ASTNodeKind::IdentifierExpr: {
            auto& id = static_cast<const IdentifierExpr&>(expr);
            std::string lower = Symbol::toLower(id.name);
            auto it = knownUdtVars_.find(lower);
            if (it != knownUdtVars_.end()) return it->second;
            // Fix 090j: 函数体内函数名标识符 = 本函数返回对象 (VB6: Function
            // pvVfsOpen As ZipVfsType 内写 pvVfsOpen.BufferArray, 即返回 UDT 的
            // 字段). 与发射层 Fix 084z-4/088d (函数名→类返回对象) 及注册层
            // Fix 090i (vb6_ret_X → knownUdtVars_) 对称: 推断层必须把 "函数名"
            // 映射到返回 UDT 类型, 否则字段类型推断落空 → 字段整体按 Unknown:
            // Variant 字段被当函数 (SourceFileInfo(3) → C2064)、LongPtr 字段被
            // Variant 化 (BufferPtr = BufferBase → VariantToLong → C2440),
            // As Any 实参把 Variant 字段强转指针 (C2440) — cZipArchive VFS 簇.
            if (currentProc_ && !currentReturnCType_.empty()
                && currentReturnCType_.rfind("vb6_type_", 0) == 0
                && lower == Symbol::toLower(currentProc_->name)) {
                return currentReturnCType_;
            }
            return "";
        }
        // Fix 081i: IndexOrCallExpr — UDT数组元素访问 arr(idx).field
        // 查 arrayUdtElemTypes_ 获取数组元素UDT类型
        case ASTNodeKind::IndexOrCallExpr: {
            auto& call = static_cast<const IndexOrCallExpr&>(expr);
            if (call.callee && call.callee->kind == ASTNodeKind::IdentifierExpr) {
                auto& id = static_cast<const IdentifierExpr&>(*call.callee);
                std::string lower = Symbol::toLower(id.name);
                auto it = arrayUdtElemTypes_.find(lower);
                if (it != arrayUdtElemTypes_.end()) return it->second;
            }
            // Fix 110c: UDT 数组的**成员**数组 — m_Serie(i).Rects(j).
            // callee 是 MemberAccessExpr(父对象, 成员名), 成员本身是 UDT 数组
            // (如 tSerie.Rects() As RectL) → 元素类型 = 成员的 UDT 类型.
            // 递归推断 callee 的 UDT 类型即可 (tSerie → Rects → RectL).
            if (call.callee && call.callee->kind == ASTNodeKind::MemberAccessExpr) {
                std::string elemUdt = inferUdtTypeOfExpr(*call.callee);
                if (!elemUdt.empty()) return elemUdt;
            }
            // Fix 110s: With 块内的成员 UDT 数组 — With .Rects(j).
            // callee 是 WithMemberExpr (当前 With 对象的字段), 字段本身是 UDT 数组
            // (如 tSerie.Rects() As RectF) → 元素类型 = 字段的 UDT 类型.
            // 此前只处理 MemberAccessExpr, WithMemberExpr 落空 → With 临时变量退化为
            // void* → `(void*)VB6_SA_AT(vb6_type_RectF, ...)` C2440 (ucTreeMaps.c 1878).
            if (call.callee && call.callee->kind == ASTNodeKind::WithMemberExpr) {
                std::string elemUdt = inferUdtTypeOfExpr(*call.callee);
                if (!elemUdt.empty()) return elemUdt;
            }
            return "";
        }
        case ASTNodeKind::MemberAccessExpr: {
            auto& ma = static_cast<const MemberAccessExpr&>(expr);
            if (!ma.object) return "";
            // 递归推断父对象的 UDT 类型
            std::string parentUdtCType = inferUdtTypeOfExpr(*ma.object);
            if (parentUdtCType.empty()) return "";
            // parentUdtCType 形如 "vb6_type_UcsBuffer", 剥前缀得到 UDT 名
            const std::string prefix = "vb6_type_";
            if (parentUdtCType.size() <= prefix.size()
                || parentUdtCType.compare(0, prefix.size(), prefix) != 0) return "";
            std::string udtName = parentUdtCType.substr(prefix.size());
            Symbol* udtSym = symTab_.lookupModule(udtName);
            if (!udtSym || udtSym->kind != SymbolKind::UserDefinedType) return "";
            std::string memLower = Symbol::toLower(ma.memberName);
            for (auto& mi : udtSym->udtMembers) {
                if (Symbol::toLower(mi.name) == memLower) {
                    // 若该成员本身是 UDT (typeRefName 非空且能查到 UserDefinedType 符号)
                    if (!mi.typeRefName.empty()) {
                        Symbol* refSym = symTab_.lookupModule(mi.typeRefName);
                        if (refSym && refSym->kind == SymbolKind::UserDefinedType) {
                            return "vb6_type_" + cIdent(mi.typeRefName);
                        }
                    }
                    return "";  // 成员是标量/数组, 不是嵌套 UDT
                }
            }
            return "";
        }
        // Fix 037: WithMemberExpr → 当前 With 块 tempVar 的 UDT 类型递归.
        // With 块临时变量 (_vb6_with_N) 已被 cgen_stmt.cpp 注册到 knownUdtVars_
        // (仅 WithObjKind::Unknown — UDT — 才注册). 若 tempVar 不是 UDT (ClassInstance/
        // COMObject 等其他 kind), 此处返回空串.
        // 递归: .member 即 With 块 UDT 的某字段; 若该字段本身是嵌套 UDT (typeRefName
        // 在符号表中查到 UserDefinedType), 返回 "vb6_type_<memberUdtName>".
        // 例: With uCtx (UcsTlsContext) 内的 .DecrBuffer (UcsBuffer) → 返回
        // "vb6_type_UcsBuffer"; 让外层 .DecrBuffer.Data(0) 的 IndexOrCallExpr 能
        // 在 UcsBuffer 的 udtMembers 中找到 Data (动态数组成员) 并生成 VB6_SA_AT.
        case ASTNodeKind::WithMemberExpr: {
            if (withObjectInfoStack_.empty() || withObjectVars_.empty()) return "";
            const auto& info = withObjectInfoStack_.back();
            if (info.kind != WithObjKind::Unknown) return "";  // 仅 UDT
            const std::string& tempVar = withObjectVars_.back();
            std::string tempLower = Symbol::toLower(tempVar);
            auto it = knownUdtVars_.find(tempLower);
            if (it == knownUdtVars_.end()) return "";
            const std::string parentUdtCType = it->second;
            const std::string prefix = "vb6_type_";
            if (parentUdtCType.size() <= prefix.size()
                || parentUdtCType.compare(0, prefix.size(), prefix) != 0) return "";
            std::string udtName = parentUdtCType.substr(prefix.size());
            Symbol* udtSym = symTab_.lookupModule(udtName);
            if (!udtSym || udtSym->kind != SymbolKind::UserDefinedType) return "";
            auto& wm = static_cast<const WithMemberExpr&>(expr);
            std::string memLower = Symbol::toLower(wm.memberName);
            for (auto& mi : udtSym->udtMembers) {
                if (Symbol::toLower(mi.name) == memLower) {
                    if (!mi.typeRefName.empty()) {
                        Symbol* refSym = symTab_.lookupModule(mi.typeRefName);
                        if (refSym && refSym->kind == SymbolKind::UserDefinedType) {
                            return "vb6_type_" + cIdent(mi.typeRefName);
                        }
                    }
                    return "";
                }
            }
            return "";
        }
        default:
            return "";
    }
}


// Fix 084n: 推断 target 是否为 UDT 字段链, 是则返回字段 Vb6Type (含 Array 标志), 否则 Unknown.
// 供赋值语句 (cgen_stmt) 将 Variant RHS 转换为目标字段类型.
Vb6Type CCodeGen::inferUdtFieldVb6Type(const ASTNode* target) const {
    if (!target) return Vb6Type::Unknown;
    std::string memName;
    if (target->kind == ASTNodeKind::MemberAccessExpr) {
        auto& ma = static_cast<const MemberAccessExpr&>(*target);
        if (inferUdtTypeOfExpr(*ma.object).empty()) return Vb6Type::Unknown;
        memName = ma.memberName;
    } else if (target->kind == ASTNodeKind::WithMemberExpr) {
        if (withObjectInfoStack_.empty() || withObjectVars_.empty()) return Vb6Type::Unknown;
        const auto& info = withObjectInfoStack_.back();
        if (info.kind != WithObjKind::Unknown) return Vb6Type::Unknown;  // 仅 UDT
        auto& wm = static_cast<const WithMemberExpr&>(*target);
        memName = wm.memberName;
    } else {
        return Vb6Type::Unknown;
    }
    if (memName.empty()) return Vb6Type::Unknown;

    // 解析对象 UDT C 类型
    std::string udtCType;
    if (target->kind == ASTNodeKind::MemberAccessExpr) {
        auto& ma = static_cast<const MemberAccessExpr&>(*target);
        udtCType = inferUdtTypeOfExpr(*ma.object);
    } else {
        const std::string& tempVar = withObjectVars_.back();
        auto it = knownUdtVars_.find(Symbol::toLower(tempVar));
        if (it == knownUdtVars_.end()) return Vb6Type::Unknown;
        udtCType = it->second;
    }
    const std::string prefix = "vb6_type_";
    if (udtCType.size() <= prefix.size() || udtCType.compare(0, prefix.size(), prefix) != 0)
        return Vb6Type::Unknown;
    std::string udtName = udtCType.substr(prefix.size());
    Symbol* udtSym = symTab_.lookupModule(udtName);
    if (!udtSym || udtSym->kind != SymbolKind::UserDefinedType) return Vb6Type::Unknown;
    std::string memLower = Symbol::toLower(memName);
    for (auto& mi : udtSym->udtMembers) {
        if (Symbol::toLower(mi.name) == memLower) return mi.type;
    }
    return Vb6Type::Unknown;
}


// ============================================================
// Fix 085: UDT 对象字段类型推断
// ============================================================
std::string CCodeGen::udtFieldObjCType(const std::string& udtCType,
                                       const std::string& memberLower) const {
    const std::string prefix = "vb6_type_";
    if (udtCType.size() <= prefix.size() || udtCType.compare(0, prefix.size(), prefix) != 0)
        return "";
    std::string udtName = udtCType.substr(prefix.size());
    Symbol* udtSym = symTab_.lookupModule(udtName);
    if (!udtSym || udtSym->kind != SymbolKind::UserDefinedType) return "";
    std::string memLower = Symbol::toLower(memberLower);
    for (const auto& mi : udtSym->udtMembers) {
        if (Symbol::toLower(mi.name) != memLower) continue;
        if (mi.type == Vb6Type::UserDefinedType) {
            // 嵌套 UDT 字段 (非对象) — 返回其 UDT C 类型供链式推断
            return mi.typeRefName.empty() ? "" : "vb6_type_" + cIdent(mi.typeRefName);
        }
        if (mi.type == Vb6Type::Object) {
            if (!mi.typeRefName.empty()) {
                // VBA. 前缀剥离 (如 VBA.Collection → Collection)
                std::string tn = mi.typeRefName;
                if (tn.size() > 4 && tn.compare(0, 4, "VBA.") == 0) tn = tn.substr(4);
                Symbol* refSym = symTab_.lookupModule(tn);
                if (refSym && refSym->kind == SymbolKind::Class) {
                    // 项目类对象字段 → 类方法/属性调度 (early bound).
                    // 注意: C 层类类型名须用类的规范模块名 (sourceModule), 而非 UDT
                    // 字段中的引用名 (如 tZipFileItem.SourceArchive As "ZipArchive" 实际
                    // 对应 vb6_cls_cZipArchive* — 引用名可能与模块名不同, 用了引用名
                    // 会让 resolveClassMemberCall 查不到成员 (类符号按模块名登记)).
                    // sourceModule 仅在跨模块注入时填写; 类自身符号(当前类模块
                    // 编译中)为空, 此时规范名即当前模块名 moduleName_.
                    std::string clsCanon = !refSym->sourceModule.empty()
                                               ? refSym->sourceModule
                                               : moduleName_;
                    return "vb6_cls_" + cIdent(clsCanon) + "*";
                }
            }
            // Collection/COM/接口 等对象字段 → COM dispatch
            return "void*";
        }
        // 标量/字符串/数组等非对象字段
        return "";
    }
    return "";
}


std::string CCodeGen::appendUdtObjFieldMarker(const std::string& objExpr,
                                              const std::string& udtCType,
                                              const std::string& member,
                                              const std::string& accessOp) const {
    std::string fieldCType = udtFieldObjCType(udtCType, Symbol::toLower(member));
    std::string fieldAccess = objExpr + accessOp + cIdent(member);
    // 仅对象字段 (项目类 vb6_cls_* / Collection·COM void*) 才追加标记;
    // 嵌套 UDT (vb6_type_*) 与标量/字符串等原样返回 — 嵌套 UDT 继续由
    // inferUdtTypeOfExpr / 普通字段拼接处理, 标记残留会干扰函数参数等上下文.
    if (fieldCType != "void*" && fieldCType.rfind("vb6_cls_", 0) != 0) return fieldAccess;
    return fieldAccess + "  /* udt objfield " + fieldCType + " */";
}



// Fix 084o: 需要 int32_t 上下文中的 Variant 表达式 → vb6_VariantToLong 包装
std::string CCodeGen::toLongIfVariant(const std::string& cExpr, const Expr* astExpr) {
    if (cExprIsVariant(cExpr)) return "vb6_VariantToLong(" + cExpr + ")";
    if (astExpr && astExpr->kind == ASTNodeKind::IdentifierExpr) {
        auto& ident = static_cast<IdentifierExpr&>(const_cast<Expr&>(*astExpr));
        std::string lower = ident.name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (knownVariantVars_.count(lower)) return "vb6_VariantToLong(" + cExpr + ")";
    }
    return cExpr;
}
} // namespace vb6c3
