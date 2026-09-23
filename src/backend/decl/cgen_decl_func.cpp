#include "backend/cgen.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <functional>

namespace vb6c3 {

// --- cgen_decl_func.cpp: FunctionDecl 声明生成 ---
// 由 src/backend/decl/cgen_decl_proc.cpp 拆出（2026-09-17），纯搬移、零行为改动。


void CCodeGen::visit(FunctionDecl& node) {
    // 泛型模板 (tB, G2/G3): 模板本体不发码 (泛型器注入特化副本)
    if (!node.typeParams.empty()) return;
    std::string sig = makeProcSignature(node);

    // Fix 055: Form事件处理函数不能为static, 因为wndproc用extern引用它们
    bool isFormEventFunc = isFormModule_ && node.name.find("Form_") == 0;
    // Fix 089e: 仅 Private 成员编译为 static (Friend/Public 跨模块可调用)
    if (node.access == AccessLevel::Private && !isFormEventFunc) {
        c_.emitLine("static " + sig + " {");
    } else {
        c_.emitLine(sig + " {");
    }

    c_.indent();

    // 查找符号获取参数信息 (重载组内按声明位置取本变体, 无重载时等价旧 lookupModule)
    auto* sym = symTab_.lookupModuleOverloadByLoc(node.name, node.loc);
    currentProc_ = sym;

    // Fix 056b: 清理局部数组注册 (模块级/类成员数组跨过程保留)
    clearProcArrayTracking();
    ansiTempsToFree_.clear();
    ansiCounter_ = 0;
    knownBstrVars_.clear();
    knownDoubleVars_.clear();
    knownSingleVars_.clear();
    knownDateVars_.clear();   // Fix 175
    knownLongVars_.clear();
    knownLongPtrVars_.clear();  // Bug #2 fix: 也清空LongPtr集合
    knownVariantVars_.clear();
    // M22-fix: 只清空UDT变量map(旧条目会冲突), set类型不清空(WithEvents等模块级条目需跨过程保留)
    knownUdtVars_.clear();
    knownFixedStringLen_.clear();
    // Fix 010o: 清空局部变量集合
    knownLocalVars_.clear();
    knownNewVars_.clear();
    knownNewVars_.insert(moduleNewVars_.begin(), moduleNewVars_.end());  // Fix 090v
    // Fix 091m: 回灌模块级 Variant 变量 (knownVariantVars_ 已被 clear)
    knownVariantVars_.insert(moduleVariantVars_.begin(), moduleVariantVars_.end());
    knownByRefParams_.clear();  // Fix 081g
    // Fix 160w: 过程入口重置 COM 属性派发标记与 With 栈 — 上一个过程的 WithMemberExpr
    // 值 (如 `.Result`, `PropCellFont.Size`) 只把对象表达式留在 lastExpr_, 成员名挂
    // 在 comObjExpr_/comMemberName_/isComMarker_… 若不清, 会泄漏到下一个过程被其
    // BinaryExpr/赋值误当作 COM 属性读取 → C2065 (_vb6_with_14/_vb6_with_15,
    // Command5_Click -> Command10_Click) / C2440 (vb6_ComIface_Font*→float).
    isComMarker_ = false;
    comObjExpr_.clear();
    comMemberName_.clear();
    isEarlyBoundCom_ = false;
    earlyBoundSym_ = nullptr;
    withObjectVars_.clear();
    withObjectInfoStack_.clear();
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
            // Fix 123b: Currency/Single/Date 形参同属 C double 组 — 此前漏注册,
            // inferExprType(形参) 落符号表查找 (129 模块工程里同名符号撞车) 或
            // Variant → RaiseEvent 实参打包误走 BSTR 分支 (cZipArchive
            // frFireProgress 的 Current/Total As Currency → vb6_BSTR_FromStr(double)
            // C2440, cZipArchive.c 2341/2343).
            else if (paramType == Vb6Type::Double || paramType == Vb6Type::Currency
                     || paramType == Vb6Type::Single || paramType == Vb6Type::Date) {
                knownDoubleVars_.insert(pLower);
                // Fix 175: Date 形参额外登记 —— C 层 Date/Double 同型, 只按
                // C 类型串分派会让 inferExprType 看不见 Date (打出序列号)。
                if (paramType == Vb6Type::Date) knownDateVars_.insert(pLower);
            }
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
    // Fix 088c: 函数返回类实例 → 注册返回值变量 (vb6_ret_X) 到 knownClassVars_,
    // 使函数体内 FuncName.Method(...)/FuncName.Field 走类成员分发.
    // 此前该变量未注册, MemberAccessExpr 主 fallback 找不到 → 生成
    // vb6_ret_X->Method (C2039: Method 不是 vb6_cls_X 的成员).
    if (retType.rfind("vb6_cls_", 0) == 0) {
        std::string clsName088c = retType.substr(8);  // strip "vb6_cls_" (8 chars)
        if (!clsName088c.empty() && clsName088c.back() == '*') clsName088c.pop_back();
        knownClassVars_[funcRetLower] = clsName088c;
    }
    // Fix 089c: 函数返回内置 COM 对象 (As Collection / As Object → C void*)
    // 时注册返回值变量到 knownObjectVars_, 使函数体内 FuncName.Add(...)/
    // FuncName.Remove(...) 走 COM dispatch (vb6_ComCall) 而非结构成员调用
    // (C2224: vb6_ret_json_ParseArray.Add — json_ParseArray As Collection).
    // 与变量注册 (941-946: cType=="void*" → knownObjectVars_) 对齐.
    else if (retType == "void*") {
        knownObjectVars_.insert(funcRetLower);
    }
    // Fix 090i: 函数返回 UDT → 注册返回值变量到 knownUdtVars_ (如 pvVfsOpen /
    // pvVfsCreate / pvArrPtr 等 As ZipVfsType 的内部函数). 此前漏注册, 函数体内
    // vb6_ret_X.Field 的字段类型推断失败 (inferExprType → inferUdtTypeOfExpr
    // 查 knownUdtVars_ 落空 → 字段按 Unknown/Variant 处理), UDT 内 Variant/
    // LongPtr 字段被误当 String (包装 .vt=VT_BSTR 复合字面量传 ByRef Variant
    // 形参) / SafeArray (UBound/ReDim 直接把 VARIANT 字段传 SafeArray*) /
    // Variant (VariantToLong 解包 LongPtr 字段) → C2440/C2198 (cZipArchive
    // pvVfsOpen/pvVfsCreate 函数簇 22 错).
    if (retType.rfind("vb6_type_", 0) == 0) {
        knownUdtVars_[funcRetLower] = retType;
    }

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
    // Fix 086: 先将块内 Dim/Const 提升到过程顶部 (VB6 局部声明是过程级作用域)
    hoistLocalDecls(node.body);
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

} // namespace vb6c3
