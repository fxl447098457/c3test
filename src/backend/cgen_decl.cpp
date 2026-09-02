#include "backend/cgen.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <functional>

namespace vb6c3 {

// Forward declaration from cgen_base.cpp
 FrmControlType controlTypeFromName(const std::string& name);

// --- cgen_decl.cpp: 声明生成 + 签名 + Property/Event ---


// Fix 056b: 过程开始时清理数组注册, 但保留模块级/类成员数组 (跨过程需要)
// 原逻辑 knownArrays_.clear() 会丢失模块级UDT数组的 arrayUdtElemTypes_ 注册,
// 导致方法体内访问元素时类型回退 vb6_VARIANT (MSVC C2440: 无法从LONG转换为vb6_VARIANT等).
void CCodeGen::clearProcArrayTracking() {
    std::vector<std::string> toErase;
    for (auto& name : knownArrays_) {
        // 模块级/类成员数组 → 符号表模块作用域有对应Variable符号 → 保留
        if (!symTab_.lookupModule(name)) toErase.push_back(name);
    }
    for (auto& name : toErase) {
        knownArrays_.erase(name);
        arrayElemTypes_.erase(name);
        arrayUdtElemTypes_.erase(name);
        arrayDimCounts_.erase(name);
        knownByteArrayVars_.erase(name);
    }
    knownNDArraysInProc_.clear();
}

// ============================================================
// 声明 visit 方法
// ============================================================

void CCodeGen::visit(SubDecl& node) {
    std::string sig = makeProcSignature(node);

    // Fix 055: Form事件处理函数不能为static, 因为wndproc用extern引用它们
    bool isFormEventProc = isFormModule_ && node.name.find("Form_") == 0;
    if (node.access != AccessLevel::Public && !isFormEventProc) {
        c_.emitLine("static " + sig + " {");
    } else {
        c_.emitLine(sig + " {");
    }

    c_.indent();

    // 查找符号获取参数信息
    auto* sym = symTab_.lookupModule(node.name);
    currentProc_ = sym;
    currentReturnVar_ = "";
    currentReturnCType_ = "";  // Fix 054

    // Fix 056b: 清理局部数组注册 (模块级/类成员数组跨过程保留)
    clearProcArrayTracking();
    ansiTempsToFree_.clear();
    ansiCounter_ = 0;
    knownBstrVars_.clear();
    knownDoubleVars_.clear();
    knownLongVars_.clear();
    knownLongPtrVars_.clear();  // Bug #2 fix: 也清空LongPtr集合
    knownVariantVars_.clear();
    // M22-fix: 只清空UDT变量map(旧条目会冲突), set类型不清空(WithEvents等模块级条目需跨过程保留)
    knownUdtVars_.clear();
    knownFixedStringLen_.clear();
    // Fix 010o: 清空局部变量集合
    knownLocalVars_.clear();
    knownByRefParams_.clear();  // Fix 081g
    // Fix 010r/010r-10: 类模块中注册me到knownClassVars_ (使Me.Method()正确分发)
    // 改为map赋值: me → 当前模块名(类名)
    if (isClassModule_) knownClassVars_["me"] = moduleName_;
    // P6.11: 恢复类模块成员变量类型 (clear后从持久化集合恢复)
    knownBstrVars_.insert(classBstrMembers_.begin(), classBstrMembers_.end());
    knownDoubleVars_.insert(classDoubleMembers_.begin(), classDoubleMembers_.end());
    knownLongVars_.insert(classLongMembers_.begin(), classLongMembers_.end());
    // Fix 010n: 恢复类模块UDT成员变量 (knownUdtVars_被clear后需要从classUdtMembers_恢复)
    knownUdtVars_.insert(classUdtMembers_.begin(), classUdtMembers_.end());
    // Fix 010n (扩展): 恢复普通模块模块级UDT变量 (同 classUdtMembers_ 机制)
    knownUdtVars_.insert(moduleUdtMembers_.begin(), moduleUdtMembers_.end());

    for (auto& p : node.params) {
        // Fix 081g: Register ByRef params for For-loop dereference fix
        if (!p->isByVal) {
            std::string brKey = p->name;
            std::transform(brKey.begin(), brKey.end(), brKey.begin(), ::tolower);
            knownByRefParams_.insert(brKey);
        }
        // Fix 084m: 无类型子句的 Optional 参数 (如 Optional RecordsAffected) 默认是
        // Variant, C 类型 vb6_VARIANT*; 必须注册到 knownVariantVars_,
        // 否则 `RecordsAffected = 123` 生成裸赋值 → C2440 (cDataBase Exec).
        if (!p->asType) {
            std::string pLower = p->name;
            std::transform(pLower.begin(), pLower.end(), pLower.begin(), ::tolower);
            knownVariantVars_.insert(pLower);
        }
        if (p->asType && p->asType->kind == ASTNodeKind::SimpleTypeRef) {
            auto& simpleP = static_cast<SimpleTypeRef&>(*p->asType);
            std::string pLower = p->name;
            std::transform(pLower.begin(), pLower.end(), pLower.begin(), ::tolower);
            auto* pSym = symTab_.lookupModule(simpleP.name);
            if (pSym && pSym->kind == SymbolKind::UserDefinedType) {
                knownUdtVars_[pLower] = "vb6_type_" + cIdent(simpleP.name);
            } else if (pSym && pSym->kind == SymbolKind::Class) {
                knownClassVars_[pLower] = pSym->name;
            } else if (pSym && (pSym->kind == SymbolKind::ComClass || pSym->kind == SymbolKind::ComInterface)) {
                knownTypedComVars_[pLower] = pSym;
            }
            // 接口类型的参数
            auto* pSym2 = symTab_.lookup(simpleP.name);
            if (pSym2 && pSym2->kind == SymbolKind::Class && pSym2->isInterface) {
                knownIfaceVars_[pLower] = pSym2->name;
            }
            // 注册BSTR/Double/Long类型参数到类型跟踪集合
            Vb6Type paramType = typeSys_.resolveTypeName(simpleP.name);
            if (paramType == Vb6Type::String) knownBstrVars_.insert(pLower);
            else if (paramType == Vb6Type::Double) knownDoubleVars_.insert(pLower);
            else if (paramType == Vb6Type::Long || paramType == Vb6Type::Integer || paramType == Vb6Type::Boolean) knownLongVars_.insert(pLower);
            // Bug #2 fix: LongPtr 参数注册到独立集合
            else if (paramType == Vb6Type::LongPtr) knownLongPtrVars_.insert(pLower);
            // Fix 035: Variant 参数也要注册, 否则 `(*X) = concrete` 赋值不会触发
            // wrapVariantValue 包装, 导致 C2440 (ByRef Variant 参数写穿透场景).
            else if (paramType == Vb6Type::Variant) knownVariantVars_.insert(pLower);

            // Fix 023c: 注册 void* 参数 (As Object / As Collection / 外部 COM 类型如
            // ADODB.Recordset / Scripting.Dictionary 等) 到 knownObjectVars_ —
            // 让成员访问走 COM dispatch (vb6_ComCall / vb6_ComGet*Prop),
            // 而非直接 obj.member 字段访问, 避免 C2224 (void* 上 .member).
            // 仅当参数未被前面分支精确注册为 Class / ComClass / Interface / UDT 时
            // 才查 C 类型, 避免对 vb6_cls_* / vb6_ComIface_* 等 C 类型参数的错误
            // 注册. 与 visit(VariableDecl) line 651-656 行为一致 (局部 void* 同样注册).
            if (!knownClassVars_.count(pLower) && !knownTypedComVars_.count(pLower)
                && !knownIfaceVars_.count(pLower) && !knownUdtVars_.count(pLower)) {
                std::string paramCType = mapTypeRef(p->asType.get());
                if (paramCType == "void*") {
                    knownObjectVars_.insert(pLower);
                }
                // Bug #2 fix: Enum等未知类型参数, C类型为int32_t时注册为Long
                else if (paramCType == "int32_t" || paramCType == "int16_t" || paramCType == "VBABOOL") {
                    if (!knownLongVars_.count(pLower)) knownLongVars_.insert(pLower);
                }
                // Bug #2 fix: C类型为intptr_t时注册为LongPtr
                else if (paramCType == "intptr_t") {
                    if (!knownLongPtrVars_.count(pLower)) knownLongPtrVars_.insert(pLower);
                }
                // Fix 082: COM interface pointer types (vb6_ComIface_*) are pointer-sized on x64
                else if (paramCType.find("vb6_ComIface_") != std::string::npos) {
                    if (!knownLongPtrVars_.count(pLower)) knownLongPtrVars_.insert(pLower);
                }
            }
        } else if (p->asType && p->asType->kind == ASTNodeKind::ArrayTypeRef) {
            // Fix 010r-6: Register array parameters so arr(idx) generates VB6_SA_AT instead of (*arr)(idx)
            Vb6Type elemType = resolveArrayElemType(p->asType.get());
            std::string pLower = p->name;
            std::transform(pLower.begin(), pLower.end(), pLower.begin(), ::tolower);
            knownArrays_.insert(pLower);
            arrayElemTypes_[pLower] = elemType;
            arrayDimCounts_[pLower] = 1;
            // Fix 055: 注册UDT数组元素C类型
            std::string udtCType = resolveArrayUdtElemCType(p->asType.get());
            if (!udtCType.empty()) {
                arrayUdtElemTypes_[pLower] = udtCType;
            }
            knownLocalVars_.insert(pLower);
        }
    }

    // VB6 Static Sub: 过程内所有局部变量都是static
    inStaticProc_ = node.isStatic;

    // 检测GoSub并声明返回地址栈
    hasGoSub_ = hasGoSubInStmts(node.body);
    gosubReturnCounter_ = 0;
    if (hasGoSub_) {
        c_.emitLine("int vb6_gosub_stack[32];");
        c_.emitLine("int vb6_gosub_sp = 0;");
    }

    // Bug #1 fix (082h): 预扫描UBound/LBound(arr,N>1)收集ND数组名
    scanNDArraysInStmts(node.body);

    // Fix 081: Apply default values for Optional parameters when not passed
    // VB6: Optional ByVal Ecl As Long = 1  →  if (!_has_Ecl) Ecl = 1;
    // This ensures the parameter variable has the correct default value
    // when the caller omits it, instead of the type's zero value.
    if (sym) {
        for (auto& pi : sym->params) {
            if (pi.isOptional && !pi.isParamArray && pi.hasDefaultValue && !pi.defaultValueExpr.empty()) {
                std::string pName = cIdent(pi.name);
                std::string hasFlag = "_has_" + pName;
                if (pi.isByVal) {
                    c_.emitLine("if (!" + hasFlag + ") " + pName + " = " + pi.defaultValueExpr + ";");
                } else {
                    std::string cType = mapType(pi.type);
                    if (pi.type == Vb6Type::String) {
                        c_.emitLine("if (!" + hasFlag + ") vb6_BSTR_Assign(" + pName + ", " + pi.defaultValueExpr + ");");
                    } else if (pi.type == Vb6Type::Variant) {
                        c_.emitLine("if (!" + hasFlag + ") (*" + pName + ") = " + pi.defaultValueExpr + ";");
                    } else {
                        c_.emitLine("if (!" + hasFlag + ") (*" + pName + ") = " + pi.defaultValueExpr + ";");
                    }
                }
            }
        }
    }

    // P12.3: 检测On Error并声明局部错误处理
    hasOnError_ = hasOnErrorInStmts(node.body);
    // P14.1.2: 检测Resume/Resume Next
    hasResume_ = hasResumeInStmts(node.body);
    inProtectedBlock_ = false;
    resumePointCounter_ = 0;
    dispatchPoints_.clear();
    currentErrorHandlerLabel_.clear();
    if (hasOnError_) {
        c_.emitLine("jmp_buf vb6_local_err_jmp;");
        if (hasResume_) {
            c_.emitLine("int32_t vb6_err_resume_point = 0;");
            c_.emitLine("int32_t vb6_err_resume_next_point = 0;");
        }
        c_.emitLine("vb6_SaveErrState();");
    }

    // 生成过程体 (P14.1.2: 传入hasResume_以启用resume点生成)
    emitStmtList(node.body, hasResume_);

        // P12.3: 恢复调用者的错误处理状态
    if (hasOnError_) {
        c_.emitLine("vb6_RestoreErrState();");
    }

    // M22: 释放当前过程中残留的ANSI临时变量 (正常退出路径)
    for (auto& ansiVar : ansiTempsToFree_) {
        c_.emitLine("vb6_FreeANSI(" + ansiVar + ");");
    }
    ansiTempsToFree_.clear();

    // 正常退出守卫 - 防止落入dispatch switch
    c_.emitLine("return;");

    // P14.1.2: Resume dispatch switch - 仅通过goto可达
    if (hasResume_ && !dispatchPoints_.empty()) {
        c_.emitLine("vb6_err_dispatch_switch:;");
        c_.emitLine("switch(vb6_err_dispatch) {");
        c_.indent();
        for (int pt : dispatchPoints_) {
            c_.emitLine("case " + std::to_string(pt) + ": goto vb6_resume_" + std::to_string(pt) + ";");
        }
        c_.dedent();
        c_.emitLine("}");
    }

    currentProc_ = nullptr;
    inStaticProc_ = false;
    hasGoSub_ = false;
    hasOnError_ = false;
    hasResume_ = false;
    inProtectedBlock_ = false;
    dispatchPoints_.clear();
    currentErrorHandlerLabel_.clear();
    c_.dedent();
    c_.emitLine("}");
    c_.emitBlank();
}

void CCodeGen::visit(FunctionDecl& node) {
    std::string sig = makeProcSignature(node);

    // Fix 055: Form事件处理函数不能为static, 因为wndproc用extern引用它们
    bool isFormEventFunc = isFormModule_ && node.name.find("Form_") == 0;
    if (node.access != AccessLevel::Public && !isFormEventFunc) {
        c_.emitLine("static " + sig + " {");
    } else {
        c_.emitLine(sig + " {");
    }

    c_.indent();

    // 查找符号获取参数信息
    auto* sym = symTab_.lookupModule(node.name);
    currentProc_ = sym;

    // Fix 056b: 清理局部数组注册 (模块级/类成员数组跨过程保留)
    clearProcArrayTracking();
    ansiTempsToFree_.clear();
    ansiCounter_ = 0;
    knownBstrVars_.clear();
    knownDoubleVars_.clear();
    knownLongVars_.clear();
    knownLongPtrVars_.clear();  // Bug #2 fix: 也清空LongPtr集合
    knownVariantVars_.clear();
    // M22-fix: 只清空UDT变量map(旧条目会冲突), set类型不清空(WithEvents等模块级条目需跨过程保留)
    knownUdtVars_.clear();
    knownFixedStringLen_.clear();
    // Fix 010o: 清空局部变量集合
    knownLocalVars_.clear();
    knownByRefParams_.clear();  // Fix 081g
    // Fix 010r/010r-10: 类模块中注册me到knownClassVars_ (使Me.Method()正确分发)
    // 改为map赋值: me → 当前模块名(类名)
    if (isClassModule_) knownClassVars_["me"] = moduleName_;
    // P6.11: 恢复类模块成员变量类型 (clear后从持久化集合恢复)
    knownBstrVars_.insert(classBstrMembers_.begin(), classBstrMembers_.end());
    knownDoubleVars_.insert(classDoubleMembers_.begin(), classDoubleMembers_.end());
    knownLongVars_.insert(classLongMembers_.begin(), classLongMembers_.end());
    // Fix 010n: 恢复类模块UDT成员变量 (knownUdtVars_被clear后需要从classUdtMembers_恢复)
    knownUdtVars_.insert(classUdtMembers_.begin(), classUdtMembers_.end());
    // Fix 010n (扩展): 恢复普通模块模块级UDT变量 (同 classUdtMembers_ 机制)
    knownUdtVars_.insert(moduleUdtMembers_.begin(), moduleUdtMembers_.end());

    // M22-fix: 注册参数中的UDT/类/接口变量到跟踪集合
    for (auto& p : node.params) {
        // Fix 081g: Register ByRef params for For-loop dereference fix
        if (!p->isByVal) {
            std::string brKey = p->name;
            std::transform(brKey.begin(), brKey.end(), brKey.begin(), ::tolower);
            knownByRefParams_.insert(brKey);
        }
        // Fix 084m: 无类型子句的 Optional 参数 (如 Optional RecordsAffected) 默认是
        // Variant, C 类型 vb6_VARIANT*; 必须注册到 knownVariantVars_,
        // 否则 `RecordsAffected = 123` 生成裸赋值 → C2440 (cDataBase Exec).
        if (!p->asType) {
            std::string pLower = p->name;
            std::transform(pLower.begin(), pLower.end(), pLower.begin(), ::tolower);
            knownVariantVars_.insert(pLower);
        }
        if (p->asType && p->asType->kind == ASTNodeKind::SimpleTypeRef) {
            auto& simpleP = static_cast<SimpleTypeRef&>(*p->asType);
            std::string pLower = p->name;
            std::transform(pLower.begin(), pLower.end(), pLower.begin(), ::tolower);
            auto* pSym = symTab_.lookupModule(simpleP.name);

            if (pSym && pSym->kind == SymbolKind::UserDefinedType) {
                knownUdtVars_[pLower] = "vb6_type_" + cIdent(simpleP.name);
            } else if (pSym && pSym->kind == SymbolKind::Class) {
                knownClassVars_[pLower] = pSym->name;
            } else if (pSym && (pSym->kind == SymbolKind::ComClass || pSym->kind == SymbolKind::ComInterface)) {
                knownTypedComVars_[pLower] = pSym;
            }
            auto* pSym2 = symTab_.lookup(simpleP.name);
            if (pSym2 && pSym2->kind == SymbolKind::Class && pSym2->isInterface) {
                knownIfaceVars_[pLower] = pSym2->name;
            }
            // 注册BSTR/Double/Long类型参数到类型跟踪集合
            Vb6Type paramType = typeSys_.resolveTypeName(simpleP.name);
            if (paramType == Vb6Type::String) knownBstrVars_.insert(pLower);
            else if (paramType == Vb6Type::Double) knownDoubleVars_.insert(pLower);
            else if (paramType == Vb6Type::Long || paramType == Vb6Type::Integer || paramType == Vb6Type::Boolean) knownLongVars_.insert(pLower);
            // Bug #2 fix: LongPtr 参数注册到独立集合
            else if (paramType == Vb6Type::LongPtr) knownLongPtrVars_.insert(pLower);
            // Fix 035: Variant 参数也要注册, 否则 `(*X) = concrete` 赋值不会触发
            // wrapVariantValue 包装, 导致 C2440 (ByRef Variant 参数写穿透场景).
            else if (paramType == Vb6Type::Variant) knownVariantVars_.insert(pLower);

            // Fix 023c: 注册 void* 参数 (As Object / As Collection / 外部 COM 类型如
            // ADODB.Recordset / Scripting.Dictionary 等) 到 knownObjectVars_ —
            // 让成员访问走 COM dispatch (vb6_ComCall / vb6_ComGet*Prop),
            // 而非直接 obj.member 字段访问, 避免 C2224 (void* 上 .member).
            // 仅当参数未被前面分支精确注册为 Class / ComClass / Interface / UDT 时
            // 才查 C 类型, 避免对 vb6_cls_* / vb6_ComIface_* 等 C 类型参数的错误
            // 注册. 与 visit(VariableDecl) line 651-656 行为一致 (局部 void* 同样注册).
            if (!knownClassVars_.count(pLower) && !knownTypedComVars_.count(pLower)
                && !knownIfaceVars_.count(pLower) && !knownUdtVars_.count(pLower)) {
                std::string paramCType = mapTypeRef(p->asType.get());
                if (paramCType == "void*") {
                    knownObjectVars_.insert(pLower);
                }
                // Bug #2 fix: Enum等未知类型参数, C类型为int32_t时注册为Long
                else if (paramCType == "int32_t" || paramCType == "int16_t" || paramCType == "VBABOOL") {
                    if (!knownLongVars_.count(pLower)) knownLongVars_.insert(pLower);
                }
                // Bug #2 fix: C类型为intptr_t时注册为LongPtr
                else if (paramCType == "intptr_t") {
                    if (!knownLongPtrVars_.count(pLower)) knownLongPtrVars_.insert(pLower);
                }
                // Fix 082: COM interface pointer types (vb6_ComIface_*) are pointer-sized on x64
                else if (paramCType.find("vb6_ComIface_") != std::string::npos) {
                    if (!knownLongPtrVars_.count(pLower)) knownLongPtrVars_.insert(pLower);
                }
            }
        } else if (p->asType && p->asType->kind == ASTNodeKind::ArrayTypeRef) {
            // Fix 010r-6: Register array parameters so arr(idx) generates VB6_SA_AT instead of (*arr)(idx)
            Vb6Type elemType = resolveArrayElemType(p->asType.get());
            std::string pLower = p->name;
            std::transform(pLower.begin(), pLower.end(), pLower.begin(), ::tolower);
            knownArrays_.insert(pLower);
            arrayElemTypes_[pLower] = elemType;
            arrayDimCounts_[pLower] = 1;
            // Fix 055: 注册UDT数组元素C类型
            std::string udtCType = resolveArrayUdtElemCType(p->asType.get());
            if (!udtCType.empty()) arrayUdtElemTypes_[pLower] = udtCType;
            knownLocalVars_.insert(pLower);
        }
    }

    // VB6 Static Function: 过程内所有局部变量都是static
    inStaticProc_ = node.isStatic;

    // 检测GoSub并声明返回地址栈
    hasGoSub_ = hasGoSubInStmts(node.body);
    gosubReturnCounter_ = 0;

    // Bug #1 fix (082h): 预扫描UBound/LBound(arr,N>1)收集ND数组名
    scanNDArraysInStmts(node.body);

    // Function返回值变量
    std::string retType = mapTypeRef(node.returnType.get());
    currentReturnVar_ = "vb6_ret_" + cIdent(node.name);
    currentReturnCType_ = retType;  // Fix 054: 保存返回类型C名称, 供With块UDT检测
    Vb6Type funcRetVb6Type = node.returnType ? typeSys_.resolveTypeName(static_cast<SimpleTypeRef*>(node.returnType.get())->name) : Vb6Type::Variant;
    // Fix 038/054: UDT 返回值不能用 = 0 初始化 (C2440), 改用 {0} 零初始化
    // 修复: 仅检查 C 类型名前缀即可 (typeSys 可能将 UDT 解析为 Unknown/Variant)
    {
        std::string initVal = defaultValue(funcRetVb6Type);
        if (retType.rfind("vb6_type_", 0) == 0) {
            initVal = "{0}";
        }
        c_.emitLine(retType + " " + currentReturnVar_ + " = " + initVal + ";");
    }
    // P6.11: 注册返回值变量类型 (用于BSTR安全赋值)
    std::string funcRetLower = currentReturnVar_;
    std::transform(funcRetLower.begin(), funcRetLower.end(), funcRetLower.begin(), ::tolower);
    if (funcRetVb6Type == Vb6Type::String) knownBstrVars_.insert(funcRetLower);
    else if (funcRetVb6Type == Vb6Type::Double) knownDoubleVars_.insert(funcRetLower);
    else if (funcRetVb6Type == Vb6Type::Long || funcRetVb6Type == Vb6Type::Integer || funcRetVb6Type == Vb6Type::Boolean) knownLongVars_.insert(funcRetLower);
    // Bug #2 fix: LongPtr 返回值变量注册到独立集合
    else if (funcRetVb6Type == Vb6Type::LongPtr) knownLongPtrVars_.insert(funcRetLower);
    // Fix 035: Variant 返回值变量也要注册, 否则 `Foo = concrete_expr` 赋值不会触发
    // wrapVariantValue 包装, 导致 C2440 (BSTR/int32_t → vb6_VARIANT).
    else if (funcRetVb6Type == Vb6Type::Variant) knownVariantVars_.insert(funcRetLower);

    if (hasGoSub_) {
        c_.emitLine("int vb6_gosub_stack[32];");
        c_.emitLine("int vb6_gosub_sp = 0;");
    }

    // Fix 081: Apply default values for Optional parameters when not passed
    // VB6: Optional ByVal Ecl As Long = 1  →  if (!_has_Ecl) Ecl = 1;
    // This ensures the parameter variable has the correct default value
    // when the caller omits it, instead of the type's zero value.
    if (sym) {
        for (auto& pi : sym->params) {
            if (pi.isOptional && !pi.isParamArray && pi.hasDefaultValue && !pi.defaultValueExpr.empty()) {
                std::string pName = cIdent(pi.name);
                std::string hasFlag = "_has_" + pName;
                if (pi.isByVal) {
                    c_.emitLine("if (!" + hasFlag + ") " + pName + " = " + pi.defaultValueExpr + ";");
                } else {
                    // ByRef Optional: dereference then assign default
                    // E.g. if (!_has_sText) (*sText) = vb6_BSTR_FromStr(L"");
                    std::string cType = mapType(pi.type);
                    if (pi.type == Vb6Type::String) {
                        c_.emitLine("if (!" + hasFlag + ") vb6_BSTR_Assign(" + pName + ", " + pi.defaultValueExpr + ");");
                    } else if (pi.type == Vb6Type::Variant) {
                        c_.emitLine("if (!" + hasFlag + ") (*" + pName + ") = " + pi.defaultValueExpr + ";");
                    } else {
                        c_.emitLine("if (!" + hasFlag + ") (*" + pName + ") = " + pi.defaultValueExpr + ";");
                    }
                }
            }
        }
    }

    // P12.3: 检测On Error并声明局部错误处理
    hasOnError_ = hasOnErrorInStmts(node.body);
    // P14.1.2: 检测Resume/Resume Next
    hasResume_ = hasResumeInStmts(node.body);
    inProtectedBlock_ = false;
    resumePointCounter_ = 0;
    dispatchPoints_.clear();
    currentErrorHandlerLabel_.clear();
    if (hasOnError_) {
        c_.emitLine("jmp_buf vb6_local_err_jmp;");
        if (hasResume_) {
            c_.emitLine("int32_t vb6_err_resume_point = 0;");
            c_.emitLine("int32_t vb6_err_resume_next_point = 0;");
        }
        c_.emitLine("vb6_SaveErrState();");
    }


    // 生成过程体 (P14.1.2: 传入hasResume_以启用resume点生成)
    emitStmtList(node.body, hasResume_);

        // P12.3: 恢复调用者的错误处理状态
    if (hasOnError_) {
        c_.emitLine("vb6_RestoreErrState();");
    }

    // M22: 释放当前过程中残留的ANSI临时变量 (正常退出路径)
    for (auto& ansiVar : ansiTempsToFree_) {
        c_.emitLine("vb6_FreeANSI(" + ansiVar + ");");
    }
    ansiTempsToFree_.clear();

    // 返回值
    c_.emitLine("return " + currentReturnVar_ + ";");

    // P14.1.2: Resume dispatch switch - 仅通过goto可达 (在return之后)
    if (hasResume_ && !dispatchPoints_.empty()) {
        c_.emitLine("vb6_err_dispatch_switch:;");
        c_.emitLine("switch(vb6_err_dispatch) {");
        c_.indent();
        for (int pt : dispatchPoints_) {
            c_.emitLine("case " + std::to_string(pt) + ": goto vb6_resume_" + std::to_string(pt) + ";");
        }
        c_.dedent();
        c_.emitLine("}");
    }

    currentProc_ = nullptr;

    currentProc_ = nullptr;
    currentReturnVar_ = "";
    currentReturnCType_ = "";  // Fix 054
    inStaticProc_ = false;
    hasGoSub_ = false;
    hasOnError_ = false;
    hasResume_ = false;
    inProtectedBlock_ = false;
    dispatchPoints_.clear();
    currentErrorHandlerLabel_.clear();
    c_.dedent();
    c_.emitLine("}");
    c_.emitBlank();
}

std::string CCodeGen::makeProcSignature(SubDecl& node) {
    // 类模块方法始终带 vb6_<ClassName>_ 前缀 (与 dll_entry.c / resolveClassMemberCall 调用一致)
    std::string name = cProcName(node.name, node.access, isClassModule_ ? moduleName_ : "");
    std::string params;
    if (isClassModule_) {
        params = classMeParam();
        std::string userParams = makeParamList(node.params);
        if (userParams != "void") {
            params += ", " + userParams;
        }
    } else {
        params = makeParamList(node.params);
    }
    return "void " + name + "(" + params + ")";
}

std::string CCodeGen::makeProcSignature(FunctionDecl& node) {
    // 类模块方法始终带 vb6_<ClassName>_ 前缀 (与 dll_entry.c / resolveClassMemberCall 调用一致)
    std::string name = cProcName(node.name, node.access, isClassModule_ ? moduleName_ : "");
    std::string params;
    if (isClassModule_) {
        params = classMeParam();
        std::string userParams = makeParamList(node.params);
        if (userParams != "void") {
            params += ", " + userParams;
        }
    } else {
        params = makeParamList(node.params);
    }
    std::string retType = mapTypeRef(node.returnType.get());
    return retType + " " + name + "(" + params + ")";
}

// Fix 084k: 单个参数的C类型+名字, 与makeParamList逐参数逻辑完全一致
std::string CCodeGen::makeParamCType(ParameterDecl* p, bool isDeclare) {
    // P14.1.5: ParamArray → SAFEARRAY* (always Variant array)
    if (p->isParamArray) {
        return "SAFEARRAY* " + cIdent(p->name);
    }

    std::string cType = mapTypeRef(p->asType.get());
    std::string cName = cIdent(p->name);

    // Fix 081e: Declare函数中ByVal Long/LongPtr参数映射为intptr_t
    // VB6 Long在Declare中常用于传句柄/指针 (ByVal hdc As Long等),
    // VB6是32位环境,Long=4字节=指针大小; 但x64下指针8字节,int32_t不够。
    // 将Declare中ByVal Long和ByVal LongPtr都映射为intptr_t:
    //   x86: intptr_t=4字节, 与VB6 Long兼容
    //   x64: intptr_t=8字节, 可容纳指针/句柄值
    // 纯值参数(如CodePage)传入intptr_t也不影响正确性(低32位包含值)。
    // ByRef Long参数不受影响(已映射为int32_t*,指针大小由架构决定)。
    if (isDeclare && p->isByVal && cType == "int32_t") {
        // 只对SimpleTypeRef中的Long/LongPtr提升为intptr_t
        if (p->asType && p->asType->kind == ASTNodeKind::SimpleTypeRef) {
            auto& simpleP = static_cast<SimpleTypeRef&>(*p->asType);
            if (simpleP.name == "Long" || simpleP.name == "LongPtr") {
                cType = "intptr_t";
            }
        }
    }

    // Fix 010: As Any 参数 — VB6中Any仅用于Declare, ByRef/ByVal均映射为void*
    // 不额外添加ByRef指针 (void*已是"指向任意类型的指针")
    bool isAnyType = false;
    if (p->asType && p->asType->kind == ASTNodeKind::SimpleTypeRef) {
        auto& simpleType = static_cast<SimpleTypeRef&>(*p->asType);
        if (simpleType.name == "Any" || simpleType.name == "any") {
            isAnyType = true;
            cType = "void*";
        }
    }

    // Fix 010r-6 rev2: ByRef array parameters need vb6_SafeArray1D** (double pointer)
    // so the callee can assign a new SafeArray (e.g. ReDim) and the caller sees it.
    // ByVal array params and As Any params stay as single pointer.
    bool isArrayParam = (p->asType && p->asType->kind == ASTNodeKind::ArrayTypeRef);

    if (p->isByVal || isAnyType) {
        return cType + " " + cName;
    }
    // ByRef → C pointer (ByRef array同: vb6_SafeArray1D** — callee can modify the caller's pointer)
    (void)isArrayParam;
    return cType + "* " + cName;
}

std::string CCodeGen::makeParamList(std::vector<std::unique_ptr<ParameterDecl>>& params, bool isDeclare) {
    if (params.empty()) return "void";

    std::string result;
    for (size_t i = 0; i < params.size(); i++) {
        if (i > 0) result += ", ";
        auto& p = params[i];
        result += makeParamCType(p.get(), isDeclare);
    }
    // P20-36: IsMissing support - append _has_ flags for Optional params
    // Fix 042c: Declare functions are __declspec(dllimport) — external DLL imports
    // that don't use the _has_ convention. Skip _has_ flags for Declare functions
    // so all modules agree on the same signature without _has_ params.
    if (!isDeclare) {
        for (size_t i = 0; i < params.size(); i++) {
            auto& p = params[i];
            if (p->isOptional && !p->isParamArray) {
                result += ", int _has_" + cIdent(p->name);
            }
        }
    }
    return result;
}

void CCodeGen::visit(EnumDecl& node) {
    std::string enumName = cIdent(node.name);

    // Fix 010b: 多个VB6模块可能定义同名枚举 (如LongPtr), 用#ifndef防止C2011重定义
    std::string guardName = "VB6_ENUM_" + enumName + "_DEFINED";
    h_.emitLine("#ifndef " + guardName);
    h_.emitLine("#define " + guardName);
    h_.emitLine("typedef enum vb6_enum_" + enumName + " {");
    h_.indent();

    int64_t nextVal = 0;
    for (auto& member : node.members) {
        std::string memName = enumName + "_" + cIdent(member->name);
        if (member->value) {
            // Fix 010b: 尝试常量折叠enum成员值 (如2^0 → 1, 2^1|2^2 → 6)
            // C语言enum值必须是编译期常量, 不能用vb6_Pow()等函数调用
            int64_t constVal;
            if (tryEvalConstInt(member->value.get(), constVal)) {
                h_.emitLine("vb6_enum_" + memName + " = " + std::to_string(constVal) + ",");
                nextVal = constVal;
            } else {
                // 回退: 使用表达式 (可能在C中编译失败)
                emitExpr(*member->value);
                h_.emitLine("vb6_enum_" + memName + " = " + lastExpr_ + ",");
                nextVal = 0;
                if (auto* lit = dynamic_cast<LiteralExpr*>(member->value.get())) {
                    if (lit->literalKind == LiteralKind::Long) nextVal = lit->longValue;
                    else if (lit->literalKind == LiteralKind::Integer) nextVal = lit->intValue;
                } else if (auto* unary = dynamic_cast<UnaryExpr*>(member->value.get())) {
                    if (auto* inner = dynamic_cast<LiteralExpr*>(unary->operand.get())) {
                        int64_t v = 0;
                        if (inner->literalKind == LiteralKind::Long) v = inner->longValue;
                        else if (inner->literalKind == LiteralKind::Integer) v = inner->intValue;
                        nextVal = (unary->op == UnaryOp::Negate) ? -v : v;
                    }
                }
            }
        } else {
            h_.emitLine("vb6_enum_" + memName + " = " + std::to_string(nextVal) + ",");
        }
        nextVal++;
    }

    h_.dedent();
    h_.emitLine("} vb6_enum_" + enumName + ";");
    h_.emitLine("#endif");
    h_.emitBlank();
}

void CCodeGen::visit(EnumMember& node) {
    // 由EnumDecl内部处理
}

void CCodeGen::visit(TypeDecl& node) {
    std::string typeName = cIdent(node.name);

    // Fix 010b: 多个VB6模块可能定义同名UDT (如SYSTEMTIME, FILETIME), 用#ifndef防止C2011重定义
    std::string guardName = "VB6_TYPE_" + typeName + "_DEFINED";
    h_.emitLine("#ifndef " + guardName);
    h_.emitLine("#define " + guardName);
    h_.emitLine("typedef struct vb6_type_" + typeName + " {");
    h_.indent();

    for (auto& member : node.members) {
        std::string memType = mapTypeRef(member->type.get());
        std::string memName = cIdent(member->name);
        // P15.2: 固定大小数组成员 (如 Buf(0 To 255) As Byte)
        if (member->arraySize) {
            // Fix 010d: 优先用常量折叠将数组维度求值为字面量
            // Private Const 只发射到.c, 不在.h中, 跨模块#include时会变成未声明标识符(C2065/C2057/C2229)
            // tryEvalConstInt覆盖: LiteralExpr, UnaryExpr, BinaryExpr(算术/位运算), IdentifierExpr(跨模块Const/EnumMember)
            int64_t arrVal;
            if (tryEvalConstInt(member->arraySize.get(), arrVal)) {
                h_.emitLine(memType + " " + memName + "[" + std::to_string(arrVal + 1) + "];");
            } else {
                emitExpr(*member->arraySize);
                h_.emitLine(memType + " " + memName + "[(" + lastExpr_ + ") + 1];");
            }
        } else if (member->isArrayDynamic) {
            // Fix 037 Pattern B: 动态数组成员 (`Data() As Byte`) emit `vb6_SafeArray1D* Member;`
            // 之前 bug: arraySize==nullptr 与无括号成员无法区分, emit `uint8_t Data;` (单标量字段),
            // 运行时不正确且导致 obj.Data(i) 调用变成 C2064.
            h_.emitLine("vb6_SafeArray1D* " + memName + ";  /* dynamic array member */");
        } else {
            h_.emitLine(memType + " " + memName + ";");
        }
    }

    h_.dedent();
    h_.emitLine("} vb6_type_" + typeName + ";");
    h_.emitLine("#endif");
    h_.emitBlank();
}

void CCodeGen::visit(TypeMember& node) {
    // 由TypeDecl内部处理
}

void CCodeGen::visit(ConstDecl& node) {
    std::string cType = mapTypeRef(node.asType.get());
    std::string cName = cIdent(node.name);

    // Fix 049: Register module-level constant to type-specific known*Vars_ sets.
    // Module-level constants are emitted as #define macros; when used in
    // expressions, the codegen writes the constant name. Without registration,
    // inferExprType falls back to Variant, causing wrapToBSTR to generate
    // vb6_CStr(BSTR_const) → C2440 (BSTR→VARIANT).
    {
        std::string lower = node.name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (cType == "BSTR") {
            knownBstrVars_.insert(lower);
        } else if (cType == "int32_t" || cType == "int16_t" || cType == "VBABOOL") {
            knownLongVars_.insert(lower);
        } else if (cType == "double" || cType == "float") {
            knownDoubleVars_.insert(lower);
        } else if (cType == "vb6_VARIANT") {
            knownVariantVars_.insert(lower);
        }
    }

    if (node.value) {
        emitExpr(*node.value);
        // 公共常量 → .h, 私有 → .c
        if (node.access == AccessLevel::Public) {
            h_.emitLine("#define " + cName + " (" + lastExpr_ + ")");
        } else {
            c_.emitLine("#define " + cName + " (" + lastExpr_ + ")");
        }
    }
}

void CCodeGen::visit(VariableDecl& node) {
    std::string cName = cIdent(node.name);

    // P8.1: 模块级数组声明 (支持多维)
    if (!node.dimensions.empty()) {
        Vb6Type elemType = resolveArrayElemType(node.asType.get());
        std::string saElemType = mapSaElemType(elemType);
        int dimCount = (int)node.dimensions.size();
        std::string cType = (dimCount > 1) ? "vb6_SafeArrayND*" : "vb6_SafeArray1D*";

        // 前向声明 -> .h
        if (!trackOnly_) {
        if (node.access == AccessLevel::Public) {
            h_.emitLine("extern " + cType + " " + cName + ";");
        }

        if (dimCount == 1) {
            // 一维数组
            auto& dim = node.dimensions[0];
            std::string lBound = "0";
            std::string uBound = "0";
            if (dim.lower) { emitExpr(*dim.lower); lBound = std::move(lastExpr_); }
            if (dim.upper) { emitExpr(*dim.upper); uBound = std::move(lastExpr_); }
            std::string initCode = "vb6_SafeArrayCreate1D(" + saElemType + ", " + lBound + ", " + uBound + ")";
            // Fix 054: C语言文件作用域变量必须用常量表达式初始化 (C2099)
            // 改为先声明为NULL, 再在模块初始化函数中赋值
            if (node.access == AccessLevel::Public) {
                c_.emitLine(cType + " " + cName + " = NULL;");
                moduleInitStmts_.push_back(cName + " = " + initCode + ";");
            } else {
                c_.emitLine("static " + cType + " " + cName + " = NULL;");
                moduleInitStmts_.push_back(cName + " = " + initCode + ";");
            }
        } else {
            // 多维数组: 使用ND运行时
            std::string boundsVar = "_bounds_" + cName;
            c_.emitLine("vb6_SafeArrayBound " + boundsVar + "[] = {");
            c_.indent();
            for (int d = 0; d < dimCount; d++) {
                auto& dim = node.dimensions[d];
                std::string lb = "0", ub = "0";
                if (dim.lower) { emitExpr(*dim.lower); lb = std::move(lastExpr_); }
                if (dim.upper) { emitExpr(*dim.upper); ub = std::move(lastExpr_); }
                std::string trailing = (d < dimCount - 1) ? "," : "";
                // vb6_SafeArrayBound = {lLbound, cElements}
                // cElements = uBound - lBound + 1 (VB6 "0 To 3" has 4 elements)
                c_.emitLine("{" + lb + ", (" + ub + " - " + lb + " + 1)}" + trailing);
            }
            c_.dedent();
            c_.emitLine("};");
            std::string initCode = "vb6_SafeArrayCreateND(" + saElemType + ", " + std::to_string(dimCount) + ", " + boundsVar + ")";
            // Fix 054: C语言文件作用域变量必须用常量表达式初始化 (C2099)
            if (node.access == AccessLevel::Public) {
                c_.emitLine(cType + " " + cName + " = NULL;");
                moduleInitStmts_.push_back(cName + " = " + initCode + ";");
            } else {
                c_.emitLine("static " + cType + " " + cName + " = NULL;");
                moduleInitStmts_.push_back(cName + " = " + initCode + ";");
            }
        }
        } // end if (!trackOnly_)

        // 注册到已知数组集合
        std::string lower = node.name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        knownArrays_.insert(lower);
        arrayElemTypes_[lower] = elemType;
        arrayDimCounts_[lower] = dimCount;
        // Fix 062: Byte 数组变量注册
        if (elemType == Vb6Type::Byte) knownByteArrayVars_.insert(lower);
        // Fix 055: 注册UDT数组元素C类型
        {
            std::string udtCType = resolveArrayUdtElemCType(node.asType.get());
            if (!udtCType.empty()) arrayUdtElemTypes_[lower] = udtCType;
        }
        if (!trackOnly_) knownLocalVars_.insert(lower);
        return;
    }

    // P8.1: 动态数组声明: Dim arr() As Long -> 默认1D
    if (node.isDynamicArray) {
        Vb6Type elemType = resolveArrayElemType(node.asType.get());
        std::string cType = "vb6_SafeArray1D*";

        if (!trackOnly_) {
        if (node.access == AccessLevel::Public) {
            h_.emitLine("extern " + cType + " " + cName + ";");
            c_.emitLine(cType + " " + cName + " = NULL;");
        } else {
            c_.emitLine("static " + cType + " " + cName + " = NULL;");
        }
        } // end if (!trackOnly_)

        // 注册到已知数组集合
        std::string lower = node.name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        knownArrays_.insert(lower);
        arrayElemTypes_[lower] = elemType;
        arrayDimCounts_[lower] = 1;  // 动态数组默认1D
        // Fix 062: Byte 数组变量注册
        if (elemType == Vb6Type::Byte) knownByteArrayVars_.insert(lower);
        // Fix 055: 注册UDT数组元素C类型
        {
            std::string udtCType = resolveArrayUdtElemCType(node.asType.get());
            if (!udtCType.empty()) arrayUdtElemTypes_[lower] = udtCType;
        }
        if (!trackOnly_) knownLocalVars_.insert(lower);
        return;
    }

    std::string cType = mapTypeRef(node.asType.get());

    // 检查是否是类类型变量 → 注册到 knownClassVars_
    if (node.asType && node.asType->kind == ASTNodeKind::SimpleTypeRef) {
        auto& simple = static_cast<SimpleTypeRef&>(*node.asType);
        auto* clsSym = lookupModuleDotted(simple.name);
        if (clsSym && clsSym->kind == SymbolKind::Class) {
            std::string lower = node.name;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            // P6.4: 接口类 → 注册到 knownIfaceVars_ (而非 knownClassVars_)
            if (clsSym->isInterface) {
                knownIfaceVars_[lower] = clsSym->name;
            } else {
                // Fix 010r-10: map赋值, 存储类名以便方法分发时查找
                knownClassVars_[lower] = clsSym->name;
                // P14.3.1: Dim As New自动实例化 (模块级)
                if (node.isNew) {
                    knownNewVars_[lower] = cIdent(clsSym->name);
                }
            }
            // P6.5: WithEvents变量 → 注册到 knownWithEventsVars_
            if (node.isWithEvents) {
                knownWithEventsVars_[lower] = clsSym->name;
            }
        }
    }

    // 检查是否是UDT类型变量 → 注册到 knownUdtVars_
    if (node.asType && node.asType->kind == ASTNodeKind::SimpleTypeRef) {
        auto& simpleUdt = static_cast<SimpleTypeRef&>(*node.asType);
        auto* udtSymDecl = lookupDotted(simpleUdt.name);
        if (udtSymDecl && udtSymDecl->kind == SymbolKind::UserDefinedType) {
            std::string udtLower = node.name;
            std::transform(udtLower.begin(), udtLower.end(), udtLower.begin(), ::tolower);
            knownUdtVars_[udtLower] = "vb6_type_" + cIdent(simpleUdt.name);
        }
    }

    // 检查是否是定长字符串变量 → 注册到 knownFixedStringLen_
    if (node.asType && node.asType->kind == ASTNodeKind::FixedStringTypeRef) {
        auto& fs = static_cast<FixedStringTypeRef&>(*node.asType);
        std::string fsLower = node.name;
        std::transform(fsLower.begin(), fsLower.end(), fsLower.begin(), ::tolower);
        // 评估长度表达式(必须是编译期常量)
        emitExpr(*fs.length);
        knownFixedStringLen_[fsLower] = lastExpr_;
    }

    // 检查是否是Object类型变量 → 注册到 knownObjectVars_ (COM后期绑定)
    if (cType == "void*") {  // Object类型映射为void*
        std::string lower = node.name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        knownObjectVars_.insert(lower);
    }

    // Fix 082: COM interface pointer types (vb6_ComIface_*) are pointer-sized on x64
    if (cType.find("vb6_ComIface_") != std::string::npos) {
        std::string lower = node.name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        knownLongPtrVars_.insert(lower);
    }

    // P6.3: 检查是否是前期绑定COM变量 → 注册到 knownTypedComVars_
    if (node.asType && node.asType->kind == ASTNodeKind::SimpleTypeRef) {
        auto& simple = static_cast<SimpleTypeRef&>(*node.asType);
        auto* comSym = lookupModuleDotted(simple.name);
        if (comSym && (comSym->kind == SymbolKind::ComClass || comSym->kind == SymbolKind::ComInterface)) {
            std::string lower = node.name;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            knownTypedComVars_[lower] = comSym;
            knownObjectVars_.erase(lower);  // 优先前期绑定
            // Dim As New ComClass 自动实例化 (P14.3.1扩展)
            if (node.isNew && comSym->kind == SymbolKind::ComClass) {
                knownNewVars_[lower] = cIdent(comSym->name);
            }
            // P13.23: ComClass WithEvents -> knownWithEventsVars_
            if (node.isWithEvents && comSym->kind == SymbolKind::ComClass && comSym->comHasSourceIface) {
                knownWithEventsVars_[lower] = comSym->name;
            }
        }
    }

        // P16: WithEvents控件类型检测 → 注册到 knownWithEventsCtrlVars_
    // Dim WithEvents cmd As CommandButton → knownWithEventsCtrlVars_["cmd"] = CommandButton
    if (node.isWithEvents && node.asType && node.asType->kind == ASTNodeKind::SimpleTypeRef) {
        auto& simple16 = static_cast<SimpleTypeRef&>(*node.asType);
        FrmControlType ctrlType = controlTypeFromName(simple16.name);
        if (ctrlType != FrmControlType::Unknown) {
            std::string lower16 = node.name;
            std::transform(lower16.begin(), lower16.end(), lower16.begin(), ::tolower);
            knownWithEventsCtrlVars_[lower16] = ctrlType;
            knownWithEventsCtrlOrigNames_[lower16] = cName;  // 保留原始变量名(大小写)
            cType = "HWND";  // 控件WithEvents变量存储HWND
            knownObjectVars_.erase(lower16);  // 移除可能的void*标记
            knownVariantVars_.erase(lower16);  // 移除可能的Variant标记
        } else {
            // Fix 056a: 非标准控件的WithEvents变量(如VBControlExtender)当作COM对象
            // cType可能是int32_t(mapTypeRef默认值), 必须改为void*
            std::string lower16 = node.name;
            std::transform(lower16.begin(), lower16.end(), lower16.begin(), ::tolower);
            if (cType != "void*") {
                cType = "void*";
                knownObjectVars_.insert(lower16);
                knownVariantVars_.erase(lower16);
                knownLongVars_.erase(lower16);
            }
        }
    }
// 记录变量类型集合 (用于Debug.Print和COM解封类型推断)
    if (cType == "double" || cType == "float") {
        std::string lower = node.name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        knownDoubleVars_.insert(lower);
    } else if (cType == "BSTR") {
        std::string lower = node.name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        knownBstrVars_.insert(lower);
    } else if (cType == "int32_t" || cType == "int16_t" || cType == "VBABOOL") {
        std::string lower = node.name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        knownLongVars_.insert(lower);
    } else if (cType == "vb6_VARIANT") {
        // P8.4: 记录Variant类型局部变量
        std::string lower = node.name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        knownVariantVars_.insert(lower);
    }

    // Fix 010: 类模块tracking-only模式, 跳过变量声明生成(已在结构体中)
    if (trackOnly_) return;

    // Fix 010o: 注册局部变量到 knownLocalVars_ (非trackOnly模式 = 过程内局部Dim)
    {
        std::string lower = node.name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        knownLocalVars_.insert(lower);
    }

    // 前向声明 → .h, 定义 → .c
    if (node.access == AccessLevel::Public) {
        h_.emitLine("extern " + cType + " " + cName + ";");
    }

    // 变量定义 → .c
    // 类类型变量的默认值是NULL
    bool isClassType = false;
    bool isComIfaceType = false;
    bool isVb6IfaceType = false;  // P6.4: VB6接口引用类型
    if (node.asType && node.asType->kind == ASTNodeKind::SimpleTypeRef) {
        auto& simple = static_cast<SimpleTypeRef&>(*node.asType);
        auto* clsSym = lookupModuleDotted(simple.name);
        isClassType = (clsSym && clsSym->kind == SymbolKind::Class && !clsSym->isInterface);
        isVb6IfaceType = (clsSym && clsSym->kind == SymbolKind::Class && clsSym->isInterface);
        isComIfaceType = (clsSym && (clsSym->kind == SymbolKind::ComClass || clsSym->kind == SymbolKind::ComInterface));
    }

    if (node.initializer) {
        emitExpr(*node.initializer);
        if (node.access == AccessLevel::Public) {
            c_.emitLine(cType + " " + cName + " = " + lastExpr_ + ";");
        } else {
            c_.emitLine("static " + cType + " " + cName + " = " + lastExpr_ + ";");
        }
    } else {
        // 判断是否是UDT/Enum类型 → 用 {0} 或 0 初始化
        bool isUdtType = false;
        bool isEnumType = false;  // Fix 010q
        if (node.asType && node.asType->kind == ASTNodeKind::SimpleTypeRef) {
            auto& simple = static_cast<SimpleTypeRef&>(*node.asType);
            auto* sym = lookupDotted(simple.name);
            isUdtType = (sym && sym->kind == SymbolKind::UserDefinedType);
            isEnumType = (sym && sym->kind == SymbolKind::EnumType);  // Fix 010q
        }
        std::string initVal;
        if (isClassType || isComIfaceType || cType == "HWND") {
            initVal = "NULL";
        } else if (isVb6IfaceType) {
            initVal = "{0}";  // P6.4: 接口引用 = {vtbl=NULL, obj=NULL}
        } else if (isUdtType) {
            initVal = "{0}";
        } else if (isEnumType) {  // Fix 010q
            initVal = "0";
        } else {
            initVal = defaultValue(
                node.asType ? typeSys_.resolveTypeName(static_cast<SimpleTypeRef*>(node.asType.get())->name) : Vb6Type::Variant
            );
        }
        // M22: 文件作用域BSTR初始化不能用函数调用(vb6_BSTR_Empty), 用NULL替代
        if (initVal == "vb6_BSTR_Empty()") initVal = "NULL";
        // Fix 084aa: 文件作用域Variant初始化不能用函数调用(vb6_VariantEmpty), 用{0}替代
        // ({0} 即 vt=0=VT_EMPTY, 与 vb6_VariantEmpty() 语义一致)
        if (initVal == "vb6_VariantEmpty()") initVal = "{0}";
        if (node.access == AccessLevel::Public) {
            c_.emitLine(cType + " " + cName + " = " + initVal + ";");
        } else {
            c_.emitLine("static " + cType + " " + cName + " = " + initVal + ";");
        }
    }
}

void CCodeGen::visit(DeclareDecl& node) {
    // 外部函数声明 (Declare Sub/Function ... Lib "xxx" [Alias "yyy"] [CDecl])
    // Fix 081e: Declare函数返回Long在x64下应映射为intptr_t
    // VB6 Long (32-bit) 在Declare中常用于返回句柄/指针 (如CreateEnhMetaFileW返回HDC),
    // 在x64下需要intptr_t (8字节) 才能容纳指针值
    std::string retType = (node.procKind == ProcKind::Function)
        ? mapDeclareType(node.returnType.get()) : "void";

    std::string params = makeParamList(node.params, true);

    // 调用约定
    std::string callConv = (node.callingConv == CallConv::CDecl) ? "__cdecl" : "__stdcall";

    // 去除字符串两端引号 (词法器保留引号)
    auto stripQuotes = [](const std::string& s) -> std::string {
        if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
            return s.substr(1, s.size() - 2);
        return s;
    };

    // Lib名: 去引号、去.dll后缀
    std::string libName = stripQuotes(node.libName);
    if (libName.size() > 4 &&
        (libName.compare(libName.size()-4, 4, ".dll") == 0 ||
         libName.compare(libName.size()-4, 4, ".DLL") == 0)) {
        libName = libName.substr(0, libName.size()-4);
    }

    // Alias: 去引号, 保持原始导出名 (大小写敏感, 可能含#序号前缀)
    std::string aliasName = stripQuotes(node.aliasName);

    // VB6函数名→C标识符 (用于调用点)
    std::string cFuncIdent = cIdent(node.name);

    // 导出名: Alias优先, 否则用VB6函数名
    // 注意: Windows API函数名是大小写敏感的, 需要保持原始大小写
    std::string exportedName = aliasName.empty() ? node.name : aliasName;

    // M22: 检测A版Declare函数 (函数名或Alias以'A'结尾)
    // A版API需要BSTR->ANSI转换: vb6_BSTR_ToANSI/vb6_FreeANSI
    bool isAnsiDeclare = false;
    if (!aliasName.empty()) {
        // Alias "SomeFuncA" - check if alias ends with 'A'
        std::string aliasStr = stripQuotes(node.aliasName);
        if (aliasStr.size() >= 2 && aliasStr.back() == 'A' && std::isupper(static_cast<unsigned char>(aliasStr[aliasStr.size()-1]))) {
            // Also check that the char before 'A' is lowercase (to avoid false positives like "Data")
            if (std::isalpha(static_cast<unsigned char>(aliasStr[aliasStr.size()-2])) &&
                std::islower(static_cast<unsigned char>(aliasStr[aliasStr.size()-2]))) {
                isAnsiDeclare = true;
            }
        }
    } else {
        // No Alias - check if function name ends with 'A'
        if (node.name.size() >= 2 && node.name.back() == 'A' &&
            std::islower(static_cast<unsigned char>(node.name[node.name.size()-2]))) {
            isAnsiDeclare = true;
        }
    }
    if (isAnsiDeclare) {
        std::string funcLower = node.name;
        std::transform(funcLower.begin(), funcLower.end(), funcLower.begin(), ::tolower);
        knownDeclareAnsi_.insert(funcLower);
    }

    // 生成: #pragma comment(lib, "xxx.lib")
    c_.emitLine("#pragma comment(lib, \"" + libName + ".lib\")");

    // Fix 010a: 避免与Windows SDK (windows.h) 声明冲突
    //
    // 问题: vb6rtl.h 已 #include <windows.h>, 即所有Windows API函数已被声明。
    // C3生成的 __declspec(dllimport) 声明与SDK声明签名不同 (如 int32_t vs HANDLE/void*),
    // 导致 C2371 "redefinition; different basic types" 等错误。
    //
    // 解决方案: 使用C3内部唯一名称 vb6_di_<ExportedName> 作为 __declspec(dllimport) 的函数名,
    // 然后用 #define 将VB6函数名映射到该内部名称。
    //
    // 这样:
    // 1. SDK已#define的宏 (如 CopyMemory → RtlMoveMemory → memmove):
    //    #ifndef CopyMemory 为false → 不生成C3的#define → 调用点使用SDK的宏展开 → 正确
    //    (vb6_di_CopyMemory 声明存在但永远不会被调用 → 无害)
    // 2. SDK已声明为函数 (如 GetCurrentProcess):
    //    #ifndef GetCurrentProcess 为true → 生成 #define GetCurrentProcess vb6_di_GetCurrentProcess
    //    → 调用点 GetCurrentProcess() 被宏展开为 vb6_di_GetCurrentProcess() → 使用C3的导入版本
    //    (SDK的 GetCurrentProcess 声明仍在, 但不会被调用 → 无冲突, 因为名字不同)
    // 3. SDK未声明的函数 (如 archive_read_new):
    //    #ifndef 为true → #define 映射生效 → 调用 vb6_di_archive_read_new() → 正确

    // C3内部导入名: 使用导出名构造唯一标识符
    // Fix 010b: 序号导出名 (如 "#644") 含非法C标识符字符, 需清洗
    // '#' → 'ord_', 其他非字母数字/下划线字符 → '_'
    std::string sanitizedExport = exportedName;
    for (size_t i = 0; i < sanitizedExport.size(); i++) {
        char c = sanitizedExport[i];
        if (c == '#') {
            sanitizedExport[i] = '_';
        } else if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
            sanitizedExport[i] = '_';
        }
    }
    // 确保不以数字开头 (合法C标识符要求)
    if (!sanitizedExport.empty() && (std::isdigit(static_cast<unsigned char>(sanitizedExport[0])) || sanitizedExport[0] == '_')) {
        // 对序号导出(#nnn清洗后为_nnn), 加ord前缀使名称更清晰
        if (!exportedName.empty() && exportedName[0] == '#') {
            sanitizedExport = "ord" + sanitizedExport;  // _644 → ord_644
        } else {
            sanitizedExport = "vb6_" + sanitizedExport;
        }
    }
    std::string cExportedIdent = "vb6_di_" + sanitizedExport;

    // Fix 010i: 同一Declare函数可能出现在多个VB6模块中 (如CoTaskMemFree)
    // 用#ifndef guard防止__declspec(dllimport)声明重定义 (C2371)
    std::string diGuard = "VB6_DI_" + sanitizedExport + "_DEFINED";
    h_.emitLine("#ifndef " + diGuard);
    h_.emitLine("#define " + diGuard);
    // Fix 076: Changed from __declspec(dllimport) to extern declaration.
    // __declspec(dllimport) creates import symbols named vb6_di_Xxx that can't be
    // resolved by Windows import libraries (which export the real API names like
    // CloseEnhMetaFile, not vb6_di_CloseEnhMetaFile). Instead, we declare them as
    // extern and provide forwarding stubs in vb6rtl.c that bridge vb6_di_Xxx → real API.
    h_.emitLine("extern " + retType + " " + callConv + " " + cExportedIdent + "(" + params + ");");
    h_.emitLine("#endif");

    // 生成: #define <VB6名> → <内部导入名> (仅当VB6名未被SDK定义为宏时)
    // #ifndef 检查处理两种情况:
    //   - SDK宏 (CopyMemory等): #ifndef为false, 跳过 → 调用使用SDK宏
    //   - SDK函数声明: #ifndef为true, 生成 → 调用重定向到C3导入版本
    //   - 无SDK定义: #ifndef为true, 生成 → 正常
    h_.emitLine("#ifndef " + cFuncIdent);
    h_.emitLine("#define " + cFuncIdent + " " + cExportedIdent);
    h_.emitLine("#endif");
}

void CCodeGen::visit(PropertyDecl& node) {
    // Property Get/Let/Set → C函数
    // 类模块: 第一个参数为 me 指针
    std::string sig = makePropertySignature(node);
    c_.emitLine(sig + " {");

    // P6.6修复: 设置currentProc_ (与SubDecl/FunctionDecl相同)
    // 这确保IdentifierExpr中的类模块变量加me->前缀, PropertyGet返回值赋值正确
    // Fix 032: 必须按属性种类精确查找符号. lookupModule() 默认返回 PropertyGet
    // ($pg 后缀, 优先级最高), 导致 Property Let/Set 体内 currentProc_ 被错误设为
    // 同名 PropertyGet 的符号 — currentProc_->params 缺失 Let/Set 的最后一个
    // 形参 (赋值 RHS, 如 Property Let SockOpt 中的 Value), 致使 IdentifierExpr
    // 的 ByRef 形参短路 (line 311-332) 无法匹配 → 形参被误解析为跨模块
    // PropertyGet 调用 (如 cAsyncSocket Value 形参 → vb6_cCsv_prop_get_Value).
    // Fix 032 改 args 发射期间 asCallCallee_=false 后, 该引用从"函数名裸引用"
    // 变为"实际函数调用 vb6_cCsv_prop_get_Value((void*)me)", 返回 vb6_VARIANT,
    // 触发 100+ 个 C2440. 与语义分析 Pass2 (semantic_analyzer.cpp:775) 一致使用
    // lookupModuleByKind 精确定位属性符号.
    SymbolKind propSk;
    switch (node.propKind) {
        case ProcKind::PropertyGet:  propSk = SymbolKind::PropertyGet; break;
        case ProcKind::PropertyLet:  propSk = SymbolKind::PropertyLet; break;
        case ProcKind::PropertySet:  propSk = SymbolKind::PropertySet; break;
        default:                     propSk = SymbolKind::PropertyGet; break;
    }
    auto* propSym = symTab_.lookupModuleByKind(node.name, propSk);
    currentProc_ = propSym;

    // Fix 056b: 清理局部数组注册 (模块级/类成员数组跨过程保留)
    clearProcArrayTracking();
    ansiTempsToFree_.clear();
    ansiCounter_ = 0;
    knownBstrVars_.clear();
    knownDoubleVars_.clear();
    knownLongVars_.clear();
    knownLongPtrVars_.clear();  // Bug #2 fix: 也清空LongPtr集合
    knownVariantVars_.clear();
    // M22-fix: 只清空UDT变量map(旧条目会冲突), set类型不清空(WithEvents等模块级条目需跨过程保留)
    knownUdtVars_.clear();
    knownFixedStringLen_.clear();
    // Fix 010o: 清空局部变量集合
    knownLocalVars_.clear();
    knownByRefParams_.clear();  // Fix 081g
    // Fix 010r/010r-10: 类模块中注册me到knownClassVars_ (使Me.Method()正确分发)
    // 改为map赋值: me → 当前模块名(类名)
    if (isClassModule_) knownClassVars_["me"] = moduleName_;
    // P6.11: 恢复类模块成员变量类型 (clear后从持久化集合恢复)
    knownBstrVars_.insert(classBstrMembers_.begin(), classBstrMembers_.end());
    knownDoubleVars_.insert(classDoubleMembers_.begin(), classDoubleMembers_.end());
    knownLongVars_.insert(classLongMembers_.begin(), classLongMembers_.end());
    // Fix 010n: 恢复类模块UDT成员变量 (knownUdtVars_被clear后需要从classUdtMembers_恢复)
    knownUdtVars_.insert(classUdtMembers_.begin(), classUdtMembers_.end());
    // Fix 010n (扩展): 恢复普通模块模块级UDT变量 (同 classUdtMembers_ 机制)
    knownUdtVars_.insert(moduleUdtMembers_.begin(), moduleUdtMembers_.end());

    // M22-fix: 注册参数中的UDT/类/接口变量到跟踪集合
    for (auto& p : node.params) {
        // Fix 081g: Register ByRef params for For-loop dereference fix
        if (!p->isByVal) {
            std::string brKey = p->name;
            std::transform(brKey.begin(), brKey.end(), brKey.begin(), ::tolower);
            knownByRefParams_.insert(brKey);
        }
        // Fix 084m: 无类型子句的 Optional 参数 (如 Optional RecordsAffected) 默认是
        // Variant, C 类型 vb6_VARIANT*; 必须注册到 knownVariantVars_,
        // 否则 `RecordsAffected = 123` 生成裸赋值 → C2440 (cDataBase Exec).
        if (!p->asType) {
            std::string pLower = p->name;
            std::transform(pLower.begin(), pLower.end(), pLower.begin(), ::tolower);
            knownVariantVars_.insert(pLower);
        }
        if (p->asType && p->asType->kind == ASTNodeKind::SimpleTypeRef) {
            auto& simpleP = static_cast<SimpleTypeRef&>(*p->asType);
            std::string pLower = p->name;
            std::transform(pLower.begin(), pLower.end(), pLower.begin(), ::tolower);
            auto* pSym = symTab_.lookupModule(simpleP.name);
            if (pSym && pSym->kind == SymbolKind::UserDefinedType) {
                knownUdtVars_[pLower] = "vb6_type_" + cIdent(simpleP.name);
            } else if (pSym && pSym->kind == SymbolKind::Class) {
                knownClassVars_[pLower] = pSym->name;
            } else if (pSym && (pSym->kind == SymbolKind::ComClass || pSym->kind == SymbolKind::ComInterface)) {
                knownTypedComVars_[pLower] = pSym;
            }
            auto* pSym2 = symTab_.lookup(simpleP.name);
            if (pSym2 && pSym2->kind == SymbolKind::Class && pSym2->isInterface) {
                knownIfaceVars_[pLower] = pSym2->name;
            }
            // 注册BSTR/Double/Long类型参数到类型跟踪集合
            Vb6Type paramType = typeSys_.resolveTypeName(simpleP.name);
            if (paramType == Vb6Type::String) knownBstrVars_.insert(pLower);
            else if (paramType == Vb6Type::Double) knownDoubleVars_.insert(pLower);
            else if (paramType == Vb6Type::Long || paramType == Vb6Type::Integer || paramType == Vb6Type::Boolean) knownLongVars_.insert(pLower);
            // Bug #2 fix: LongPtr 参数注册到独立集合
            else if (paramType == Vb6Type::LongPtr) knownLongPtrVars_.insert(pLower);
            // Fix 035: Variant 参数也要注册, 否则 `(*X) = concrete` 赋值不会触发
            // wrapVariantValue 包装, 导致 C2440 (ByRef Variant 参数写穿透场景).
            else if (paramType == Vb6Type::Variant) knownVariantVars_.insert(pLower);

            // Fix 023c: 注册 void* 参数 (As Object / As Collection / 外部 COM 类型如
            // ADODB.Recordset / Scripting.Dictionary 等) 到 knownObjectVars_ —
            // 让成员访问走 COM dispatch (vb6_ComCall / vb6_ComGet*Prop),
            // 而非直接 obj.member 字段访问, 避免 C2224 (void* 上 .member).
            // 仅当参数未被前面分支精确注册为 Class / ComClass / Interface / UDT 时
            // 才查 C 类型, 避免对 vb6_cls_* / vb6_ComIface_* 等 C 类型参数的错误
            // 注册. 与 visit(VariableDecl) line 651-656 行为一致 (局部 void* 同样注册).
            if (!knownClassVars_.count(pLower) && !knownTypedComVars_.count(pLower)
                && !knownIfaceVars_.count(pLower) && !knownUdtVars_.count(pLower)) {
                std::string paramCType = mapTypeRef(p->asType.get());
                if (paramCType == "void*") {
                    knownObjectVars_.insert(pLower);
                }
                // Bug #2 fix: Enum等未知类型参数, C类型为int32_t时注册为Long
                else if (paramCType == "int32_t" || paramCType == "int16_t" || paramCType == "VBABOOL") {
                    if (!knownLongVars_.count(pLower)) knownLongVars_.insert(pLower);
                }
                // Bug #2 fix: C类型为intptr_t时注册为LongPtr
                else if (paramCType == "intptr_t") {
                    if (!knownLongPtrVars_.count(pLower)) knownLongPtrVars_.insert(pLower);
                }
                // Fix 082: COM interface pointer types (vb6_ComIface_*) are pointer-sized on x64
                else if (paramCType.find("vb6_ComIface_") != std::string::npos) {
                    if (!knownLongPtrVars_.count(pLower)) knownLongPtrVars_.insert(pLower);
                }
            }
        } else if (p->asType && p->asType->kind == ASTNodeKind::ArrayTypeRef) {
            // Fix 010r-6: Register array parameters so arr(idx) generates VB6_SA_AT instead of (*arr)(idx)
            Vb6Type elemType = resolveArrayElemType(p->asType.get());
            std::string pLower = p->name;
            std::transform(pLower.begin(), pLower.end(), pLower.begin(), ::tolower);
            knownArrays_.insert(pLower);
            arrayElemTypes_[pLower] = elemType;
            arrayDimCounts_[pLower] = 1;
            // Fix 055: 注册UDT数组元素C类型
            std::string udtCType = resolveArrayUdtElemCType(p->asType.get());
            if (!udtCType.empty()) arrayUdtElemTypes_[pLower] = udtCType;
            knownLocalVars_.insert(pLower);
        }
    }

        // P12.3: 恢复调用者的错误处理状态
    if (hasOnError_) {
        c_.emitLine("vb6_RestoreErrState();");
    }

    // Property Get: 设置返回值变量 (与Function相同语义)
    if (node.propKind == ProcKind::PropertyGet) {
        currentReturnVar_ = "vb6_ret_" + cIdent(node.name);
        if (node.returnType) {
            std::string retType = mapTypeRef(node.returnType.get());
            Vb6Type retVb6Type = typeSys_.resolveTypeName(
                static_cast<SimpleTypeRef*>(node.returnType.get())->name);
            // Fix 038/054: UDT 返回值不能用 = 0 初始化 (C2440), 改用 {0}
            // 修复: 仅检查 C 类型名前缀即可 (typeSys 可能将 UDT 解析为 Unknown/Variant)
            std::string initVal = defaultValue(retVb6Type);
            if (retType.rfind("vb6_type_", 0) == 0) {
                initVal = "{0}";
            }
            c_.emitLine(retType + " " + currentReturnVar_ + " = " + initVal + ";");
            // P6.11: 注册返回值变量类型 (用于BSTR安全赋值)
            // Property Get 的 Prefix = me->m_Prefix 会被替换为 vb6_ret_Prefix = me->m_Prefix
            // 如果返回类型是String, 必须使用 vb6_BSTR_Assign 确保 deep copy,
            // 否则返回浅引用会导致 COM 调用者 SysFreeString 与 me->m_Prefix 双重释放
            std::string retLower = currentReturnVar_;
            std::transform(retLower.begin(), retLower.end(), retLower.begin(), ::tolower);
            if (retVb6Type == Vb6Type::String) knownBstrVars_.insert(retLower);
            else if (retVb6Type == Vb6Type::Double) knownDoubleVars_.insert(retLower);
            else if (retVb6Type == Vb6Type::Long || retVb6Type == Vb6Type::Integer || retVb6Type == Vb6Type::Boolean) knownLongVars_.insert(retLower);
            else if (retVb6Type == Vb6Type::Variant) knownVariantVars_.insert(retLower);
        }
    }

    // Bug #1 fix (082h): 预扫描UBound/LBound(arr,N>1)收集ND数组名
    scanNDArraysInStmts(node.body);

    c_.indent();
    // P12.3: 检测On Error并声明局部错误处理
    hasOnError_ = hasOnErrorInStmts(node.body);
    if (hasOnError_) {
        c_.emitLine("jmp_buf vb6_local_err_jmp;");
        c_.emitLine("vb6_SaveErrState();");
    }
    emitStmtList(node.body);

    // M22: 释放ANSI临时变量
    for (auto& ansiVar : ansiTempsToFree_) {
        c_.emitLine("vb6_FreeANSI(" + ansiVar + ");");
    }
    ansiTempsToFree_.clear();

    // Property Get: 隐式返回 vb6_ret_<propName>
    if (node.propKind == ProcKind::PropertyGet && node.returnType) {
        c_.emitLine("return " + currentReturnVar_ + ";");
    }
    c_.dedent();

    // 清理返回值变量和currentProc_
    if (node.propKind == ProcKind::PropertyGet) {
        currentReturnVar_ = "";
    }
    currentProc_ = nullptr;
    hasOnError_ = false;

    c_.emitLine("}");
    c_.emitBlank();
}

void CCodeGen::visit(EventDecl& node) {
    // P6.5: Event声明 → 回调函数指针typedef在emitEventSink中统一生成
    // 此处仅生成注释标记
    c_.emitLine("/* Event " + node.name + " — callback typedef in event sink table */");
}

// ============================================================
// Property签名生成
// ============================================================

std::string CCodeGen::makePropertySignature(PropertyDecl& node) {
    // Property Get/Let/Set使用不同前缀: prop_get_/prop_let_/prop_set_
    // 避免同名Property在C层面链接冲突
    std::string prefix;
    switch (node.propKind) {
        case ProcKind::PropertyGet:  prefix = "prop_get_"; break;
        case ProcKind::PropertyLet:  prefix = "prop_let_"; break;
        case ProcKind::PropertySet:  prefix = "prop_set_"; break;
        default:                     prefix = "prop_get_"; break;
    }
    // 类模块属性始终带 vb6_<ClassName>_ 前缀 (与 dll_entry.c / resolveClassMemberCall 调用一致)
    std::string propName = cProcName(prefix + node.name, node.access, isClassModule_ ? moduleName_ : "");
    std::string params;

    // 类模块: 第一个参数为 me 指针
    if (isClassModule_) {
        params = classMeParam();
        if (!node.params.empty()) params += ", ";
    }

    // 空参数列表: 类模块已有me参数时不需要"void"
    if (node.params.empty() && isClassModule_) {
        // params已经有me, 不追加
    } else {
        params += makeParamList(node.params);
    }

    switch (node.propKind) {
        case ProcKind::PropertyGet: {
            std::string retType = node.returnType ? mapTypeRef(node.returnType.get()) : "vb6_VARIANT";
            return retType + " " + propName + "(" + params + ")";
        }
        case ProcKind::PropertyLet: {
            // VB6: Property Let Name(v) — 最后一个参数v就是赋值值
            // 不追加额外的vb6_let_value参数
            return "void " + propName + "(" + params + ")";
        }
        case ProcKind::PropertySet: {
            // VB6: Property Set Name(v) — 最后一个参数v就是对象引用
            return "void " + propName + "(" + params + ")";
        }
        default:
            return "void " + propName + "(" + params + ")";
    }
}

// ============================================================
// 类工厂函数生成
// ============================================================

std::string CCodeGen::classMeParam() const {
    std::string clsStruct = "vb6_cls_" + cIdent(moduleName_);  // Fix 013: 用 moduleName_ (VB_Name)
    return clsStruct + "* me";
}

// Fix 019: 在事件包装函数体内, handler (void*) 需要被强制转换为类指针类型
// 以正确调用 vb6_<Mod>_<Handler>(cls* me, ...) 形式的类方法。
// 历史上这里误用了 classMeParam() ("vb6_cls_X* me" — 参数声明) 作为函数调用实参,
// 导致生成伪 C: vb6_<Mod>_<Hand>(vb6_cls_X* me/* from handler */, ...) 报 C2065 'me' 未声明。
std::string CCodeGen::classHandlerCast() const {
    std::string clsStruct = "vb6_cls_" + cIdent(moduleName_);
    return "((" + clsStruct + "*)handler)";
}


} // namespace vb6c3
