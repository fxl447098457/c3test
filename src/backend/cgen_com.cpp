#include "backend/cgen.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <functional>

namespace vb6c3 {

// --- cgen_com.cpp: COM类工厂 + 接口vtable + RaiseEvent + 事件接收器 ---

void CCodeGen::emitClassFactory(Module& module) {
    std::string clsStruct = "vb6_cls_" + cIdent(baseName_);

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
                c_.emitLine("me->" + field + " = " + defaultValue(resolveArrayElemType(var.asType.get())) + ";");
            } else {
                c_.emitLine("me->" + field + " = 0;");
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
                c_.emitLine(cProcName("Class_Initialize", sub.access) + "(me);");
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
                c_.emitLine(cProcName("Class_Terminate", sub.access) + "(me);");
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

// ============================================================
// P6.4: 接口 vtable + 包装类型生成 (Implements 代码生成)
// ============================================================

void CCodeGen::emitInterfaceVtable(Module& module) {
    std::string clsStruct = "vb6_cls_" + cIdent(baseName_);

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
                    methods.push_back({methodName, cProcName(sub.name, sub.access),
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
                    methods.push_back({methodName, cProcName(func.name, func.access),
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
                        methods.push_back({methodName, cProcName(propPrefix + prop.name, prop.access),
                                           retType, params, false});
                    } else {
                        propPrefix = (prop.propKind == ProcKind::PropertySet) ? "prop_set_" : "prop_let_";
                        methods.push_back({methodName, cProcName(propPrefix + prop.name, prop.access),
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
        std::string vtblInstance = vtblName + "_for_" + cIdent(baseName_);
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
// P6.5: RaiseEvent语句 → 生成事件回调分发代码
// ============================================================

void CCodeGen::visit(RaiseEventStmt& node) {
    // RaiseEvent EventName(args...)
    // 1. EXE内部回调: if (me->events && me->events->onEventName) { ... }
    // 2. DLL模式: vb6_FireEvent(comObj, dispid, args, argc) 广播给COM连接的接收器
    std::string evtId = cIdent(node.eventName);
    std::string callbackName = "on" + evtId;  // 事件接收器中的回调字段名

    c_.emitLine("if (me->events && me->events->" + callbackName + ") {");
    c_.indent();
    // 生成回调调用
    std::string call = "me->events->" + callbackName + "(me->events->handler";
    for (size_t i = 0; i < node.args.size(); i++) {
        emitExpr(*node.args[i]);
        call += ", " + lastExpr_;
    }
    call += ");";
    c_.emitLine(call);
    c_.dedent();
    c_.emitLine("}");

    // P6.6.3: DLL模式 - 广播事件给COM连接的接收器
    if (isDll_) {
        // 使用事件在eventNames中的索引+1作为DISPID (与driver.cpp TypeLib构建一致)
        // 因为driver中方法DISPID先分配(方法数), 然后事件DISPID从方法数+1开始递增
        // 但为简化, 我们在driver和cgen中都使用: 方法总数 + 事件索引 + 1
        // 这里使用 eventNames 索引位置即可, 因为 DISPID 只需在同一个 source dispinterface 内唯一
        int32_t evtDispid = 0;
        {
            auto* modScope = symTab_.moduleScope();
            if (modScope) {
                for (auto& [key, symPtr] : modScope->symbols()) {
                    if (!symPtr || symPtr->kind != SymbolKind::Class) continue;
                    if (symPtr->instancing == VBInstancing::Private) continue;
                    // 先尝试comEventDispids (从driver回写或从TypeLib导入)
                    std::string evtLower = node.eventName;
                    std::transform(evtLower.begin(), evtLower.end(), evtLower.begin(), ::tolower);
                    auto itDispId = symPtr->comEventDispids.find(evtLower);
                    if (itDispId != symPtr->comEventDispids.end()) {
                        evtDispid = itDispId->second;
                        break;
                    }
                    // 回退: 使用eventNames中的索引+1
                    for (size_t ei = 0; ei < symPtr->eventNames.size(); ei++) {
                        if (Symbol::toLower(symPtr->eventNames[ei]) == evtLower) {
                            evtDispid = (int32_t)(ei + 1);
                            break;
                        }
                    }
                    if (evtDispid != 0) break;
                }
            }
        }

        // 收集参数到VARIANT数组
        if (!node.args.empty()) {
            c_.emitLine("{ // vb6_FireEvent args scope");
            c_.indent();
            c_.emitLine("VARIANT __evt_args[" + std::to_string(node.args.size()) + "];");
            for (size_t i = 0; i < node.args.size(); i++) {
                c_.emitLine("VariantInit(&__evt_args[" + std::to_string(i) + "]);");
                emitExpr(*node.args[i]);
                // P15.1: 根据参数类型设置VARIANT
                Vb6Type argType = inferExprType(*node.args[i]);
                std::string idx = std::to_string(i);
                if (argType == Vb6Type::Long || argType == Vb6Type::Integer || argType == Vb6Type::Boolean) {
                    c_.emitLine("__evt_args[" + idx + "].vt = VT_I4; __evt_args[" + idx + "].lVal = (int32_t)(" + lastExpr_ + ");");
                } else if (argType == Vb6Type::Single) {
                    c_.emitLine("__evt_args[" + idx + "].vt = VT_R4; __evt_args[" + idx + "].lVal = *(int32_t*)&(float){" + lastExpr_ + "};");
                } else if (argType == Vb6Type::Double) {
                    c_.emitLine("__evt_args[" + idx + "].vt = VT_R8; __evt_args[" + idx + "].dblVal = (double)(" + lastExpr_ + ");");
                } else if (argType == Vb6Type::Object) {
                    c_.emitLine("__evt_args[" + idx + "].vt = VT_DISPATCH; __evt_args[" + idx + "].pdispVal = (IDispatch*)(" + lastExpr_ + ");");
                } else {
                    c_.emitLine("__evt_args[" + idx + "].vt = VT_BSTR; __evt_args[" + idx + "].bstrVal = vb6_BSTR_FromStr(" + lastExpr_ + ");");
                }
            }
            c_.emitLine("vb6_FireEvent((vb6_ComObject*)me->__comObj, " + std::to_string(evtDispid) + ", __evt_args, " + std::to_string(node.args.size()) + ");");
            for (size_t i = 0; i < node.args.size(); i++) {
                c_.emitLine("VariantClear(&__evt_args[" + std::to_string(i) + "]);");
            }
            c_.dedent();
            c_.emitLine("}");
        } else {
            c_.emitLine("vb6_FireEvent((vb6_ComObject*)me->__comObj, " + std::to_string(evtDispid) + ", NULL, 0);");
        }
    }
}

void CCodeGen::visit(BeepStmt& node) {
    c_.emitLine("vb6_Beep();");
}

void CCodeGen::visit(DoEventsStmt& node) {
    c_.emitLine("vb6_DoEvents();");
}

// ============================================================
// P6.5: 事件接收器表生成 (Event Sink Table)
// ============================================================

void CCodeGen::emitEventSink(Module& module) {
    std::string clsStruct = "vb6_cls_" + cIdent(baseName_);
    std::string sinkName = "vb6_events_" + cIdent(baseName_);

    // 收集所有Event声明
    struct EventInfo {
        std::string name;           // 事件名(原始)
        std::string cName;          // 安全C标识符
        std::vector<ParameterInfo> params;  // 事件参数
    };
    std::vector<EventInfo> events;

    for (auto& decl : module.declarations) {
        if (decl->kind == ASTNodeKind::EventDecl) {
            auto& evt = static_cast<EventDecl&>(*decl);
            EventInfo info;
            info.name = evt.name;
            info.cName = cIdent(evt.name);
            for (auto& param : evt.params) {
                ParameterInfo pi;
                pi.name = param->name;
                // 用mapTypeRef获取C类型, 同时从TypeSystem获取Vb6Type
                std::string cType = mapTypeRef(param->asType.get());
                pi.type = (param->asType && param->asType->kind == ASTNodeKind::SimpleTypeRef)
                    ? typeSys_.resolveTypeName(static_cast<SimpleTypeRef*>(param->asType.get())->name)
                    : Vb6Type::Variant;
                if (pi.type == Vb6Type::Unknown || pi.type == Vb6Type::Empty)
                    pi.type = Vb6Type::Variant;
                // 事件参数总是ByVal传递(跨对象边界)
                pi.isByVal = true;
                info.params.push_back(pi);
            }
            events.push_back(std::move(info));
        }
    }

    if (events.empty()) return;

    // 1. 生成事件回调函数指针typedef
    // Fix 010: typedef名称包含类名前缀, 避免不同类同名事件(但不同签名)的typedef冲突 (C2370/C2040/C2371)
    h_.emitLine("// P6.5: Event callback function pointer types");
    for (auto& evt : events) {
        std::string cbName = "vb6_evt_" + cIdent(baseName_) + "_" + evt.cName + "_cb";
        std::string sig = "void (*" + cbName + ")(void* handler";
        for (auto& p : evt.params) {
            sig += ", " + mapType(p.type) + " " + cIdent(p.name);
        }
        sig += ")";
        h_.emitLine("typedef " + sig + ";");
    }
    h_.emitBlank();

    // 2. 生成事件接收器表结构体
    h_.emitLine("// Event sink table: " + module.moduleName);
    h_.emitLine("typedef struct " + sinkName + " {");
    h_.emitLine("    void* handler;  /* event handler object (consumer) */");
    for (auto& evt : events) {
        std::string cbName = "vb6_evt_" + cIdent(baseName_) + "_" + evt.cName + "_cb";
        h_.emitLine("    " + cbName + " on" + evt.cName + ";  /* Event " + evt.name + " */");
    }
    h_.emitLine("} " + sinkName + ";");
    h_.emitBlank();
}

void CCodeGen::visit(ParameterDecl& node) {
    // 由makeParamList内部处理
}

// ============================================================
// P6.6: ActiveX DLL代码生成
// 为ActiveX DLL工程生成COM服务端代码:
//   1. coclass描述表 (g_vb6_coclasses[])
//   2. IDispatch方法描述 (g_vb6_disp_<Class>Methods[])
//   3. IDispatch方法调用桥接 (vb6_disp_<Class>_<Method>_invoke)
//   4. DllGetClassObject / DllCanUnloadNow
//   5. DllRegisterServer / DllUnregisterServer
//   6. .def导出文件
// ============================================================
// P13.23: 外部COM vtable source interface 事件接收器生成
// ============================================================

void CCodeGen::emitComVtableSinkDecls() {
    for (auto& [varLower, srcClassName] : knownWithEventsVars_) {
        auto* srcClsSym = symTab_.lookup(srcClassName);
        if (!srcClsSym || srcClsSym->kind != SymbolKind::ComClass) continue;
        if (!srcClsSym->comHasSourceIface) continue;
        if (srcClsSym->comSourceIfaceIsDispOnly) continue;  // dispinterface uses vb6_CreateEventSink
        if (srcClsSym->comSourceMethods.empty()) continue;
        if (srcClsSym->comSourceIfaceIid.empty()) continue;
        std::string createFn = "vb6_vsink_" + varLower + "_create";
        h_.emitLine("void* " + createFn + "(void);");
        h_.emitBlank();
    }
}

static std::string comEventParamExpr(const std::string& comName, Vb6Type uhType) {
    // 将 COM vtable 参数转换为 VB6 用户处理器参数表达式
    bool isArray = (static_cast<uint16_t>(uhType) & static_cast<uint16_t>(Vb6Type::Array)) != 0;
    Vb6Type baseType = isArray
        ? static_cast<Vb6Type>(static_cast<uint16_t>(uhType) & ~static_cast<uint16_t>(Vb6Type::Array))
        : uhType;
    if (isArray) {
        if (baseType == Vb6Type::Byte) {
            return "(" + comName + " ? (uint8_t*)" + comName + "->pvData : NULL)";
        } else {
            return "(" + comName + " ? (void*)" + comName + "->pvData : NULL)";
        }
    }
    return comName;
}

void CCodeGen::emitComVtableSinks() {
    for (auto& [varLower, srcClassName] : knownWithEventsVars_) {
        auto* srcClsSym = symTab_.lookup(srcClassName);
        if (!srcClsSym || srcClsSym->kind != SymbolKind::ComClass) continue;
        if (!srcClsSym->comHasSourceIface) continue;
        if (srcClsSym->comSourceIfaceIsDispOnly) continue;  // dispinterface uses vb6_CreateEventSink
        if (srcClsSym->comSourceMethods.empty()) continue;
        if (srcClsSym->comSourceIfaceIid.empty()) continue;

        std::string iidStr = srcClsSym->comSourceIfaceIid;
        std::string sinkType = "vb6_vsink_" + varLower;
        std::string sinkPrefix = "vb6_vsink_" + varLower;
        std::string iidConst = sinkPrefix + "_iid";
        std::string guidInit = emitGuidInitializer(iidStr);
        if (guidInit.empty()) continue;

        // 查找原始变量名(保留大小写)
        std::string varName = varLower;
        auto* varSym = symTab_.lookup(varLower);
        if (varSym) varName = varSym->name;

        // 1. GUID 常量
        c_.emitBlank();
        c_.emitLine("// P13.23: vtable event sink for " + varName + " (" + srcClassName + ")");
        c_.emitLine("static const IID " + iidConst + " = " + guidInit + ";");

        // 2. Sink 结构体
        c_.emitLine("typedef struct " + sinkType + " {");
        c_.emitLine("    void** vtable;");
        c_.emitLine("    LONG refCount;");
        c_.emitLine("} " + sinkType + ";");

        // Forward declarations (used by QI before definition)
        c_.emitBlank();
        c_.emitLine("static ULONG __stdcall " + sinkPrefix + "_AddRef(void* This);");
        c_.emitLine("static ULONG __stdcall " + sinkPrefix + "_Release(void* This);");

        // 3. IUnknown methods
        // Note: QI does NOT respond to IID_IDispatch because this sink has no
        // Invoke implementation — only IUnknown and the source IID are supported.
        c_.emitBlank();
        c_.emitLine("static HRESULT __stdcall " + sinkPrefix + "_QI(void* This, REFIID riid, void** ppv) {");
        c_.indent();
        c_.emitLine("if (IsEqualIID(riid, \u0026IID_IUnknown) || IsEqualIID(riid, \u0026" + iidConst + ")) {");
        c_.indent();
        c_.emitLine("*ppv = This;");
        c_.emitLine(sinkPrefix + "_AddRef(This);");
        c_.emitLine("return S_OK;");
        c_.dedent();
        c_.emitLine("}");
        c_.emitLine("*ppv = NULL;");
        c_.emitLine("return E_NOINTERFACE;");
        c_.dedent();
        c_.emitLine("}");

        c_.emitBlank();
        c_.emitLine("static ULONG __stdcall " + sinkPrefix + "_AddRef(void* This) {");
        c_.indent();
        c_.emitLine("return InterlockedIncrement(\u0026((" + sinkType + "*)This)->refCount);");
        c_.dedent();
        c_.emitLine("}");

        c_.emitBlank();
        c_.emitLine("static ULONG __stdcall " + sinkPrefix + "_Release(void* This) {");
        c_.indent();
        c_.emitLine("ULONG c = InterlockedDecrement(\u0026((" + sinkType + "*)This)->refCount);");
        c_.emitLine("if (c == 0) CoTaskMemFree(This);");
        c_.emitLine("return c;");
        c_.dedent();
        c_.emitLine("}");

        // 4. Source interface 方法
        for (auto& evtName : srcClsSym->eventNames) {
            std::string evtLower = Symbol::toLower(evtName);
            auto itSig = srcClsSym->comSourceMethods.find(evtLower);
            if (itSig == srcClsSym->comSourceMethods.end()) continue;
            auto& sig = itSig->second;

            std::string handlerName = varName + "_" + evtName;
            auto* handlerSym = symTab_.lookup(handlerName);
            if (!handlerSym) continue;

            std::string methodName = sinkPrefix + "_" + cIdent(evtName);
            std::string methodSig = "static HRESULT __stdcall " + methodName + "(void* This";
            std::string callArgs = "(";
            for (size_t i = 0; i < sig.params.size(); i++) {
                std::string comType = mapComType(sig.params[i].type);
                std::string comName = "com_" + cIdent(sig.params[i].name);
                methodSig += ", " + comType + " " + comName;
                if (i > 0) callArgs += ", ";
                callArgs += comEventParamExpr(comName, sig.params[i].type);
            }
            methodSig += ")";
            callArgs += ")";

            c_.emitBlank();
            c_.emitLine(methodSig + " {");
            c_.indent();
            std::string procCall = cProcName(handlerName, handlerSym->access, handlerSym->sourceModule);
            c_.emitLine(procCall + callArgs + ";");
            c_.emitLine("return S_OK;");
            c_.dedent();
            c_.emitLine("}");
        }

        // 5. vtable 数组
        c_.emitBlank();
        c_.emitLine("static void* " + sinkPrefix + "_vtable[] = {");
        c_.indent();
        c_.emitLine(sinkPrefix + "_QI, " + sinkPrefix + "_AddRef, " + sinkPrefix + "_Release,");
        for (auto& evtName : srcClsSym->eventNames) {
            std::string evtLower = Symbol::toLower(evtName);
            if (srcClsSym->comSourceMethods.find(evtLower) == srcClsSym->comSourceMethods.end()) continue;
            c_.emitLine(sinkPrefix + "_" + cIdent(evtName) + ",");
        }
        c_.dedent();
        c_.emitLine("};");

        // 6. create 函数
        c_.emitBlank();
        c_.emitLine("void* " + sinkPrefix + "_create(void) {");
        c_.indent();
        c_.emitLine(sinkType + "* s = (" + sinkType + "*)CoTaskMemAlloc(sizeof(" + sinkType + "));");
        c_.emitLine("if (!s) return NULL;");
        c_.emitLine("memset(s, 0, sizeof(" + sinkType + "));");
        c_.emitLine("s->vtable = " + sinkPrefix + "_vtable;");
        c_.emitLine("s->refCount = 1;");
        c_.emitLine("return s;");
        c_.dedent();
        c_.emitLine("}");
    }
}


// ============================================================
// P6.6: ActiveX DLL代码生成
// ============================================================


} // namespace vb6c3

