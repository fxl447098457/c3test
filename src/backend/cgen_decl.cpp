#include "backend/cgen.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <functional>

namespace vb6c3 {

// Forward declaration from cgen_base.cpp
 FrmControlType controlTypeFromName(const std::string& name);

// --- cgen_decl.cpp: 声明生成 + 签名 + Property/Event ---


// ============================================================
// 声明 visit 方法
// ============================================================

void CCodeGen::visit(SubDecl& node) {
    std::string sig = makeProcSignature(node);

    if (node.access != AccessLevel::Public) {
        c_.emitLine("static " + sig + " {");
    } else {
        c_.emitLine(sig + " {");
    }

    c_.indent();

    // 查找符号获取参数信息
    auto* sym = symTab_.lookupModule(node.name);
    currentProc_ = sym;
    currentReturnVar_ = "";

    // 清空已知数组集合 (新过程)
    knownArrays_.clear();
    arrayElemTypes_.clear();
    knownBstrVars_.clear();
    knownDoubleVars_.clear();
    knownLongVars_.clear();
    knownVariantVars_.clear();
    // P6.11: 恢复类模块成员变量类型 (clear后从持久化集合恢复)
    knownBstrVars_.insert(classBstrMembers_.begin(), classBstrMembers_.end());
    knownDoubleVars_.insert(classDoubleMembers_.begin(), classDoubleMembers_.end());
    knownLongVars_.insert(classLongMembers_.begin(), classLongMembers_.end());

    // VB6 Static Sub: 过程内所有局部变量都是static
    inStaticProc_ = node.isStatic;

    // 检测GoSub并声明返回地址栈
    hasGoSub_ = hasGoSubInStmts(node.body);
    gosubReturnCounter_ = 0;
    if (hasGoSub_) {
        c_.emitLine("int vb6_gosub_stack[32];");
        c_.emitLine("int vb6_gosub_sp = 0;");
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

    if (node.access != AccessLevel::Public) {
        c_.emitLine("static " + sig + " {");
    } else {
        c_.emitLine(sig + " {");
    }

    c_.indent();

    // 查找符号获取参数信息
    auto* sym = symTab_.lookupModule(node.name);
    currentProc_ = sym;

    // 清空已知数组集合 (新过程)
    knownArrays_.clear();
    arrayElemTypes_.clear();
    knownBstrVars_.clear();
    knownDoubleVars_.clear();
    knownLongVars_.clear();
    knownVariantVars_.clear();
    // P6.11: 恢复类模块成员变量类型 (clear后从持久化集合恢复)
    knownBstrVars_.insert(classBstrMembers_.begin(), classBstrMembers_.end());
    knownDoubleVars_.insert(classDoubleMembers_.begin(), classDoubleMembers_.end());
    knownLongVars_.insert(classLongMembers_.begin(), classLongMembers_.end());

    // VB6 Static Function: 过程内所有局部变量都是static
    inStaticProc_ = node.isStatic;

    // 检测GoSub并声明返回地址栈
    hasGoSub_ = hasGoSubInStmts(node.body);
    gosubReturnCounter_ = 0;

    // Function返回值变量
    std::string retType = mapTypeRef(node.returnType.get());
    currentReturnVar_ = "vb6_ret_" + cIdent(node.name);
    Vb6Type funcRetVb6Type = node.returnType ? typeSys_.resolveTypeName(static_cast<SimpleTypeRef*>(node.returnType.get())->name) : Vb6Type::Variant;
    c_.emitLine(retType + " " + currentReturnVar_ + " = " + defaultValue(funcRetVb6Type) + ";");
    // P6.11: 注册返回值变量类型 (用于BSTR安全赋值)
    std::string funcRetLower = currentReturnVar_;
    std::transform(funcRetLower.begin(), funcRetLower.end(), funcRetLower.begin(), ::tolower);
    if (funcRetVb6Type == Vb6Type::String) knownBstrVars_.insert(funcRetLower);
    else if (funcRetVb6Type == Vb6Type::Double) knownDoubleVars_.insert(funcRetLower);
    else if (funcRetVb6Type == Vb6Type::Long || funcRetVb6Type == Vb6Type::Integer || funcRetVb6Type == Vb6Type::Boolean) knownLongVars_.insert(funcRetLower);

    if (hasGoSub_) {
        c_.emitLine("int vb6_gosub_stack[32];");
        c_.emitLine("int vb6_gosub_sp = 0;");
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
    std::string name = cProcName(node.name, node.access);
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
    std::string name = cProcName(node.name, node.access);
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

std::string CCodeGen::makeParamList(std::vector<std::unique_ptr<ParameterDecl>>& params) {
    if (params.empty()) return "void";

    std::string result;
    for (size_t i = 0; i < params.size(); i++) {
        if (i > 0) result += ", ";
        auto& p = params[i];

        // P14.1.5: ParamArray → SAFEARRAY* (always Variant array)
        if (p->isParamArray) {
            std::string cName = cIdent(p->name);
            result += "SAFEARRAY* " + cName;
            continue;
        }

        std::string cType = mapTypeRef(p->asType.get());
        std::string cName = cIdent(p->name);

        if (p->isByVal) {
            result += cType + " " + cName;
        } else {
            // ByRef → C指针
            result += cType + "* " + cName;
        }
    }
    return result;
}

void CCodeGen::visit(EnumDecl& node) {
    std::string enumName = cIdent(node.name);

    h_.emitLine("typedef enum vb6_enum_" + enumName + " {");
    h_.indent();

    int64_t nextVal = 0;
    for (auto& member : node.members) {
        std::string memName = enumName + "_" + cIdent(member->name);
        if (member->value) {
            // 枚举成员有显式值
            emitExpr(*member->value);
            h_.emitLine("vb6_enum_" + memName + " = " + lastExpr_ + ",");
            // Try to evaluate the constant value for auto-increment
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
        } else {
            h_.emitLine("vb6_enum_" + memName + " = " + std::to_string(nextVal) + ",");
        }
        nextVal++;
    }

    h_.dedent();
    h_.emitLine("} vb6_enum_" + enumName + ";");
    h_.emitBlank();
}

void CCodeGen::visit(EnumMember& node) {
    // 由EnumDecl内部处理
}

void CCodeGen::visit(TypeDecl& node) {
    std::string typeName = cIdent(node.name);

    h_.emitLine("typedef struct vb6_type_" + typeName + " {");
    h_.indent();

    for (auto& member : node.members) {
        std::string memType = mapTypeRef(member->type.get());
        std::string memName = cIdent(member->name);
        // P15.2: 固定大小数组成员 (如 Buf(0 To 255) As Byte)
        if (member->arraySize) {
            // 评估数组上界表达式
            if (member->arraySize->kind == ASTNodeKind::LiteralExpr) {
                auto& lit = static_cast<LiteralExpr&>(*member->arraySize);
                int upperBound = (lit.literalKind == LiteralKind::Long) ? (int)lit.longValue : lit.intValue;
                h_.emitLine(memType + " " + memName + "[" + std::to_string(upperBound + 1) + "];");
            } else {
                emitExpr(*member->arraySize);
                h_.emitLine(memType + " " + memName + "[(" + lastExpr_ + ") + 1];");
            }
        } else {
            h_.emitLine(memType + " " + memName + ";");
        }
    }

    h_.dedent();
    h_.emitLine("} vb6_type_" + typeName + ";");
    h_.emitBlank();
}

void CCodeGen::visit(TypeMember& node) {
    // 由TypeDecl内部处理
}

void CCodeGen::visit(ConstDecl& node) {
    std::string cType = mapTypeRef(node.asType.get());
    std::string cName = cIdent(node.name);

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
            if (node.access == AccessLevel::Public) {
                c_.emitLine(cType + " " + cName + " = " + initCode + ";");
            } else {
                c_.emitLine("static " + cType + " " + cName + " = " + initCode + ";");
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
                c_.emitLine("{" + lb + ", " + ub + "}" + trailing);
            }
            c_.dedent();
            c_.emitLine("};");
            std::string initCode = "vb6_SafeArrayCreateND(" + saElemType + ", " + std::to_string(dimCount) + ", " + boundsVar + ")";
            if (node.access == AccessLevel::Public) {
                c_.emitLine(cType + " " + cName + " = " + initCode + ";");
            } else {
                c_.emitLine("static " + cType + " " + cName + " = " + initCode + ";");
            }
        }

        // 注册到已知数组集合
        std::string lower = node.name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        knownArrays_.insert(lower);
        arrayElemTypes_[lower] = elemType;
        arrayDimCounts_[lower] = dimCount;
        return;
    }

    // P8.1: 动态数组声明: Dim arr() As Long -> 默认1D
    if (node.isDynamicArray) {
        Vb6Type elemType = resolveArrayElemType(node.asType.get());
        std::string cType = "vb6_SafeArray1D*";

        if (node.access == AccessLevel::Public) {
            h_.emitLine("extern " + cType + " " + cName + ";");
            c_.emitLine(cType + " " + cName + " = NULL;");
        } else {
            c_.emitLine("static " + cType + " " + cName + " = NULL;");
        }

        // 注册到已知数组集合
        std::string lower = node.name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        knownArrays_.insert(lower);
        arrayElemTypes_[lower] = elemType;
        arrayDimCounts_[lower] = 1;  // 动态数组默认1D
        return;
    }

    std::string cType = mapTypeRef(node.asType.get());

    // 检查是否是类类型变量 → 注册到 knownClassVars_
    if (node.asType && node.asType->kind == ASTNodeKind::SimpleTypeRef) {
        auto& simple = static_cast<SimpleTypeRef&>(*node.asType);
        auto* clsSym = symTab_.lookupModule(simple.name);
        if (clsSym && clsSym->kind == SymbolKind::Class) {
            std::string lower = node.name;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            // P6.4: 接口类 → 注册到 knownIfaceVars_ (而非 knownClassVars_)
            if (clsSym->isInterface) {
                knownIfaceVars_[lower] = clsSym->name;
            } else {
                knownClassVars_.insert(lower);
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

    // 检查是否是Object类型变量 → 注册到 knownObjectVars_ (COM后期绑定)
    if (cType == "void*") {  // Object类型映射为void*
        std::string lower = node.name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        knownObjectVars_.insert(lower);
    }

    // P6.3: 检查是否是前期绑定COM变量 → 注册到 knownTypedComVars_
    if (node.asType && node.asType->kind == ASTNodeKind::SimpleTypeRef) {
        auto& simple = static_cast<SimpleTypeRef&>(*node.asType);
        auto* comSym = symTab_.lookupModule(simple.name);
        if (comSym && (comSym->kind == SymbolKind::ComClass || comSym->kind == SymbolKind::ComInterface)) {
            std::string lower = node.name;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            knownTypedComVars_[lower] = comSym;
            knownObjectVars_.erase(lower);  // 优先前期绑定
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
        auto* clsSym = symTab_.lookupModule(simple.name);
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
        // 判断是否是UDT类型 → 用 {0} 初始化
        bool isUdtType = false;
        if (node.asType && node.asType->kind == ASTNodeKind::SimpleTypeRef) {
            auto& simple = static_cast<SimpleTypeRef&>(*node.asType);
            auto* sym = symTab_.lookup(simple.name);
            isUdtType = (sym && sym->kind == SymbolKind::UserDefinedType);
        }
        std::string initVal;
        if (isClassType || isComIfaceType || cType == "HWND") {
            initVal = "NULL";
        } else if (isVb6IfaceType) {
            initVal = "{0}";  // P6.4: 接口引用 = {vtbl=NULL, obj=NULL}
        } else if (isUdtType) {
            initVal = "{0}";
        } else {
            initVal = defaultValue(
                node.asType ? typeSys_.resolveTypeName(static_cast<SimpleTypeRef*>(node.asType.get())->name) : Vb6Type::Variant
            );
        }
        if (node.access == AccessLevel::Public) {
            c_.emitLine(cType + " " + cName + " = " + initVal + ";");
        } else {
            c_.emitLine("static " + cType + " " + cName + " = " + initVal + ";");
        }
    }
}

void CCodeGen::visit(DeclareDecl& node) {
    // 外部函数声明 (Declare Sub/Function ... Lib "xxx" [Alias "yyy"] [CDecl])
    std::string retType = (node.procKind == ProcKind::Function)
        ? mapTypeRef(node.returnType.get()) : "void";

    std::string params = makeParamList(node.params);
    if (params.empty()) params = "void";

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

    // 生成: #pragma comment(lib, "xxx.lib")
    c_.emitLine("#pragma comment(lib, \"" + libName + ".lib\")");

    // 生成DLL导入声明 + 名称映射
    if (exportedName != cFuncIdent) {
        // 导出名≠VB6名: 声明导出名, 用#define映射
        h_.emitLine("__declspec(dllimport) " + retType + " " + callConv + " " + exportedName + "(" + params + ");");
        h_.emitLine("#define " + cFuncIdent + " " + exportedName);
    } else {
        // 导出名=VB6名: 直接声明
        h_.emitLine("__declspec(dllimport) " + retType + " " + callConv + " " + exportedName + "(" + params + ");");
    }
}

void CCodeGen::visit(PropertyDecl& node) {
    // Property Get/Let/Set → C函数
    // 类模块: 第一个参数为 me 指针
    std::string sig = makePropertySignature(node);
    c_.emitLine(sig + " {");

    // P6.6修复: 设置currentProc_ (与SubDecl/FunctionDecl相同)
    // 这确保IdentifierExpr中的类模块变量加me->前缀, PropertyGet返回值赋值正确
    auto* propSym = symTab_.lookupModule(node.name);
    currentProc_ = propSym;

    // 清空已知数组集合 (新过程)
    knownArrays_.clear();
    arrayElemTypes_.clear();
    knownBstrVars_.clear();
    knownDoubleVars_.clear();
    knownLongVars_.clear();
    knownVariantVars_.clear();
    // P6.11: 恢复类模块成员变量类型 (clear后从持久化集合恢复)
    knownBstrVars_.insert(classBstrMembers_.begin(), classBstrMembers_.end());
    knownDoubleVars_.insert(classDoubleMembers_.begin(), classDoubleMembers_.end());
    knownLongVars_.insert(classLongMembers_.begin(), classLongMembers_.end());

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
            c_.emitLine(retType + " " + currentReturnVar_ + " = " + defaultValue(retVb6Type) + ";");
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

    c_.indent();
    // P12.3: 检测On Error并声明局部错误处理
    hasOnError_ = hasOnErrorInStmts(node.body);
    if (hasOnError_) {
        c_.emitLine("jmp_buf vb6_local_err_jmp;");
        c_.emitLine("vb6_SaveErrState();");
    }
    emitStmtList(node.body);

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
    std::string propName = cProcName(prefix + node.name, node.access);
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
    std::string clsStruct = "vb6_cls_" + cIdent(baseName_);
    return clsStruct + "* me";
}


} // namespace vb6c3
