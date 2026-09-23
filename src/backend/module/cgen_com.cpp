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
    // ExeComBridge 01: 无条件初始化 (原先仅 DLL). __comObj 现在是所有类模块
    //   结构体的首字段 (见 cgen_base_generate_c_open.inc), EXE 的 _New() 与
    //   DLL 侧保持一致; 纯 EXE 下无人写读, 恒 NULL, 行为零变化.
    c_.emitLine("me->__comObj = NULL;  /* P6.6.3: no COM wrapper yet */");
    emitIfaceNewInit(module);  // tB Interface 契约 (B04): me->__iv_<I>.vt = &vb6_ivtbl_<I>_for_<C>

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

    // ExeComBridge 03: COM 实参打包函数 (每个类模块一个).
    //   `New <本工程类>` 出现在 COM 调用实参位置时 (cgen_util_com.cpp comPackExpr),
    //   通用打包 vb6_ComPackValue 会把裸 vb6_cls_X* 当 VT_DISPATCH 传出去, 对端
    //   AddRef 时把结构体首字段当 vtable → 0xC0000005
    //   (VBMAN_DEMO: .Router.Reg "Demo", New bHello).
    //   本函数先经 __comObj 包装成真 IDispatch 再转 VARIANT (RTL:
    //   vb6_ComPackVB6InstanceRaw). 类名用原始名 —— desc 表的 classVariable 即
    //   VB_Name (cgen_util_dllentry_collect.inc), 查找按 _stricmp 大小写不敏感.
    c_.emitBlank();
    c_.emitLine("// ExeComBridge 03: 工程类实例 → COM 实参 (先包装成 IDispatch 再转 VARIANT)");
    c_.emitLine("void* vb6_ComPack_" + cIdent(moduleName_) + "(void* instance) {");
    c_.indent();
    c_.emitLine("return vb6_ComPackVB6InstanceRaw(\"" + moduleName_ + "\", instance);");
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
// Fix 099: Public 字段的 COM 读写访问器 (ActiveX DLL)
// ============================================================
// 真 VB6 把类模块的 `Public X As T` 暴露为 property get/let 对 (TypeLib 里同为
// Property). 此前 C3 完全不暴露: cHttpServer 的 Router/RouteBefore/RouteAfter/
// SSE/Database/Statistics 等 Public 字段既不进 IDispatch 方法表也不进 TypeLib,
// 客户端 GetIDsOfNames("Router") 直接失败 → vb6_ComGetProp 返回 NULL → 调用方
// 拿 NULL 当对象解引用 → 0xC0000005 (段错误).
//
// 访问器必须生成在**类模块自己的 .c** 里: dll_entry.c 只有 `struct vb6_cls_X;`
// 前向声明, 属不完整类型, 无法 `me->Fld`. 命名 vb6_cls_<cls>_field_get_<fld> /
// _field_let_<fld>, 由 dll_entry.c 的字段桥接 extern 调用.
//
// 只暴露能直接映射到 vb6_VARIANT 的字段类型; Variant / UDT / 数组 / 外部 COM
// 接口指针字段一律不生成 (宁缺勿错 —— 暴露了但类型映射不对会写出编译不过或
// 语义错误的 C). 收集条件与 semantic_analyzer 的 publicFieldNames 严格一致.
void CCodeGen::emitClassFieldAccessors(Module& module) {
    // ExeComBridge 01: 放开 EXE (原先 if (!isDll_ || ...) return).
    //   EXE 工程也生成 com_entry.c (driver_codegen_dll_entry.inc), 其字段桥接
    //   extern 引用 _field_get_/_let_ —— 访问器实体必须无条件生成, 否则
    //   Instancing 非 Private 且有 Public 字段的 EXE 类 LNK2019.
    //   依赖的 RTL 符号 (vb6_ComObject_FromInstance 等) 由 vb6comserver_obj.c
    //   提供, driver_link.cpp 已无条件链入; 无 COM 引用时为死代码, 链接器剔除.
    if (!isClassModule_) return;

    const std::string clsStruct = "vb6_cls_" + cIdent(moduleName_);

    struct FieldEmit {
        std::string name;                    // 声明原名
        std::vector<std::string> getLines;   // getter 体 (写 r)
        std::vector<std::string> letLines;   // setter 体 (读 value 写 me->fld)
    };
    std::vector<FieldEmit> fields;

    for (auto& decl : module.declarations) {
        if (decl->kind != ASTNodeKind::VariableDecl) continue;
        auto& var = static_cast<VariableDecl&>(*decl);
        // 与 semantic_analyzer.cpp 的 publicFieldNames 收集条件一致
        if (var.access != AccessLevel::Public) continue;
        if (var.isWithEvents || var.isNew || var.isDynamicArray || !var.dimensions.empty()) continue;

        const std::string fld = cIdent(var.name);
        const std::string fieldCT = mapTypeRef(var.asType.get());
        const Vb6Type fvt = var.asType ? resolveArrayElemType(var.asType.get()) : Vb6Type::Variant;

        FieldEmit fe;
        fe.name = var.name;

        const bool isClassPtr = fieldCT.size() > 9
            && fieldCT.compare(0, 8, "vb6_cls_") == 0
            && fieldCT[fieldCT.size() - 1] == '*';

        if (isClassPtr) {
            // (a) 工程类实例指针字段 → VT_DISPATCH: 裸实例交 RTL 包装成 IDispatch
            std::string target = (var.asType && var.asType->kind == ASTNodeKind::SimpleTypeRef)
                ? static_cast<SimpleTypeRef*>(var.asType.get())->name : std::string();
            size_t dot = target.find_last_of('.');
            if (dot != std::string::npos) target = target.substr(dot + 1);
            if (target.empty()) continue;
            fe.getLines.push_back("r = vb6_VariantObject(vb6_ComObject_FromInstance("
                                  "vb6_FindCoClassDesc(\"" + target + "\"), (void*)me->" + fld + "));");
            fe.letLines.push_back("me->" + fld + " = (" + fieldCT + ")("
                                  "(value.vt == vb6_vtDispatch) ? "
                                  "vb6_ComObject_GetInstance(value.pdispVal) : NULL);");
        } else if (fvt == Vb6Type::String) {
            // (b) 字符串 → VT_BSTR, 必须深拷贝 (BSTR 所有权归实例)
            fe.getLines.push_back("vb6_BSTR_Assign(&r.bstrVal, me->" + fld + ");");
            fe.getLines.push_back("r.vt = vb6_vtBSTR;");
            fe.letLines.push_back("vb6_BSTR_Assign(&me->" + fld + ", "
                                  "(value.vt == vb6_vtBSTR) ? value.bstrVal : NULL);");
        } else if (fvt == Vb6Type::Boolean) {
            // (c) 布尔 → VT_BOOL (VB6 True = -1)
            fe.getLines.push_back("r = vb6_VariantBool(me->" + fld + " ? -1 : 0);");
            fe.letLines.push_back("me->" + fld + " = (value.lVal != 0) ? -1 : 0;");
        } else if (fvt == Vb6Type::Integer) {
            fe.getLines.push_back("r = vb6_VariantInt(me->" + fld + ");");
            fe.letLines.push_back("me->" + fld + " = (int16_t)value.lVal;");
        } else if (fvt == Vb6Type::Byte) {
            fe.getLines.push_back("r = vb6_VariantByte(me->" + fld + ");");
            fe.letLines.push_back("me->" + fld + " = (uint8_t)value.lVal;");
        } else if (fvt == Vb6Type::Long || fieldCT == "int32_t") {
            // (d) Long / 枚举 (mapTypeRef 对 EnumType 返回 int32_t, 底层即 Long)
            fe.getLines.push_back("r = vb6_VariantLong(me->" + fld + ");");
            fe.letLines.push_back("me->" + fld + " = (int32_t)value.lVal;");
        } else if (fvt == Vb6Type::Double) {
            fe.getLines.push_back("r = vb6_VariantDouble(me->" + fld + ");");
            fe.letLines.push_back("me->" + fld + " = (double)value.dblVal;");
        } else if (fvt == Vb6Type::Single) {
            fe.getLines.push_back("r.vt = vb6_vtSingle;");
            fe.getLines.push_back("r.fltVal = me->" + fld + ";");
            fe.letLines.push_back("me->" + fld + " = (float)value.dblVal;");
        } else if (fvt == Vb6Type::Date) {
            fe.getLines.push_back("r.vt = vb6_vtDate;");
            fe.getLines.push_back("r.dblVal = me->" + fld + ";");
            fe.letLines.push_back("me->" + fld + " = (double)value.dblVal;");
        } else if (fvt == Vb6Type::Currency) {
            fe.getLines.push_back("r.vt = vb6_vtCurrency;");
            fe.getLines.push_back("r.cyVal = me->" + fld + ";");
            fe.letLines.push_back("me->" + fld + " = (int64_t)value.cyVal;");
        } else if (fvt == Vb6Type::Object || fieldCT == "void*") {
            // (e) 外部 COM 对象 / As Object 字段 → VT_DISPATCH. 字段里存的就是接口
            //     指针 (Dictionary/Collection 等), 直接传出, 不做包装.
            fe.getLines.push_back("r = vb6_VariantObject((void*)me->" + fld + ");");
            fe.letLines.push_back("me->" + fld + " = (value.vt == vb6_vtDispatch) ? value.pdispVal : NULL;");
        }
        // 说明: 判不出可映射 C 类型的字段 (UDT / 接口值类型 / Variant / 未知外部类型)
        // 不跳过, 而是生成**空体存根** —— getter 返回 r 的初值 (Empty), setter 忽略
        // 写入. 必须生成定义的原因: dll_entry.c 在另一个编译单元里为 publicFieldNames
        // 无条件建桥接 (那边看不到这里的类型判定), 少一个定义就是 LNK2019 (实测 96 个).
        fields.push_back(fe);
    }

    if (fields.empty()) return;

    c_.emitBlank();
    c_.emitLine("// === Fix 099: Public 字段的 COM 访问器 (Property Get/Let) ===");
    c_.emitBlank();

    for (auto& fe : fields) {
        const std::string fldId = cIdent(fe.name);

        // getter: 返回 vb6_VARIANT (与 dll_entry.c 桥接的 result 缓冲布局兼容)
        c_.emitLine("vb6_VARIANT " + clsStruct + "_field_get_" + fldId + "(" + clsStruct + "* me) {");
        c_.indent();
        c_.emitLine("vb6_VARIANT r = vb6_VariantEmpty();");
        c_.emitLine("if (!me) return r;");
        for (auto& ln : fe.getLines) c_.emitLine(ln);
        c_.emitLine("return r;");
        c_.dedent();
        c_.emitLine("}");

        // setter: 从 vb6_VARIANT 取值写入字段
        c_.emitLine("void " + clsStruct + "_field_let_" + fldId + "(" + clsStruct + "* me, vb6_VARIANT value) {");
        c_.indent();
        c_.emitLine("if (!me) return;");
        for (auto& ln : fe.letLines) c_.emitLine(ln);
        c_.dedent();
        c_.emitLine("}");
        c_.emitBlank();
    }
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

