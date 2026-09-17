#include "backend/cgen.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <functional>

namespace vb6c3 {

// --- cgen_com.cpp: COM类工厂 + 接口vtable + RaiseEvent + 事件接收器 ---

void CCodeGen::emitClassFactory(Module& module) {
    std::string clsStruct = "vb6_cls_" + cIdent(moduleName_);  // Fix 013: VB_Name

    c_.emitBlank();
    c_.emitLine("// === 类工厂函数: " + module.moduleName + " ===");
    c_.emitBlank();

    // _New: 分配+初始化
    c_.emitLine(clsStruct + "* " + clsStruct + "_New(void) {");
    c_.indent();
    c_.emitLine(clsStruct + "* me = (" + clsStruct + "*)vb6_Alloc(sizeof(" + clsStruct + "));");
    c_.emitLine("if (!me) return NULL;");
    if (isDll_) {
        c_.emitLine("me->__comObj = NULL;  /* P6.6.3: no COM wrapper yet */");
    }

    // 初始化所有字段为默认值
    // 同时注册BSTR/Long类型成员到knownBstrVars_/knownLongVars_ (用于赋值时BSTR安全处理)
    for (auto& decl : module.declarations) {
        if (decl->kind == ASTNodeKind::VariableDecl) {
            auto& var = static_cast<VariableDecl&>(*decl);
            std::string field = cIdent(var.name);
            // 注册到类型集合 (用于后续赋值时BSTR安全处理)
            std::string lower = var.name;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (var.asType) {
                Vb6Type vtype = resolveArrayElemType(var.asType.get());
                if (vtype == Vb6Type::String) knownBstrVars_.insert(lower);
                else if (vtype == Vb6Type::Long || vtype == Vb6Type::Integer || vtype == Vb6Type::Boolean) knownLongVars_.insert(lower);
                else if (vtype == Vb6Type::Double || vtype == Vb6Type::Single) knownDoubleVars_.insert(lower);
            }
            if (var.isDynamicArray || !var.dimensions.empty()) {
                c_.emitLine("me->" + field + " = NULL;");
            } else if (var.asType) {
                Vb6Type fvt = resolveArrayElemType(var.asType.get());
                // Fix 084m: 字段C类型为指针 (void*/vb6_ComIface_X*/vb6_cls_X*/vb6_SafeArray1D*)
                // → 初始化为 NULL。resolveArrayElemType 对外部COM类型(ADODB.Connection等)返回
                // Variant, 若用 vb6_VariantEmpty() 初始化指针字段会 C2440
                // (cDataBase: me->Rs/Conn/pvWhereParams/Cmd = vb6_VariantEmpty())。
                std::string fieldCT = mapTypeRef(var.asType.get());
                if (!fieldCT.empty() && fieldCT.back() == '*') {
                    c_.emitLine("me->" + field + " = NULL;");
                } else if (fvt == Vb6Type::UserDefinedType) {
                    // Fix 038: UDT 字段不能用 = 0 初始化 (C2440), 改用 memset 零化
                    c_.emitLine("memset(&me->" + field + ", 0, sizeof(me->" + field + "));");
                } else {
                    c_.emitLine("me->" + field + " = " + defaultValue(fvt) + ";");
                }
            } else {
                // Fix 089j: VB6 无 As 类型声明的成员变量默认 Variant (C 字段
                // 类型 vb6_VARIANT), 不能 `= 0` 初始化 → C2440 (int→vb6_VARIANT),
                // 必须用 vb6_VariantEmpty() (与显式 `As Variant` 的 Tag 字段一致).
                c_.emitLine("me->" + field + " = vb6_VariantEmpty();");
            }
        }
    }

    // P6.5: 初始化事件接收器指针为NULL
    bool hasEvents = false;
    for (auto& decl : module.declarations) {
        if (decl->kind == ASTNodeKind::EventDecl) { hasEvents = true; break; }
    }
    if (hasEvents) {
        c_.emitLine("me->events = NULL;  /* P6.5: no event sink initially */");
    }

    // 检查是否有 Class_Initialize 方法
    bool hasInit = false;
    for (auto& decl : module.declarations) {
        if (decl->kind == ASTNodeKind::SubDecl) {
            auto& sub = static_cast<SubDecl&>(*decl);
            if (sub.name == "Class_Initialize") {
                hasInit = true;
                c_.emitLine(cProcName("Class_Initialize", sub.access, isClassModule_ ? moduleName_ : "") + "(me);");
                break;
            }
        }
    }

    c_.emitLine("return me;");
    c_.dedent();
    c_.emitLine("}");

    // _Destroy: 终止+释放
    c_.emitBlank();
    c_.emitLine("void " + clsStruct + "_Destroy(" + clsStruct + "* me) {");
    c_.indent();
    c_.emitLine("if (!me) return;");

    // 检查是否有 Class_Terminate 方法
    for (auto& decl : module.declarations) {
        if (decl->kind == ASTNodeKind::SubDecl) {
            auto& sub = static_cast<SubDecl&>(*decl);
            if (sub.name == "Class_Terminate") {
                c_.emitLine(cProcName("Class_Terminate", sub.access, isClassModule_ ? moduleName_ : "") + "(me);");
                break;
            }
        }
    }

    // 释放BSTR字段
    for (auto& decl : module.declarations) {
        if (decl->kind == ASTNodeKind::VariableDecl) {
            auto& var = static_cast<VariableDecl&>(*decl);
            Vb6Type varType = var.asType ? resolveArrayElemType(var.asType.get()) : Vb6Type::Variant;
            if (varType == Vb6Type::String) {
                c_.emitLine("vb6_BSTR_Free(me->" + cIdent(var.name) + ");");
            } else if (var.isDynamicArray || !var.dimensions.empty()) {
                c_.emitLine("if (me->" + cIdent(var.name) + ") vb6_SA_Destroy(me->" + cIdent(var.name) + ");");
            }
        }
    }

    c_.emitLine("vb6_Free(me);");
    c_.dedent();
    c_.emitLine("}");
}

// P6.4+: 类模块默认实例 (VB_PredeclaredId=True) 惰性单例访问器.
// VB6 为 PredeclaredId 类生成隐藏的全局默认实例, frm 里裸类名成员访问经过
// knownClassVars_ 解析, 对象表达式生成 vb6_cls_X_Default() 调用. 这里生成:
//   static vb6_cls_cTT* g_p_vb6_cls_cTT_Default = NULL;
//   vb6_cls_cTT* vb6_cls_cTT_Default(void) { ... _New() on demand ... }
void CCodeGen::emitClassDefaultInstance(Module& module) {
    if (defaultInstanceClassName(moduleName_).empty()) return;
    std::string clsStruct = "vb6_cls_" + cIdent(moduleName_);
    c_.emitBlank();
    c_.emitLine("// 默认实例 (VB_PredeclaredId=True): 惰性创建共享单例");
    c_.emitLine("static " + clsStruct + "* g_p_" + clsStruct + "_Default = NULL;");
    c_.emitLine(clsStruct + "* " + clsStruct + "_Default(void) {");
    c_.indent();
    c_.emitLine("if (!g_p_" + clsStruct + "_Default)");
    c_.emitLine("    g_p_" + clsStruct + "_Default = " + clsStruct + "_New();");
    c_.emitLine("return g_p_" + clsStruct + "_Default;");
    c_.dedent();
    c_.emitLine("}");
    c_.emitBlank();
}

// ============================================================
// P6.4: 接口 vtable + 包装类型生成 (Implements 代码生成)
// ============================================================

void CCodeGen::emitInterfaceVtable(Module& module) {
    std::string clsStruct = "vb6_cls_" + cIdent(moduleName_);  // Fix 013: VB_Name

    for (auto& impl : module.implements) {
        const std::string& ifaceName = impl->interfaceName;
        std::string ifaceId = cIdent(ifaceName);

        // 收集接口方法信息: 从实现类中查找 IFoo_MethodName 方法
        struct IfaceMethodInfo {
            std::string methodName;   // 原始方法名 (如 "Bar")
            std::string implFuncName; // 实现函数C名 (如 "vb6_Class1_IFoo_Bar")
            std::string retType;      // 返回C类型
            std::string params;       // 参数列表 (不含me, 如 "int32_t x")
            bool isSub;               // Sub vs Function
        };
        std::vector<IfaceMethodInfo> methods;

        for (auto& decl : module.declarations) {
            if (decl->kind == ASTNodeKind::SubDecl) {
                auto& sub = static_cast<SubDecl&>(*decl);
                // 检查是否为 Implements 实现方法 (IFoo_MethodName 格式)
                if (sub.name.size() > ifaceName.size() + 1 &&
                    Symbol::toLower(sub.name.substr(0, ifaceName.size() + 1)) ==
                    Symbol::toLower(ifaceName + "_")) {
                    std::string methodName = sub.name.substr(ifaceName.size() + 1);
                    std::string params = makeParamList(sub.params);
                    methods.push_back({methodName, cProcName(sub.name, sub.access, isClassModule_ ? moduleName_ : ""),
                                       "void", params, true});
                }
            } else if (decl->kind == ASTNodeKind::FunctionDecl) {
                auto& func = static_cast<FunctionDecl&>(*decl);
                if (func.name.size() > ifaceName.size() + 1 &&
                    Symbol::toLower(func.name.substr(0, ifaceName.size() + 1)) ==
                    Symbol::toLower(ifaceName + "_")) {
                    std::string methodName = func.name.substr(ifaceName.size() + 1);
                    std::string params = makeParamList(func.params);
                    std::string retType = mapTypeRef(func.returnType.get());
                    methods.push_back({methodName, cProcName(func.name, func.access, isClassModule_ ? moduleName_ : ""),
                                       retType, params, false});
                }
            } else if (decl->kind == ASTNodeKind::PropertyDecl) {
                auto& prop = static_cast<PropertyDecl&>(*decl);
                if (prop.name.size() > ifaceName.size() + 1 &&
                    Symbol::toLower(prop.name.substr(0, ifaceName.size() + 1)) ==
                    Symbol::toLower(ifaceName + "_")) {
                    std::string methodName = prop.name.substr(ifaceName.size() + 1);
                    std::string params = makeParamList(prop.params);
                    // Property Get → Function, Property Let/Set → Sub
                    // 使用prop_get_/prop_let_/prop_set_前缀
                    std::string propPrefix;
                    if (prop.propKind == ProcKind::PropertyGet) {
                        propPrefix = "prop_get_";
                        std::string retType = mapTypeRef(prop.returnType.get());
                        methods.push_back({methodName, cProcName(propPrefix + prop.name, prop.access, isClassModule_ ? moduleName_ : ""),
                                           retType, params, false});
                    } else {
                        propPrefix = (prop.propKind == ProcKind::PropertySet) ? "prop_set_" : "prop_let_";
                        methods.push_back({methodName, cProcName(propPrefix + prop.name, prop.access, isClassModule_ ? moduleName_ : ""),
                                           "void", params, true});
                    }
                }
            }
        }

        if (methods.empty()) continue;

        // 1. 生成 vtable 结构体 (函数指针表)
        std::string vtblName = "vb6_vtbl_" + ifaceId;
        h_.emitLine("// Interface vtable: " + ifaceName);
        h_.emitLine("typedef struct " + vtblName + " {");
        for (auto& m : methods) {
            std::string paramList = classMeParam();
            if (m.params != "void") {
                paramList += ", " + m.params;
            }
            h_.emitLine("    " + m.retType + " (*" + cIdent(m.methodName) + ")(" + paramList + ");");
        }
        h_.emitLine("} " + vtblName + ";");
        h_.emitBlank();

        // 2. 生成接口引用包装类型 (vtable指针 + 对象指针)
        std::string ifaceTypeName = "vb6_iface_" + ifaceId;
        h_.emitLine("typedef struct " + ifaceTypeName + " {");
        h_.emitLine("    " + vtblName + "* vtbl;");
        h_.emitLine("    void* obj;");
        h_.emitLine("} " + ifaceTypeName + ";");
        h_.emitBlank();

        // 3. 生成全局 vtable 实例 (指向实现类的接口方法)
        std::string vtblInstance = vtblName + "_for_" + cIdent(moduleName_);  // Fix 013: VB_Name
        c_.emitBlank();
        c_.emitLine("// Interface vtable instance: " + ifaceName + " for " + module.moduleName);
        c_.emitLine("static " + vtblName + " " + vtblInstance + " = {");
        c_.indent();
        for (size_t i = 0; i < methods.size(); i++) {
            std::string entry = "." + cIdent(methods[i].methodName) + " = " + methods[i].implFuncName;
            if (i < methods.size() - 1) entry += ",";
            c_.emitLine(entry);
        }
        c_.dedent();
        c_.emitLine("};");

        // 4. 生成包装函数: vb6_iface_IFoo_wrap(obj) → 创建接口引用
        h_.emitLine(ifaceTypeName + " " + ifaceTypeName + "_wrap(" + clsStruct + "* obj);");
        h_.emitBlank();
        c_.emitBlank();
        c_.emitLine(ifaceTypeName + " " + ifaceTypeName + "_wrap(" + clsStruct + "* obj) {");
        c_.indent();
        c_.emitLine(ifaceTypeName + " iface;");
        c_.emitLine("iface.vtbl = &" + vtblInstance + ";");
        c_.emitLine("iface.obj = obj;");
        c_.emitLine("return iface;");
        c_.dedent();
        c_.emitLine("}");
    }
}


// ============================================================
// P6.6: ActiveX DLL代码生成
// ============================================================


} // namespace vb6c3

