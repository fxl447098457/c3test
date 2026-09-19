#include "backend/cgen.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <functional>

namespace vb6c3 {

// --- cgen_decl_prop.cpp: 属性与事件声明生成（PropertyDecl / EventDecl + 属性签名） ---


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
    knownSingleVars_.clear();
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
    // Fix 010r/010r-10: 类模块中注册me到knownClassVars_ (使Me.Method()正确分发)
    // 改为map赋值: me → 当前模块名(类名)
    if (isClassModule_) knownClassVars_["me"] = moduleName_;
    // P6.11: 恢复类模块成员变量类型 (clear后从持久化集合恢复)
    knownBstrVars_.insert(classBstrMembers_.begin(), classBstrMembers_.end());
    knownDoubleVars_.insert(classDoubleMembers_.begin(), classDoubleMembers_.end());
    knownLongVars_.insert(classLongMembers_.begin(), classLongMembers_.end());
    // Fix 140: 恢复类模块 Byte() 数组成员 (knownByteArrayVars_ 被 clear 后需恢复).
    knownByteArrayVars_.insert(classByteArrayMembers_.begin(), classByteArrayMembers_.end());
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
            // czUI fix: Single 参数也要登记, 否则被当成 Variant 走 VarCmpLong 通道
            // (把 float* 当 vb6_VARIANT* 解引用, 比较结果为垃圾 — FontSize 钳成 1px)
            else if (paramType == Vb6Type::Single) knownSingleVars_.insert(pLower);
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
    // Fix 086: 先将块内 Dim/Const 提升到过程顶部 (VB6 局部声明是过程级作用域)
    hoistLocalDecls(node.body);
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
