// cgen_base_naming.cpp - C3 代码生成: 默认值 / 标识符转换 / 常量查询 / 过程名
// 2026-09-17 从 src/backend/cgen_base.cpp 纯搬移（原第 1538~1746 行，逐行未改）。
// 函数: defaultValue / cIdent / resolveArrayTargetIdent / lookupConstSym / isStringConstIdent / isConstIdent / constIdentType / wrapConstArgForByRef / cProcName

#include "backend/cgen.hpp"
#include <algorithm>
#include <cstdio>
#include <cctype>
#include <iostream>
#include <functional>
#include <unordered_set>

namespace vb6c3 {

std::string CCodeGen::defaultValue(Vb6Type type) const {
    switch (type) {
        case Vb6Type::Integer:
        case Vb6Type::Long:
        case Vb6Type::Byte:
        case Vb6Type::Boolean:
        case Vb6Type::Error:
            return "0";
        case Vb6Type::Single:
        case Vb6Type::Double:
        case Vb6Type::Date:
            return "0.0";
        case Vb6Type::Currency:
            return "0.0";   // Fix 126: Currency 值语义 (double)
        case Vb6Type::String:
            return "vb6_BSTR_Empty()";
        case Vb6Type::Object:
            return "NULL";
        case Vb6Type::Variant:
        case Vb6Type::Empty:
        case Vb6Type::Null:
            return "vb6_VariantEmpty()";
        case Vb6Type::UserDefinedType:
        case Vb6Type::Unknown:
            return "0";  // Fix 010q: Enum types resolve to Unknown, default to 0
        default:
            return "0";  // Fix 010q: safe default instead of vb6_VariantEmpty()
    }
}

// ============================================================
// 标识符命名
// ============================================================

std::string CCodeGen::cIdent(const std::string& vb6Name) const {
    // VB6标识符可能含C关键字冲突, 添加前缀
    static const std::unordered_set<std::string> cKeywords = {
        "auto", "break", "case", "char", "const", "continue", "default", "do",
        "double", "else", "enum", "extern", "float", "for", "goto", "if",
        "int", "long", "register", "return", "short", "signed", "sizeof",
        "static", "struct", "switch", "typedef", "union", "unsigned", "void",
        "volatile", "while", "bool", "true", "false", "NULL",
        // C99/C11
        "inline", "restrict", "_Bool", "_Complex", "_Imaginary",
        // MSVC扩展
        "cdecl", "stdcall", "declspec", "dllimport", "dllexport",
    };

    std::string name = vb6Name;

    // 替换VB6方括号标识符 [Name] → Name
    if (!name.empty() && name.front() == '[' && name.back() == ']') {
        name = name.substr(1, name.size() - 2);
    }

    // Fix 010c: 替换VB6标识符中的特殊字符 (如版本号 ucsOsvWin8.1 → ucsOsvWin8_1)
    // C标识符只允许字母、数字、下划线
    for (auto& ch : name) {
        if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_') {
            ch = '_';
        }
    }

    // 如果是C关键字, 添加vb6_前缀
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (cKeywords.count(lower)) {
        return "vb6_" + name;
    }

    return name;
}

// Fix 084y-5: ReDim/Erase 目标的 C 标识符解析 (成员访问形态).
// 1) ".Field" — With 块成员 → _vb6_with_N->Field (与 Fix 061 一致)
// 2) "obj.Field" — UDT 成员:
//    - obj 是 ByRef UDT/数组/Variant 参数 → (*obj).Field (参数是 vb6_type_X*)
//    - obj 是 UDT 变量 (knownUdtVars_) → obj.Field (值类型)
// 否则返回 cIdent(varName) 原逻辑.
std::string CCodeGen::resolveArrayTargetIdent(const std::string& varName) {
    if (varName.size() > 1 && varName[0] == '.'
        && !withObjectVars_.empty() && !withObjectInfoStack_.empty()) {
        const auto& info = withObjectInfoStack_.back();
        if (info.kind == WithObjKind::Unknown || info.kind == WithObjKind::ClassInstance) {
            // Fix 092k: With 成员可能是多级路径 (.MessBuffer.Data) — cIdent 把
            // '.' 替换为 '_' 会生成 MessBuffer_Data (ToolsTlsThunks 1780:
            //   Erase .MessBuffer.Data → vb6_SafeArrayDestroy1D(_vb6_with_N->MessBuffer_Data)
            //   C2039 "MessBuffer_Data 不是 vb6_type_UcsTlsContext 的成员").
            // 逐段展开: 首段用 -> (With 对象是指针), 后续段保留 '.'.
            std::string path = varName.substr(1);
            std::string out = withObjectVars_.back();
            size_t pos = 0;
            bool firstSeg = true;
            while (pos <= path.size()) {
                size_t d = path.find('.', pos);
                std::string seg = (d == std::string::npos)
                    ? path.substr(pos) : path.substr(pos, d - pos);
                if (!seg.empty()) {
                    out += (firstSeg ? "->" : ".") + cIdent(seg);
                    firstSeg = false;
                }
                if (d == std::string::npos) break;
                pos = d + 1;
            }
            return out;
        }
    }
    size_t dot = varName.find('.');
    if (dot != std::string::npos && dot > 0 && dot + 1 < varName.size()) {
        std::string objName = varName.substr(0, dot);
        std::string memberName = varName.substr(dot + 1);
        std::string objLower = objName;
        std::transform(objLower.begin(), objLower.end(), objLower.begin(), ::tolower);
        if (currentProc_) {
            for (auto& p : currentProc_->params) {
                std::string pLower = p.name;
                std::transform(pLower.begin(), pLower.end(), pLower.begin(), ::tolower);
                if (pLower == objLower && !p.isByVal
                    && (p.type == Vb6Type::UserDefinedType
                        || (static_cast<uint16_t>(p.type) & static_cast<uint16_t>(Vb6Type::Array))
                        || p.type == Vb6Type::Variant)) {
                    return "(*" + objName + ")." + cIdent(memberName);
                }
            }
        }
        if (knownUdtVars_.count(objLower)) {
            return objName + "." + cIdent(memberName);
        }
    }
    return cIdent(varName);
}

// Fix 084aa: 常量符号查找.
// 模块级 Const 被生成为 #define 宏 (如 #define K (vb6_BSTR_FromStr(L"..."))),
// 对宏取址 &K 是非法的 (C2102). 局部变量同名时 lookup 优先命中局部符号,
// 返回 false → 保持 &变量 正常取址.
Symbol* CCodeGen::lookupConstSym(const std::string& name) const {
    Symbol* sym = symTab_.lookup(name);
    if (!sym) sym = symTab_.lookupModule(name);
    if (sym && sym->kind == SymbolKind::Constant) {
        return sym;
    }
    return nullptr;
}

bool CCodeGen::isStringConstIdent(const std::string& name) const {
    Symbol* sym = lookupConstSym(name);
    return sym && sym->constType == Vb6Type::String;
}

bool CCodeGen::isConstIdent(const std::string& name) const {
    return lookupConstSym(name) != nullptr;
}

Vb6Type CCodeGen::constIdentType(const std::string& name) const {
    Symbol* sym = lookupConstSym(name);
    if (sym) return sym->constType;
    return Vb6Type::Variant;
}

// Fix 084aa: 常量宏作为 ByRef 实参 — 生成可寻址复合字面量.
// 常量是 #define 宏 (可能是函数调用如 vb6_BSTR_FromStr(L"...") 或数值字面量),
// 不能 &CONST 取址, 须按形参 C 类型包装为 (&(TYPE){CONST}):
//   - ByRef Variant → (&(vb6_VARIANT){.vt=..., .xxx=CONST}) 按常量自身类型选字段
//   - ByRef String  → (&(BSTR){CONST})
//   - 其他          → (&(cType){CONST})
std::string CCodeGen::wrapConstArgForByRef(const std::string& argVal, Vb6Type paramVb6Type) const {
    if (paramVb6Type == Vb6Type::Variant || paramVb6Type == Vb6Type::Empty ||
        paramVb6Type == Vb6Type::Null || paramVb6Type == Vb6Type::Object) {
        switch (constIdentType(argVal)) {
            case Vb6Type::String:
                return "(&(vb6_VARIANT){.vt=VT_BSTR, .bstrVal=" + argVal + "})";
            case Vb6Type::Double:
            case Vb6Type::Single:
            case Vb6Type::Decimal:
            case Vb6Type::Currency:
                return "(&(vb6_VARIANT){.vt=VT_R8, .dblVal=(double)(" + argVal + ")})";
            case Vb6Type::Boolean:
                return "(&(vb6_VARIANT){.vt=VT_BOOL, .boolVal=(int16_t)(" + argVal + ")})";
            case Vb6Type::Byte:
                return "(&(vb6_VARIANT){.vt=VT_UI1, .bVal=(uint8_t)(" + argVal + ")})";
            default:
                return "(&(vb6_VARIANT){.vt=VT_I4, .lVal=(int32_t)(" + argVal + ")})";
        }
    }
    if (isStringConstIdent(argVal)) {
        return "(&(BSTR){" + argVal + "})";
    }
    std::string cType = mapType(paramVb6Type);
    if (cType.empty()) cType = "int32_t";
    return "(&(" + cType + "){" + argVal + "})";
}

std::string CCodeGen::cProcName(const std::string& procName, AccessLevel access,
                                 const std::string& sourceModule) const {
    // 跨模块函数(外部符号): vb6_<ModuleName>_<ProcName>
    if (!sourceModule.empty()) {
        return "vb6_" + cIdent(sourceModule) + "_" + cIdent(procName);
    }
    // 多模块项目:
    // - 所有函数都包含模块名, 避免与RTL函数名冲突
    //   (如用户定义 Private Sub ErrRaise 会生成 vb6_ErrRaise, 与RTL的 vb6_ErrRaise 冲突)
    // - Private标准模块函数也包含模块名 (虽然为static, 但其名可能被本模块内
    //   Err.Raise等硬编码RTL调用遮蔽, 导致参数不匹配)
    if (isMultiModule_ && !moduleName_.empty()) {
        return "vb6_" + cIdent(moduleName_) + "_" + cIdent(procName);
    }
    return "vb6_" + cIdent(procName);
}

} // namespace vb6c3
