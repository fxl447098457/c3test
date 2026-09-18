#include "backend/cgen.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <functional>

namespace vb6c3 {

// --- cgen_decl.cpp: 通用声明辅助（过程数组追踪清理、过程/参数签名、枚举/类型/常量声明） ---

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
        } else if (cType == "float") {
            knownSingleVars_.insert(lower); knownDoubleVars_.insert(lower);   // Fix 117c: VT_R4
        } else if (cType == "double") {
            knownDoubleVars_.insert(lower);
        } else if (cType == "vb6_VARIANT") {
            knownVariantVars_.insert(lower);
        }
    }

    if (node.value) {
        // Fix 091d: 整型常量表达式折叠 — 避免 Variant 语义包装操作数.
        // 例: BIF_USENEWUI = BIF_RETURNONLYFSDIRS Or BIF_NEWDIALOGSTYLE →
        // ((vb6_VariantToLong(64) | vb6_VariantToLong(16))) → C2440
        // "int → vb6_VARIANT" (cDialog.c 36; 使用处报 532). 折叠为字面量后
        // 与 VB6 常量语义一致 (整型位运算).
        int64_t cv091d = 0;
        if (tryEvalConstInt(node.value.get(), cv091d)) {
            std::string folded = "(" + std::to_string(cv091d) + ")";
            if (isPublicModuleDecl(node)) {
                h_.emitLine("#define " + cName + " " + folded);
            } else {
                c_.emitLine("#define " + cName + " " + folded);
            }
        } else {
            emitExpr(*node.value);
            // 公共常量 → .h, 私有 → .c
            if (isPublicModuleDecl(node)) {
                h_.emitLine("#define " + cName + " (" + lastExpr_ + ")");
            } else {
                c_.emitLine("#define " + cName + " (" + lastExpr_ + ")");
            }
        }
    }
}

} // namespace vb6c3
