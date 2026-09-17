#include "backend/cgen.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <functional>

namespace vb6c3 {

// --- cgen_decl_var.cpp: 变量声明生成（VariableDecl，含模块级 / 类成员 / 局部） ---

// Forward declaration from cgen_base.cpp
 FrmControlType controlTypeFromName(const std::string& name);


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
        if (isPublicModuleDecl(node)) {
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
            if (isPublicModuleDecl(node)) {
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
            if (isPublicModuleDecl(node)) {
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
        if (isPublicModuleDecl(node)) {
            h_.emitLine("extern " + cType + " " + cName + ";");
            c_.emitLine(cType + " " + cName + " = NULL;");
        } else {
            c_.emitLine("static " + cType + " " + cName + " = NULL;");
        }
        // Fix 092w: Dim arr() As Byte = <初始化表达式> (twinbasic 兼容) — 文件作用域
        // 必须用常量初始化 (C2099), 故声明为 NULL, 初始化表达式放到模块初始化函数中,
        // 与 Fix 054 静态数组的 moduleInitStmts_ 模式一致.
        if (node.initializer && elemType == Vb6Type::Byte) {
            emitExpr(*node.initializer);
            moduleInitStmts_.push_back(cName + " = " + rewriteByteArrayValue(lastExpr_) + ";");
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
                    knownNewVars_[lower] = cIdent(clsSym->name);  // Fix 090v reg
                    moduleNewVars_[lower] = cIdent(clsSym->name);  // Fix 090v
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
        // 项目类实例 (Dim b As Button / Dim WithEvents b As Button) 的 C 类型同样是 void*,
        // 但它并非 COM 后期绑定对象, 方法调用必须走直接分发 (vb6_Button_DoClick(...)).
        // 若误注册进 knownObjectVars_, cgen_expr 的"优先级1"会把它当 IDispatch 处理,
        // 生成 vb6_ComCall(b, L"DoClick", ...) → 对纯 C 结构体解引用 vtable → 运行期 0xC0000005.
        if (knownClassVars_.find(lower) == knownClassVars_.end() &&
            knownIfaceVars_.find(lower) == knownIfaceVars_.end()) {
            knownObjectVars_.insert(lower);
        }
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
                knownNewVars_[lower] = cIdent(comSym->name);  // Fix 090v reg2
                moduleNewVars_[lower] = cIdent(comSym->name);  // Fix 090v
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
        // Fix 091m: 模块级 (过程外) Variant 变量额外登记, 供过程体内回灌.
        // 类模块/窗体的成员变量不属此列 (方法体内须写 me->name, 回灌裸名会 C2065).
        if (currentProc_ == nullptr) {
            if (isClassModule_) {
                // Fix 091p: 类模块/窗体字段单独登记 — 供 me->field 形态判定
                // (cWinsock.m_vUserData As Variant: Set m_vUserData = Value 需
                // 走 Variant 容器分支, 而非 ToObjectVal 提取).
                classVariantFields_.insert(lower);
            } else {
                moduleVariantVars_.insert(lower);
            }
        }
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
    if (isPublicModuleDecl(node)) {
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
        if (isPublicModuleDecl(node)) {
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
        if (isPublicModuleDecl(node)) {
            c_.emitLine(cType + " " + cName + " = " + initVal + ";");
        } else {
            c_.emitLine("static " + cType + " " + cName + " = " + initVal + ";");
        }
    }
}

} // namespace vb6c3
