#include "backend/cgen.hpp"
#include <algorithm>
#include <cstdio>
#include <cctype>
#include <iostream>
#include <functional>

namespace vb6c3 {

// --- cgen_base.cpp: 辅助函数 + CodeEmitter + 构造 + generate + 类型/命名映射 ---

FrmControlType controlTypeFromName(const std::string& name) {
    static const std::unordered_map<std::string, FrmControlType> map = {
        {"commandbutton", FrmControlType::CommandButton},
        {"textbox", FrmControlType::TextBox},
        {"label", FrmControlType::Label},
        {"checkbox", FrmControlType::CheckBox},
        {"optionbutton", FrmControlType::OptionButton},
        {"listbox", FrmControlType::ListBox},
        {"combobox", FrmControlType::ComboBox},
        {"hscrollbar", FrmControlType::HScrollBar},
        {"vscrollbar", FrmControlType::VScrollBar},
        {"frame", FrmControlType::Frame},
        {"timer", FrmControlType::Timer},
        {"picturebox", FrmControlType::PictureBox},
        {"image", FrmControlType::Image},
    };
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    auto it = map.find(lower);
    return (it != map.end()) ? it->second : FrmControlType::Unknown;
}


// ============================================================
// CodeEmitter
// ============================================================

void CodeEmitter::emitLine(const std::string& line) {
    for (int i = 0; i < indentLevel_; i++) {
        oss_ << "    ";  // 4空格缩进
    }
    oss_ << line << "\n";
}

void CodeEmitter::emit(const std::string& text) {
    oss_ << text;
}

void CodeEmitter::emitBlank() {
    oss_ << "\n";
}

// ============================================================
// CCodeGen 构造
// ============================================================

CCodeGen::CCodeGen(Diagnostics& diag, const SymbolTable& symTab,
                   const TypeSystem& typeSys,
                   const std::unordered_map<std::string, std::set<std::string>>* classVoidFieldMap,
                   const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>* classTypedFieldMap,
                   const std::unordered_set<std::string>* variantReturnFuncs,
                   bool verbose)
    : diag_(diag), symTab_(symTab), typeSys_(typeSys),
      classVoidFieldMap_(classVoidFieldMap), classTypedFieldMap_(classTypedFieldMap),
      variantReturnFuncs_(variantReturnFuncs), verbose_(verbose) {}

// ============================================================
// 主入口: 生成 .h + .c
// ============================================================

bool CCodeGen::generate(Module& module, const std::string& baseName,
                         const std::unordered_set<std::string>& externalModules,
                         bool isDll, const std::string& dllProgId,
                         const FrmFormDesc* frmDesc) {
    currentModule_ = &module;
    baseName_ = baseName;
    moduleName_ = module.moduleName;  // Fix 013: 模块名 = VB_Name (规范名称), 与 clsSym->name 一致
    isMultiModule_ = !externalModules.empty();  // 有外部依赖 = 多模块项目
    externalModules_ = externalModules;  // M22: 保存外部模块集合
    isClassModule_ = module.isClassModule;
    isFormModule_ = module.isFormModule;
    isDll_ = isDll;  // P6.6
    dllProgId_ = dllProgId;  // P6.6
    emittedSymbols_.clear();
    labelCounter_ = 0;
    tempCounter_ = 0;

    // P6.11: 类模块成员变量类型扫描 (用于方法体内的BSTR安全赋值)
    // 生成代码时类成员的C名格式为 m_Xxx (首字母大写), lowercase 后为 m_xxx
    classBstrMembers_.clear();
    classLongMembers_.clear();
    classDoubleMembers_.clear();
    classVariantMembers_.clear();  // Fix 037
    classUdtMembers_.clear();  // Fix 010n
    classMemberVars_.clear();  // Fix 010r
    if (isClassModule_) {
        for (auto& decl : module.declarations) {
            if (decl->kind == ASTNodeKind::VariableDecl) {
                auto& var = static_cast<VariableDecl&>(*decl);
                // Fix 010r: Register ALL class member variable names (lowercase)
                {
                    std::string mLower = "m_" + var.name;
                    std::transform(mLower.begin(), mLower.end(), mLower.begin(), ::tolower);
                    std::string oLower = var.name;
                    std::transform(oLower.begin(), oLower.end(), oLower.begin(), ::tolower);
                    classMemberVars_.insert(mLower);
                    classMemberVars_.insert(oLower);
                }
                if (var.asType) {
                    Vb6Type vtype = resolveArrayElemType(var.asType.get());
                    // 注册 C 字段名 m_Xxx 的 lowercase: m_xxx
                    std::string mLower = "m_" + var.name;
                    std::transform(mLower.begin(), mLower.end(), mLower.begin(), ::tolower);
                    // 同时注册不带 m_ 前缀的原始名 lowercase
                    std::string oLower = var.name;
                    std::transform(oLower.begin(), oLower.end(), oLower.begin(), ::tolower);
                    if (vtype == Vb6Type::String) {
                        classBstrMembers_.insert(mLower);
                        classBstrMembers_.insert(oLower);
                    } else if (vtype == Vb6Type::Long || vtype == Vb6Type::Integer || vtype == Vb6Type::Boolean) {
                        classLongMembers_.insert(mLower);
                        classLongMembers_.insert(oLower);
                    } else if (vtype == Vb6Type::Double || vtype == Vb6Type::Single) {
                        classDoubleMembers_.insert(mLower);
                        classDoubleMembers_.insert(oLower);
                    }
                    // Fix 037: Variant 类成员注册到 classVariantMembers_.
                    // 用于 IndexOrCallExpr 处理 me->VarField(idx) / obj.VarField(args),
                    // 应当 emit vb6_VariantArrayGet(&me->VarField, idx) 而非把 Variant
                    // 字段当函数调用 (C2064 term does not evaluate to a function).
                    else if (vtype == Vb6Type::Variant) {
                        classVariantMembers_.insert(mLower);
                        classVariantMembers_.insert(oLower);
                    }
                        // Fix 010n: 记录UDT类型成员变量 (用于With块类型检测)
                        if (var.asType->kind == ASTNodeKind::SimpleTypeRef) {
                            auto& simple = static_cast<SimpleTypeRef&>(*var.asType);
                            auto* udtSym = lookupDotted(simple.name);
                            if (udtSym && udtSym->kind == SymbolKind::UserDefinedType) {
                                std::string udtCType = "vb6_type_" + cIdent(simple.name);
                                classUdtMembers_[oLower] = udtCType;
                                classUdtMembers_[mLower] = udtCType;
                            }
                            // NOTE: 类类型成员变量 (Private WithEvents m_oSocket As cTlsSocket 等)
                            // 由 visit(VariableDecl) 在 trackOnly_=true 路径 (cgen_decl.cpp:605-628)
                            // 统一注册到 knownClassVars_, 此处无需重复注册.
                            // Fix 014 改在 cgen_expr.cpp visit(MemberAccessExpr) 通用 fallback 中
                            // 提取 C 表达式尾部标识符以支持链式访问 (me.m_oSocket.Create).
                        }
                }
            }
        }
    }

    // 生成 .h 头文件
    std::string guard = "VB6CGEN_" + cIdent(baseName_) + "_H";
    for (auto& c : guard) { if (!isalnum((unsigned char)c) && c != '_') c = '_'; }  // 文件名可能含.等非标识符字符
    std::transform(guard.begin(), guard.end(), guard.begin(), ::toupper);

    h_.emitLine("// Generated by vb6c3 (C3.exe) from " + module.filename);
    h_.emitLine("#ifndef " + guard);
    h_.emitLine("#define " + guard);
    h_.emitBlank();
    h_.emitLine("#include <stdint.h>");
    h_.emitLine("#include <stddef.h>");
    h_.emitLine("#include <stdbool.h>");
    h_.emitLine("#include <wchar.h>");
    // P7: 窗体模式下windows.h必须在vb6rtl.h之前, 避免VARIANT重定义冲突
    if (module.isFormModule) {
        h_.emitLine("#include <windows.h>");
        h_.emitLine("#include \"vb6forms.h\"");
    }
    h_.emitLine("#include \"vb6rtl.h\"");
    // Fix 020: 取消 C/Windows 标准宏定义, 防止与 VB6 字段/方法名冲突
    // 例: stdio.h 的 EOF 宏让 Db->Rs.EOF 被预处理展开为 Db->Rs.(-1) → C2059 语法错误
    // vb6rtl.h 此前的 transitive include (stdio.h/windows.h/oleauto.h 等) 已完成声明,
    // 此后生成的 C 代码不会以宏形式使用这些名字 (所有出现都是字符串字面量或字段访问).
    // 列表保守, 仅含确认会冲突或极可能冲突的标准宏:
    //   EOF (stdio.h — ADO Recordset 字段) — 已确认在 vbman 中冲突 11 处
    //   ERROR (winerror.h — VB6 常用作属性/枚举名)
    //   DELETE (winuser.h MF_DELETE — ADO Command.Delete 等)
    //   min / max (windef.h 宏 — VB6 大小写不敏感, Min/Max/Field.Min 等都会撞)
    //   OPTIONAL / IN / OUT (sal.h — 注解宏, 与 VB6 参数名冲突)
    //   BEEP (winuser.h — VB6 内置 Beep 语句虽不冲突, 但用户可能定义 BEEP 字段)
    {
        static const char* kClashMacros[] = {
            "EOF", "ERROR", "DELETE", "min", "max",
            "OPTIONAL", "IN", "OUT", "BEEP",
            // Fix 056: Windows SDK COM interface IID macros clash with VB6 array variables
            // e.g. Private IID_IPersistStream(0 To 3) As Long
            "IID_IPersistStream", "IID_IPicture"
        };
        // 使用 #ifdef/#undef 对保护: 未定义的宏也安全跳过
        h_.emitLine("/* Fix 020: undef C/Windows macros clashing with VB6 identifiers */");
        for (auto m : kClashMacros) {
            h_.emitLine(std::string("#ifdef ") + m);
            h_.emitLine(std::string("#undef ") + m);
            h_.emitLine("#endif");
        }
        // Fix 059: Windows SDK extern declarations (not macros) also clash with VB6 variable names.
        // #undef only removes macro definitions, but IID_IPersistStream / IID_IPicture are
        // declared as 'extern const GUID' in objidl.h / olectl.h. We must #define-remap them
        // so our generated C code uses a different identifier name.
        static const char* kClashExterns[] = {
            "IID_IPersistStream", "IID_IPicture"
        };
        h_.emitLine("/* Fix 059: remap Windows SDK extern names that clash with VB6 array vars */");
        for (auto m : kClashExterns) {
            std::string remap = "vb6_arr_" + std::string(m);
            h_.emitLine("#define " + std::string(m) + " " + remap);
        }
    }
    // Fix 010r-11: 注册外部模块的类实例变量到 knownClassVars_
    // 遍历符号表中所有外部 Variable 符号, 检查 variableTypeName 是否是类名
    // 如 ToolsStr As New cToolsStr → variableTypeName = "cToolsStr"
    // 这样在 MemberAccessExpr 中, ToolsStr.HasStr 能正确分发为 vb6_cToolsStr_HasStr(ToolsStr, ...)
    for (const auto& [key, sym] : symTab_.moduleScope()->symbols()) {
        if (sym->isExternal && sym->kind == SymbolKind::Variable && !sym->variableTypeName.empty()) {
            auto* clsSym = symTab_.lookupModule(sym->variableTypeName);
            if (clsSym && clsSym->kind == SymbolKind::Class && !clsSym->isInterface) {
                std::string varLower = sym->name;
                std::transform(varLower.begin(), varLower.end(), varLower.begin(), ::tolower);
                knownClassVars_[varLower] = clsSym->name;
                // P14.3.1: Dim As New 自动实例化 — 外部 As New 变量也需要注册
                // 检查源Symbol是否有 As New 标志 (通过 knownNewVars_ 传递)
                // 对于 Public x As New ClassName, 在 consuming 模块中也需要 auto-instantiate
                // 暂时不注册 knownNewVars_, 因为 As New 信息没有传递到外部 Symbol
            }
        }
    }

    // Fix 010r-12: Enum和Type(UDT)定义必须在跨模块#include之前生成
    // 原因: 类模块(如cTlsSocket)的.h需要标准模块(如mdTlsThunks)的UDT完整定义
    // 作为类结构体值成员. 若UDT定义在#include之后, 循环引用会导致UDT不可见.
    // UDT不引用类类型(只含基本类型/其他UDT/指针), 可安全地先输出.
    // 1. Enum定义 (必须最先, 因为常量可能引用枚举值)
    for (auto& decl : module.declarations) {
        if (decl->kind == ASTNodeKind::EnumDecl) {
            visit(static_cast<EnumDecl&>(*decl));
        }
    }

    // 2. Type(UDT)定义 — 必须在跨模块#include之前, 确保循环引用时UDT定义先于include可见
    for (auto& decl : module.declarations) {
        if (decl->kind == ASTNodeKind::TypeDecl) {
            visit(static_cast<TypeDecl&>(*decl));
        }
    }

    // P8.7 + Fix 010r-12: 跨模块#include策略
    // - 类模块引用任何模块(类或标准) → .h (类结构体需要完整UDT定义和类类型)
    // - 标准模块引用类模块 → .h (函数参数需要完整类类型)
    // - 标准模块引用标准模块 → .c (避免.h之间循环依赖)
    // UDT定义已在上方输出, 循环#include时UDT定义总是先于include可见
    for (const auto& extMod : externalModules) {
        auto* extSym = symTab_.lookup(extMod);
        bool extIsClass = extSym && extSym->kind == SymbolKind::Class;
        if (extIsClass || isClassModule_) {
            h_.emitLine("#include \"" + extMod + ".h\"");
        }
    }
    h_.emitBlank();

    // 生成 .c 源文件头
    c_.emitLine("// Generated by vb6c3 (C3.exe) from " + module.filename);
    c_.emitLine("#include \"" + baseName_ + ".h\"");
    // P13.23: External COM WithEvents need vb6com.h for vb6_CreateEventSink/ComAdvise/ComUnadvise
    if (!knownWithEventsVars_.empty()) {
        for (auto& [varLower, srcClassName] : knownWithEventsVars_) {
            auto* srcClsSym = symTab_.lookup(srcClassName);
            if (srcClsSym && srcClsSym->kind == SymbolKind::ComClass && srcClsSym->comHasSourceIface) {
                c_.emitLine("#include \"vb6com.h\"");
                break;
            }
        }
    }
    // P6.6.3: ActiveX DLL classes with events need vb6comserver.h for vb6_FireEvent
    if (isDll_ && isClassModule_) {
        bool _hasEvts = false;
        for (auto& _d : module.declarations) {
            if (_d->kind == ASTNodeKind::EventDecl) { _hasEvts = true; break; }
        }
        if (_hasEvts) {
            c_.emitLine("#include \"vb6comserver.h\"");
        }
    }
    // P8.7 + Fix 010r-12: 标准模块之间的#include放在.c文件，避免.h循环依赖
    // 类模块引用标准模块 → .h (见上方); 标准模块引用标准模块 → .c
    {
        for (const auto& extMod : externalModules) {
            auto* extSym = symTab_.lookup(extMod);
            bool extIsClass = extSym && extSym->kind == SymbolKind::Class;
            if (!extIsClass && !isClassModule_) {
                c_.emitLine("#include \"" + extMod + ".h\"");
            }
        }
    }
    c_.emitBlank();

    // === 类模块: 生成结构体定义 ===
    if (isClassModule_) {
        std::string clsStruct = "vb6_cls_" + cIdent(moduleName_);  // Fix 013: 用 moduleName_ (VB_Name)
        h_.emitLine("// Class module: " + module.moduleName);

        // P6.5: 前向声明event sink结构体 (在类结构体定义之前)
        bool hasEvents = false;
        for (auto& decl : module.declarations) {
            if (decl->kind == ASTNodeKind::EventDecl) { hasEvents = true; break; }
        }
        if (hasEvents) {
            std::string sinkName = "vb6_events_" + cIdent(moduleName_);  // Fix 013: 用 moduleName_
            h_.emitLine("struct " + sinkName + ";  /* P6.5: forward decl */");
        }

        h_.emitLine("typedef struct " + clsStruct + " {");
    if (isDll_) {
        h_.emitLine("    void* __comObj;  /* P6.6.3: back-ptr to COM wrapper (vb6_ComObject*) */");
    }

        // 收集模块级变量作为结构体字段
        bool hasFields = false;
        for (auto& decl : module.declarations) {
            if (decl->kind == ASTNodeKind::VariableDecl) {
                hasFields = true;
                auto& var = static_cast<VariableDecl&>(*decl);
                std::string cType = mapTypeRef(var.asType.get());
                if (var.isDynamicArray || !var.dimensions.empty()) {
                    // 数组: 使用vb6_SafeArray1D*/vb6_SafeArrayND* (NOT SAFEARRAY*)
                    Vb6Type elemType = resolveArrayElemType(var.asType.get());
                    int dimCount = (int)var.dimensions.size();
                    std::string arrType = (dimCount > 1) ? "vb6_SafeArrayND*" : "vb6_SafeArray1D*";
                    h_.emitLine("    " + arrType + " " + cIdent(var.name) + "; /* " +
                                TypeSystem::typeToString(elemType) + " array */");
                } else if (var.isNew) {
                    // Dim x As New ClassName → 指针字段
                    h_.emitLine("    void* " + cIdent(var.name) + "; /* As New " +
                                (var.asType ? static_cast<SimpleTypeRef*>(var.asType.get())->name : "Object") + " */");
                } else {
                    h_.emitLine("    " + cType + " " + cIdent(var.name) + ";");
                }
            }
        }
        // C不允许空结构体 → 接口类无数据成员时添加占位字段
        if (!hasFields) {
            h_.emitLine("    int _placeholder;  /* interface class: no data members */");
        }
        // P6.5: 如果类有事件声明，添加事件接收器指针字段
        if (hasEvents) {
            std::string sinkName = "vb6_events_" + cIdent(moduleName_);  // Fix 013: 用 moduleName_
            h_.emitLine("    struct " + sinkName + "* events;  /* P6.5: event sink */");
        }
        h_.emitLine("} " + clsStruct + ";");
        h_.emitBlank();

        // 类工厂函数声明
        h_.emitLine(clsStruct + "* " + clsStruct + "_New(void);");
        h_.emitLine("void " + clsStruct + "_Destroy(" + clsStruct + "* me);");
        h_.emitBlank();

        // P6.4: 生成接口vtable结构体 + 包装类型 + 全局vtable实例
        if (!module.implements.empty()) {
            emitInterfaceVtable(module);
        }

        // P6.5: 生成事件接收器表 (事件源类的回调函数指针表)
        if (hasEvents) {
            emitEventSink(module);
        }
    }

    // === 第一遍: 声明 (前向声明 → .h, 定义 → .c) ===
    // (Enum和Type定义已在类结构体前生成, 见上方Fix 010)

    // 3. Const
    for (auto& decl : module.declarations) {
        if (decl->kind == ASTNodeKind::ConstDecl) {
            visit(static_cast<ConstDecl&>(*decl));
        }
    }

    // 4. 类模块变量: 只注册tracking set, 不生成声明(已在结构体中)
    //    标准模块/窗体模块: 生成变量声明并注册tracking set
    trackOnly_ = isClassModule_;
    for (auto& decl : module.declarations) {
        if (decl->kind == ASTNodeKind::VariableDecl) {
            visit(static_cast<VariableDecl&>(*decl));
        }
    }
    trackOnly_ = false;

    // P13.23: External COM WithEvents need vb6com.h for vb6_CreateEventSink/ComAdvise/ComUnadvise
    // (knownWithEventsVars_ is populated during VariableDecl visits above)
    if (!knownWithEventsVars_.empty()) {
        for (auto& [varLower, srcClassName] : knownWithEventsVars_) {
            auto* srcClsSym = symTab_.lookup(srcClassName);
            if (srcClsSym && srcClsSym->kind == SymbolKind::ComClass && srcClsSym->comHasSourceIface) {
                c_.emitLine("#include \"vb6com.h\"");
                break;
            }
        }
    }

    // === P7: 窗体模块额外代码 ===
    // 窗体模块需要: WndProc声明、控件句柄变量、控件创建函数
    if (module.isFormModule && frmDesc) {
        emitFormFramework(*frmDesc, module);
    }

    // 5. Declare (外部函数声明)
    for (auto& decl : module.declarations) {
        if (decl->kind == ASTNodeKind::DeclareDecl) {
            visit(static_cast<DeclareDecl&>(*decl));
        }
    }

    // 6. Event声明 (类模块中)
    if (isClassModule_) {
        for (auto& decl : module.declarations) {
            if (decl->kind == ASTNodeKind::EventDecl) {
                visit(static_cast<EventDecl&>(*decl));
            }
        }
    }

    // 7. 过程前向声明 → .h
    // P6.4: 接口类的方法不生成前向声明 (方法是抽象的, 由实现类提供)
    bool currentClassIsInterface = false;
    if (isClassModule_) {
        auto* clsSym = symTab_.lookupModule(module.moduleName);
        if (clsSym && clsSym->kind == SymbolKind::Class && clsSym->isInterface) {
            currentClassIsInterface = true;
        }
    }
    for (auto& decl : module.declarations) {
        if (currentClassIsInterface) continue;  // 接口类: 跳过方法声明
        if (decl->kind == ASTNodeKind::SubDecl) {
            auto& sub = static_cast<SubDecl&>(*decl);
            std::string sig = makeProcSignature(sub);
            // Fix 055: Form事件处理函数不能为static, 因为wndproc用extern引用它们
            bool isFormEventProc = module.isFormModule && sub.name.find("Form_") == 0;
            if (sub.access == AccessLevel::Public || isFormEventProc) {
                h_.emitLine(sig + ";");
            } else {
                h_.emitLine("static " + sig + ";");
            }
        } else if (decl->kind == ASTNodeKind::FunctionDecl) {
            auto& func = static_cast<FunctionDecl&>(*decl);
            std::string sig = makeProcSignature(func);
            // Fix 055: Form事件处理函数不能为static, 因为wndproc用extern引用它们
            bool isFormEventFunc = module.isFormModule && func.name.find("Form_") == 0;
            if (func.access == AccessLevel::Public || isFormEventFunc) {
                h_.emitLine(sig + ";");
            } else {
                h_.emitLine("static " + sig + ";");
            }
        } else if (decl->kind == ASTNodeKind::PropertyDecl) {
            auto& prop = static_cast<PropertyDecl&>(*decl);
            std::string sig = makePropertySignature(prop);
            if (prop.access == AccessLevel::Public) {
                h_.emitLine(sig + ";");
            } else {
                h_.emitLine("static " + sig + ";");
            }
        }
    }

    // P6.5/P13.23: 先生成事件处理器包装函数的.h声明（在#endif之前）
    // 收集wrapper函数签名，稍后在#endif之前输出到.h
    std::vector<std::string> evtWrapperSigs;
    if (!knownWithEventsVars_.empty()) {
        for (auto& [varLower, srcClassName] : knownWithEventsVars_) {
            auto* srcClsSym = symTab_.lookup(srcClassName);
            if (!srcClsSym) continue;
            if (srcClsSym->kind == SymbolKind::Class) {
                for (auto& evtName : srcClsSym->eventNames) {
                    std::string varName = varLower;
                    auto* varSym = symTab_.lookup(varLower);
                    if (varSym) varName = varSym->name;
                    std::string handlerName = varName + "_" + evtName;
                    auto* handlerSym = symTab_.lookup(handlerName);
                    if (!handlerSym) continue;
                    std::string wrapperName = "vb6_evt_wrap_" + varLower + "_" + cIdent(evtName);
                    // 查找Event声明的参数
                    std::vector<ParameterInfo> evtParams;
                    for (auto& decl2 : module.declarations) {
                        if (decl2->kind == ASTNodeKind::SubDecl) {
                            auto& sub = static_cast<SubDecl&>(*decl2);
                            if (Symbol::toLower(sub.name) == Symbol::toLower(handlerName)) {
                                for (auto& param : sub.params) {
                                    ParameterInfo pi;
                                    pi.name = param->name;
                                    pi.type = (param->asType && param->asType->kind == ASTNodeKind::SimpleTypeRef)
                                        ? typeSys_.resolveTypeName(static_cast<SimpleTypeRef*>(param->asType.get())->name)
                                        : Vb6Type::Variant;
                                    if (pi.type == Vb6Type::Unknown || pi.type == Vb6Type::Empty)
                                        pi.type = Vb6Type::Variant;
                                    pi.isByVal = true;
                                    evtParams.push_back(pi);
                                }
                                break;
                            }
                        }
                    }
                    std::string sig = "void " + wrapperName + "(void* handler";
                    for (auto& p : evtParams) {
                        sig += ", " + mapType(p.type) + " " + cIdent(p.name);
                    }
                    sig += ")";
                    evtWrapperSigs.push_back(sig);
                }
            } else if (srcClsSym->kind == SymbolKind::ComClass && srcClsSym->comHasSourceIface) {
                // P13.23: COM WithEvents 包装函数签名统一为 (VARIANT* args, int argc, VARIANT* result)
                for (auto& evtName : srcClsSym->eventNames) {
                    std::string varName = varLower;
                    auto* varSym = symTab_.lookup(varLower);
                    if (varSym) varName = varSym->name;
                    std::string handlerName = varName + "_" + evtName;
                    auto* handlerSym = symTab_.lookup(handlerName);
                    if (!handlerSym) continue;
                    std::string wrapperName = "vb6_com_evt_" + varLower + "_" + cIdent(evtName);
                    std::string sig = "void " + wrapperName + "(VARIANT* args, int argc, VARIANT* result)";
                    evtWrapperSigs.push_back(sig);
                }
            }
        }
    }
    // 输出wrapper声明到.h（在#endif之前）
    for (auto& sig : evtWrapperSigs) {
        h_.emitLine(sig + ";");
        h_.emitBlank();
    }

    // P13.23: vtable source interface 事件接收器创建函数前向声明
    emitComVtableSinkDecls();

    h_.emitBlank();
    h_.emitLine("#endif /* " + guard + " */");

    // === 第二遍: 过程体 → .c ===
    c_.emitBlank();
    c_.emitLine("// === 过程实现 ===");
    c_.emitBlank();

    for (auto& decl : module.declarations) {
        if (currentClassIsInterface) continue;  // P6.4: 接口类不生成方法实现体 (由实现类提供)
        if (decl->kind == ASTNodeKind::SubDecl) {
            visit(static_cast<SubDecl&>(*decl));
        } else if (decl->kind == ASTNodeKind::FunctionDecl) {
            visit(static_cast<FunctionDecl&>(*decl));
        } else if (decl->kind == ASTNodeKind::PropertyDecl) {
            visit(static_cast<PropertyDecl&>(*decl));
        }
    }

    // === 类模块: 生成工厂函数 ===
    if (isClassModule_ && !currentClassIsInterface) {  // P6.4: 接口类不生成工厂/析构函数
        emitClassFactory(module);
    }

    // === P6.5: 生成事件处理器包装函数 ===
    // 查找 WithEvents 变量，为 obj_EventName 形式的处理器生成C回调包装
    if (!knownWithEventsVars_.empty()) {
        c_.emitBlank();
        c_.emitLine("// === P6.5: Event handler wrappers ===");
        for (auto& [varLower, srcClassName] : knownWithEventsVars_) {
            auto* srcClsSym = symTab_.lookup(srcClassName);
            if (!srcClsSym || srcClsSym->kind != SymbolKind::Class) continue;

            for (auto& evtName : srcClsSym->eventNames) {
                std::string handlerName;
                // 查找原始变量名（保留大小写）
                std::string varName = varLower;
                // 从符号表查找保留大小写的变量名 (用lookup跨模块查找)
                auto* varSym = symTab_.lookup(varLower);
                if (varSym) varName = varSym->name;
                handlerName = varName + "_" + evtName;

                auto* handlerSym = symTab_.lookup(handlerName);
                if (!handlerSym) continue;

                // 生成包装函数: void vb6_evt_wrap_<var>_<evt>(void* handler, params...)
                std::string wrapperName = "vb6_evt_wrap_" + varLower + "_" + cIdent(evtName);
                std::string sinkName = "vb6_events_" + cIdent(srcClassName);

                // 查找Event声明的参数
                std::vector<ParameterInfo> evtParams;
                for (auto& decl2 : module.declarations) {
                    // 事件处理器参数与源类Event声明参数相同
                    // 从处理器Sub的参数获取
                    if (decl2->kind == ASTNodeKind::SubDecl) {
                        auto& sub = static_cast<SubDecl&>(*decl2);
                        if (Symbol::toLower(sub.name) == Symbol::toLower(handlerName)) {
                            for (auto& param : sub.params) {
                                ParameterInfo pi;
                                pi.name = param->name;
                                pi.type = (param->asType && param->asType->kind == ASTNodeKind::SimpleTypeRef)
                                    ? typeSys_.resolveTypeName(static_cast<SimpleTypeRef*>(param->asType.get())->name)
                                    : Vb6Type::Variant;
                                if (pi.type == Vb6Type::Unknown || pi.type == Vb6Type::Empty)
                                    pi.type = Vb6Type::Variant;
                                pi.isByVal = true;
                                evtParams.push_back(pi);
                            }
                            break;
                        }
                    }
                }

                // 构建包装函数签名
                std::string sig = "void " + wrapperName + "(void* handler";
                for (auto& p : evtParams) {
                    sig += ", " + mapType(p.type) + " " + cIdent(p.name);
                }
                sig += ")";

                // .h前向声明已在上方（#endif之前）输出，此处只生成.c实现

                // 生成.c实现
                c_.emitLine(sig + " {");
                c_.indent();
                // 调用实际的VB6处理器函数
                // 如果是类模块，处理器是 vb6_<Mod>_<HandlerName>(me, params...)
                // 如果是标准模块，处理器是 vb6_<HandlerName>(params...)
                // Fix 019: 类模块必须将 handler (void*) 转换为 cls* 再作为 me 传入,
                //          不可使用 classMeParam() (那是参数声明)
                std::string procCall = cProcName(handlerName, handlerSym->access, isClassModule_ ? moduleName_ : "");
                std::string callArgs = "(";
                if (isClassModule_) {
                    callArgs += classHandlerCast();
                }
                for (size_t i = 0; i < evtParams.size(); i++) {
                    if (i > 0 || isClassModule_) {
                        callArgs += ", ";
                    }
                    callArgs += cIdent(evtParams[i].name);
                }
                callArgs += ");";
                c_.emitLine(procCall + callArgs);
                c_.dedent();
                c_.emitLine("}");
                c_.emitBlank();
            }
        }
    }

        // P13.23: External COM WithEvents event callback generation
        for (auto& [varLower, srcClassName] : knownWithEventsVars_) {
            auto* srcClsSym = symTab_.lookup(srcClassName);
            if (!srcClsSym || srcClsSym->kind != SymbolKind::ComClass) continue;

            for (auto& evtName : srcClsSym->eventNames) {
                std::string varName = varLower;
                auto* varSym = symTab_.lookup(varLower);
                if (varSym) varName = varSym->name;
                std::string handlerName = varName + "_" + evtName;

                auto* handlerSym = symTab_.lookup(handlerName);
                if (!handlerSym) continue;

                // Generate: void vb6_com_evt_<var>_<evt>(VARIANT* args, int argc, VARIANT* result)
                std::string wrapperName = "vb6_com_evt_" + varLower + "_" + cIdent(evtName);

                // Get event handler parameters from Sub declaration
                std::vector<ParameterInfo> evtParams;
                for (auto& decl2 : module.declarations) {
                    if (decl2->kind == ASTNodeKind::SubDecl) {
                        auto& sub = static_cast<SubDecl&>(*decl2);
                        if (Symbol::toLower(sub.name) == Symbol::toLower(handlerName)) {
                            for (auto& param : sub.params) {
                                ParameterInfo pi;
                                pi.name = param->name;
                                pi.isByVal = true;
                                if (param->asType && param->asType->kind == ASTNodeKind::ArrayTypeRef) {
                                    auto& arr = static_cast<ArrayTypeRef&>(*param->asType);
                                    Vb6Type elemType = Vb6Type::Variant;
                                    if (arr.elementType && arr.elementType->kind == ASTNodeKind::SimpleTypeRef) {
                                        elemType = typeSys_.resolveTypeName(static_cast<SimpleTypeRef*>(arr.elementType.get())->name);
                                    }
                                    if (elemType == Vb6Type::Unknown || elemType == Vb6Type::Empty)
                                        elemType = Vb6Type::Variant;
                                    pi.type = static_cast<Vb6Type>(static_cast<uint16_t>(elemType) | static_cast<uint16_t>(Vb6Type::Array));
                                } else if (param->asType && param->asType->kind == ASTNodeKind::SimpleTypeRef) {
                                    pi.type = typeSys_.resolveTypeName(static_cast<SimpleTypeRef*>(param->asType.get())->name);
                                    if (pi.type == Vb6Type::Unknown || pi.type == Vb6Type::Empty)
                                        pi.type = Vb6Type::Variant;
                                } else {
                                    pi.type = Vb6Type::Variant;
                                }
                                evtParams.push_back(pi);
                            }
                            break;
                        }
                    }
                }

                c_.emitLine("void " + wrapperName + "(VARIANT* args, int argc, VARIANT* result) {");
                c_.indent();

                // Extract parameters from VARIANT args
                for (size_t pi = 0; pi < evtParams.size(); pi++) {
                    std::string pName = cIdent(evtParams[pi].name);
                    std::string pType = mapType(evtParams[pi].type);
                    c_.emitLine(pType + " " + pName + ";");
                    bool isArray = (static_cast<uint16_t>(evtParams[pi].type) & static_cast<uint16_t>(Vb6Type::Array)) != 0;
                    Vb6Type baseType = isArray
                        ? static_cast<Vb6Type>(static_cast<uint16_t>(evtParams[pi].type) & ~static_cast<uint16_t>(Vb6Type::Array))
                        : evtParams[pi].type;
                    if (isArray) {
                        // P13.23: COM event arrays come as VARIANT containing SAFEARRAY (VT_ARRAY | VT_xxx).
                        // Extract raw pointer to the first element for 1D Byte arrays.
                        std::string idx = std::to_string(pi);
                        if (baseType == Vb6Type::Byte) {
                            c_.emitLine(pName + " = (uint8_t*)((args[" + idx + "].vt & VT_ARRAY) ? args[" + idx + "].parray->pvData : NULL);");
                        } else {
                            c_.emitLine(pName + " = (" + pType + ")((args[" + idx + "].vt & VT_ARRAY) ? args[" + idx + "].parray->pvData : NULL);");
                        }
                    } else if (evtParams[pi].type == Vb6Type::String) {
                        c_.emitLine(pName + " = (args[" + std::to_string(pi) + "].vt == VT_BSTR) ? args[" + std::to_string(pi) + "].bstrVal : NULL;");
                    } else if (evtParams[pi].type == Vb6Type::Long || evtParams[pi].type == Vb6Type::Integer) {
                        c_.emitLine(pName + " = (" + pType + ")V_I4(&args[" + std::to_string(pi) + "]);");
                    } else if (evtParams[pi].type == Vb6Type::Boolean) {
                        c_.emitLine(pName + " = (args[" + std::to_string(pi) + "].vt == VT_BOOL) ? (V_I4(&args[" + std::to_string(pi) + "]) != 0) : 0;");
                    } else if (evtParams[pi].type == Vb6Type::Single || evtParams[pi].type == Vb6Type::Double) {
                        c_.emitLine(pName + " = (" + pType + ")V_R8(&args[" + std::to_string(pi) + "]);");
                    } else {
                        c_.emitLine(pName + " = args[" + std::to_string(pi) + "];");
                    }
                }

                // Call the VB6 handler function
                // Fix 019: 类模块必须将 handler (void*) 转换为 cls* 再作为 me 传入,
                //          不可使用 classMeParam() (那是参数声明)
                std::string procCall = cProcName(handlerName, handlerSym->access, isClassModule_ ? moduleName_ : "");
                std::string callArgs = "(";
                if (isClassModule_) {
                    callArgs += classHandlerCast();
                }
                for (size_t i = 0; i < evtParams.size(); i++) {
                    if (i > 0 || isClassModule_) {
                        callArgs += ", ";
                    }
                    callArgs += cIdent(evtParams[i].name);
                }
                callArgs += ");";
                c_.emitLine(procCall + callArgs);

                c_.dedent();
                c_.emitLine("}");
                c_.emitBlank();
            }
        }

    // P13.23: 生成外部COM vtable source interface 事件接收器实现
    emitComVtableSinks();

    // Fix 054/080: 模块级变量延迟初始化函数 (C2099 workaround)
    // C语言文件作用域变量不能用运行时函数调用初始化, 所以先声明为 NULL/0,
    // 再在模块初始化函数中执行实际的 SafeArrayCreate 等运行时初始化.
    // Fix 080: 多模块工程中, 每个模块都生成初始化函数(即使为空),
    // 非入口模块的初始化函数需要被入口模块调用,
    // 所以去掉 static 改为 extern 可见, 并在 .h 中添加声明.
    std::string modInitFuncName = "vb6_mod_" + cIdent(baseName_) + "_init";
    bool needModInitFunc = isMultiModule_ || !moduleInitStmts_.empty();
    if (needModInitFunc) {
        c_.emitBlank();
        c_.emitLine("// Fix 054/080: Module-level variable deferred initialization");
        if (isMultiModule_) {
            c_.emitLine("void " + modInitFuncName + "(void) {");
            h_.emitLine("extern void " + modInitFuncName + "(void);");
        } else {
            c_.emitLine("static void " + modInitFuncName + "(void) {");
        }
        c_.indent();
        for (auto& stmt : moduleInitStmts_) {
            c_.emitLine(stmt);
        }
        c_.dedent();
        c_.emitLine("}");
        c_.emitBlank();
    }
    if (!needModInitFunc) modInitFuncName.clear();

    // 生成入口点 (类模块不生成main; 多模块工程中仅有Sub Main的模块生成main)
    // P6.6: ActiveX DLL入口点统一由dll_entry.c生成, 不在各模块.c中生成
    if (!isClassModule_) {
        if (isDll_) {
            // P6.6: DLL模式下, 入口点由driver额外生成的dll_entry.c提供
            // 这里只生成vb6_Init/vb6_Exit的调用桩 (供dll_entry.c中的DllMain使用)
            // 不再调用emitActiveXDll()以避免符号重复定义
        } else {
            c_.emitBlank();
            c_.emitLine("// === 入口点 ===");
            bool hasMain = false;
        for (auto& decl : module.declarations) {
            if (decl->kind == ASTNodeKind::SubDecl) {
                auto& sub = static_cast<SubDecl&>(*decl);
                if (sub.name == "Main" && sub.access == AccessLevel::Public) {
                    hasMain = true;
                    break;
                }
            }
        }
        // 单模块: 允许用第一个Public Sub作为入口
        // 多模块: 只有Sub Main模块才生成main
        bool shouldGenMain = hasMain;
        if (!shouldGenMain && !isMultiModule_) {
            // 单模块模式: 无Sub Main时自动找首个Public Sub
    for (auto& decl : module.declarations) {
        if (currentClassIsInterface) continue;  // P6.4: 接口类不生成方法实现体
        if (decl->kind == ASTNodeKind::SubDecl) {
                    auto& sub = static_cast<SubDecl&>(*decl);
                    if (sub.access == AccessLevel::Public) {
                        shouldGenMain = true;
                        // P7: 窗体模块生成WinMain, 标准模块生成main
                        if (module.isFormModule && frmDesc) {
                            // 窗体模块: WinMain → 显示窗体 → 消息循环
                            std::string formName = frmDesc->formName;
                            c_.emitLine("int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nCmdShow) {");
                            c_.indent();
                            c_.emitLine("(void)hPrevInst; (void)lpCmdLine; (void)nCmdShow;");
            c_.emitLine("vb6_Init();");
            if (!modInitFuncName.empty()) c_.emitLine(modInitFuncName + "();");
            // Fix 080: 多模块工程中, 调用所有外部模块的初始化函数
            if (isMultiModule_) {
                for (const auto& extMod : externalModules_) {
                    std::string extInitFunc = "vb6_mod_" + cIdent(extMod) + "_init";
                    c_.emitLine(extInitFunc + "();");
                }
            }
            c_.emitLine("vb6_SetAppInstance((void*)hInst);");
            c_.emitLine("vb6_form_show_" + cIdent(formName) + "(NULL);  /* Show form modeless, NULL=hMDIClient */");
                            c_.emitLine("int ret = vb6_MessageLoop();");
                            c_.emitLine("vb6_Exit();");
                            c_.emitLine("return ret;");
                            c_.dedent();
                            c_.emitLine("}");
                        } else {
                            c_.emitLine("int main(int argc, char* argv[]) {");
                            c_.indent();
            c_.emitLine("vb6_Init();");
            if (!modInitFuncName.empty()) c_.emitLine(modInitFuncName + "();");
            // Fix 080: 多模块工程中, 调用所有外部模块的初始化函数
            if (isMultiModule_) {
                for (const auto& extMod : externalModules_) {
                    std::string extInitFunc = "vb6_mod_" + cIdent(extMod) + "_init";
                    c_.emitLine(extInitFunc + "();");
                }
            }
            c_.emitLine(cProcName(sub.name, sub.access) + "();");
            c_.emitLine("vb6_Exit();");
                            c_.emitLine("return 0;");
                            c_.dedent();
                            c_.emitLine("}");
                        }
                        break;
                    }
                }
            }
        }
        if (shouldGenMain && hasMain) {
            // P7: 窗体模块生成WinMain, 标准模块生成main
            if (module.isFormModule) {
                c_.emitLine("int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nCmdShow) {");
                c_.indent();
                c_.emitLine("(void)hPrevInst; (void)lpCmdLine; (void)nCmdShow;");
                c_.emitLine("vb6_Init();");
                // Fix 080: 多模块工程中, 调用所有外部模块的初始化函数
                if (isMultiModule_) {
                    for (const auto& extMod : externalModules_) {
                        std::string extInitFunc = "vb6_mod_" + cIdent(extMod) + "_init";
                        c_.emitLine(extInitFunc + "();");
                    }
                }
                if (!modInitFuncName.empty()) c_.emitLine(modInitFuncName + "();");
                c_.emitLine("vb6_SetAppInstance((void*)hInst);");
                c_.emitLine(cProcName("Main", AccessLevel::Public) + "();");
                c_.emitLine("int ret = vb6_MessageLoop();");
                c_.emitLine("vb6_Exit();");
                c_.emitLine("return ret;");
                c_.dedent();
                c_.emitLine("}");
            } else {
                c_.emitLine("int main(int argc, char* argv[]) {");
                c_.indent();
            c_.emitLine("vb6_Init();");
            if (!modInitFuncName.empty()) c_.emitLine(modInitFuncName + "();");
            // Fix 080: 多模块工程中, 调用所有外部模块的初始化函数
            if (isMultiModule_) {
                for (const auto& extMod : externalModules_) {
                    std::string extInitFunc = "vb6_mod_" + cIdent(extMod) + "_init";
                    c_.emitLine(extInitFunc + "();");
                }
            }
            c_.emitLine(cProcName("Main", AccessLevel::Public) + "();");
                c_.emitLine("vb6_Exit();");
                c_.emitLine("return 0;");
                c_.dedent();
                c_.emitLine("}");
            }
        } else if (!shouldGenMain) {
            if (module.isFormModule && frmDesc) {
                // P7: 空窗体也要生成WinMain, 自动显示窗体
                std::string formName = frmDesc->formName;
                c_.emitLine("int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nCmdShow) {");
                c_.indent();
                c_.emitLine("(void)hPrevInst; (void)lpCmdLine; (void)nCmdShow;");
                c_.emitLine("vb6_Init();");
                if (!modInitFuncName.empty()) c_.emitLine(modInitFuncName + "();");
                // Fix 080: 多模块工程中, 调用所有外部模块的初始化函数
                if (isMultiModule_) {
                    for (const auto& extMod : externalModules_) {
                        std::string extInitFunc = "vb6_mod_" + cIdent(extMod) + "_init";
                        c_.emitLine(extInitFunc + "();");
                    }
                }
                c_.emitLine("vb6_SetAppInstance((void*)hInst);");
                c_.emitLine("vb6_form_show_" + cIdent(formName) + "(NULL);  /* Show form modeless, NULL=hMDIClient */");
                c_.emitLine("int ret = vb6_MessageLoop();");
                c_.emitLine("vb6_Exit();");
                c_.emitLine("return ret;");
                c_.dedent();
                c_.emitLine("}");
            } else {
                c_.emitLine("// No entry point (library module)");
            }
        }
        }  // end else (EXE mode)
    }

    // 保存生成结果
    header_ = h_.str();
    source_ = c_.str();

    // P6.3: 如果使用了COM接口类型, 在头文件中插入typedef前向声明
    // vb6_ComIface_<Name> 是不透明结构体, 仅用作类型化指针
    if (!usedComIfaceTypes_.empty() || !usedVb6IfaceTypes_.empty() || !usedClassTypes_.empty() || !usedUdtTypes_.empty()) {
        std::string typedefBlock;
        if (!usedComIfaceTypes_.empty()) {
            typedefBlock += "\n// P6.3: COM接口类型前向声明 (早期绑定)\n";
            for (const auto& ifaceName : usedComIfaceTypes_) {
                typedefBlock += "typedef struct vb6_ComIface_" + ifaceName + " vb6_ComIface_" + ifaceName + ";\n";
            }
        }
        if (!usedVb6IfaceTypes_.empty()) {
            typedefBlock += "\n// P6.4: VB6接口类型前向声明 (Implements)\n";
            for (const auto& ifaceName : usedVb6IfaceTypes_) {
                typedefBlock += "typedef struct vb6_vtbl_" + ifaceName + " vb6_vtbl_" + ifaceName + ";\n";
                typedefBlock += "typedef struct vb6_iface_" + ifaceName + " vb6_iface_" + ifaceName + ";\n";
            }
        }
        // Fix 010: VB6类类型前向声明
        // C11允许typedef重定义, 所以无条件发出所有使用到的类类型前向声明
        // 解决: (1) 当前模块类struct中使用UDT类型 (2) 循环#include导致的类型不可见
        if (!usedClassTypes_.empty()) {
            typedefBlock += "\n// VB6类类型前向声明\n";
            for (const auto& clsName : usedClassTypes_) {
                typedefBlock += "typedef struct vb6_cls_" + clsName + " vb6_cls_" + clsName + ";\n";
            }
        }
        // Fix 010: VB6 UDT类型前向声明
        // C11允许typedef重定义, 无条件发出 — 解决类struct中UDT类型先使用后定义的问题
        if (!usedUdtTypes_.empty()) {
            typedefBlock += "\n// VB6 UDT类型前向声明\n";
            for (const auto& udtName : usedUdtTypes_) {
                typedefBlock += "typedef struct vb6_type_" + udtName + " vb6_type_" + udtName + ";\n";
            }
        }
        // 在 #include "vb6rtl.h" 之后插入
        std::string marker = "#include \"vb6rtl.h\"";
        size_t pos = header_.find(marker);
        if (pos != std::string::npos) {
            pos = header_.find('\n', pos);  // 找到行尾
            if (pos != std::string::npos) {
                header_.insert(pos + 1, typedefBlock);
            }
        }
    }

    currentModule_ = nullptr;
    return diag_.errorCount() == 0;
}

// ============================================================
// 类型映射
// ============================================================

std::string CCodeGen::mapType(Vb6Type type) const {
    // 检查数组标志
    bool isArray = (static_cast<uint16_t>(type) & static_cast<uint16_t>(Vb6Type::Array)) != 0;
    Vb6Type baseType = isArray
        ? static_cast<Vb6Type>(static_cast<uint16_t>(type) & ~static_cast<uint16_t>(Vb6Type::Array))
        : type;

    std::string cType;
    switch (baseType) {
        case Vb6Type::Empty:    cType = "int16_t"; break;   // VB6 Empty = 0
        case Vb6Type::Null:     cType = "int16_t"; break;   // VB6 Null = 1
        case Vb6Type::Integer:  cType = "int16_t"; break;
        case Vb6Type::Long:     cType = "int32_t"; break;
        case Vb6Type::LongPtr: cType = "intptr_t"; break;   // Fix 081e: architecture-width integer
        case Vb6Type::Single:   cType = "float"; break;
        case Vb6Type::Double:   cType = "double"; break;
        case Vb6Type::Currency: cType = "int64_t"; break;   // scaled integer
        case Vb6Type::Date:     cType = "double"; break;    // OLE date
        case Vb6Type::String:   cType = "BSTR"; break;      // wchar_t* wrapper
        case Vb6Type::Object:   cType = "void*"; break;     // IDispatch* → void* for now
        case Vb6Type::Error:    cType = "int32_t"; break;   // SCODE/HRESULT
        case Vb6Type::Boolean:  cType = "int16_t"; break;   // VB6: True=-1, False=0
        case Vb6Type::Variant:  cType = "vb6_VARIANT"; break;   // tagged union
        case Vb6Type::Byte:     cType = "uint8_t"; break;
        case Vb6Type::ULong:    cType = "uint32_t"; break;
        case Vb6Type::Void:     cType = "void"; break;
        case Vb6Type::Decimal:  cType = "vb6_VARIANT"; break;   // 用VARIANT兜底
        case Vb6Type::UserDefinedType: cType = "vb6_VARIANT"; break; // 占位, 后续改进
        default:                cType = "vb6_VARIANT"; break;    // 未知/安全兜底
    }

    if (isArray) {
        if (baseType == Vb6Type::Byte) {
            cType = "uint8_t*";  // Byte数组作为原始字节指针
        } else {
            cType = "vb6_SafeArray1D*";  // 数组用SAFEARRAY
        }
    }
    return cType;
}

std::string CCodeGen::mapComType(Vb6Type type) const {
    // COM vtable 方法签名映射 (用于外部COM事件接收器)
    bool isArray = (static_cast<uint16_t>(type) & static_cast<uint16_t>(Vb6Type::Array)) != 0;
    Vb6Type baseType = isArray
        ? static_cast<Vb6Type>(static_cast<uint16_t>(type) & ~static_cast<uint16_t>(Vb6Type::Array))
        : type;
    if (isArray) {
        return "SAFEARRAY*";  // 数组在COM vtable中总是SAFEARRAY指针
    }
    switch (baseType) {
        case Vb6Type::Integer:  return "int16_t";
        case Vb6Type::Long:     return "int32_t";
        case Vb6Type::LongPtr: return "intptr_t";  // Fix 081e        case Vb6Type::Single:   return "float";
        case Vb6Type::Double:   return "double";
        case Vb6Type::Currency: return "int64_t";
        case Vb6Type::Date:     return "double";
        case Vb6Type::String:   return "BSTR";
        case Vb6Type::Object:   return "IUnknown*";
        case Vb6Type::Error:    return "int32_t";
        case Vb6Type::Boolean:  return "int16_t";
        case Vb6Type::Variant:  return "VARIANT";
        case Vb6Type::Byte:     return "uint8_t";
        case Vb6Type::ULong:    return "uint32_t";
        case Vb6Type::Void:     return "void";
        default:                return "void*";
    }
}

std::string CCodeGen::emitGuidInitializer(const std::string& iidStr) const {
    // 解析 {XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX} 格式
    if (iidStr.size() < 38 || iidStr.front() != '{' || iidStr.back() != '}') return "";
    std::string s = iidStr.substr(1, iidStr.size() - 2);  // 去掉花括号
    // 分割为5个部分
    std::vector<std::string> parts;
    size_t start = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '-') {
            parts.push_back(s.substr(start, i - start));
            start = i + 1;
        }
    }
    parts.push_back(s.substr(start));
    if (parts.size() != 5) return "";
    std::string d1 = parts[0];
    std::string d2 = parts[1];
    std::string d3 = parts[2];
    std::string d4 = parts[3] + parts[4];  // 16个hex字符
    if (d1.size() != 8 || d2.size() != 4 || d3.size() != 4 || d4.size() != 16) return "";
    char buf[128];
    snprintf(buf, sizeof(buf), "{0x%s, 0x%s, 0x%s, {0x%s, 0x%s, 0x%s, 0x%s, 0x%s, 0x%s, 0x%s, 0x%s}}",
             d1.c_str(), d2.c_str(), d3.c_str(),
             d4.substr(0,2).c_str(), d4.substr(2,2).c_str(), d4.substr(4,2).c_str(), d4.substr(6,2).c_str(),
             d4.substr(8,2).c_str(), d4.substr(10,2).c_str(), d4.substr(12,2).c_str(), d4.substr(14,2).c_str());
    return std::string(buf);
}

std::string CCodeGen::mapTypeRef(ASTNode* typeRef) {
    if (!typeRef) return "vb6_VARIANT";  // 未指定类型 = Variant

    switch (typeRef->kind) {
        case ASTNodeKind::SimpleTypeRef: {
            auto& simple = static_cast<SimpleTypeRef&>(*typeRef);
            // Fix 010: VB6 As Any (Declare语句) → C void*
            // Any在VB6中仅用于Declare语句的参数, 表示"任意类型指针"
            if (simple.name == "Any" || simple.name == "any") {
                return "void*";
            }
            // Fix 010: VBA.库前缀 (如 VBA.ErrObject, VBA.Collection) — 去除前缀
            std::string typeName = simple.name;
            if (typeName.size() > 4 && typeName.compare(0, 4, "VBA.") == 0) {
                typeName = typeName.substr(4);
            }
            // Fix 010: VB6内置对象类型 (Collection, ErrObject等) → void* (对象指针)
            static const std::unordered_set<std::string> vb6BuiltinObjTypes = {
                "Collection", "Forms", "ErrObject", "App", "Screen", "Printer", "Clipboard"
            };
            if (vb6BuiltinObjTypes.count(typeName)) {
                return "void*";
            }
            // 尝试从类型系统解析
            Vb6Type t = typeSys_.resolveTypeName(typeName);
            if (t != Vb6Type::Unknown) {
                return mapType(t);
            }
            // 限定类型名 (如 Scripting.Dictionary): 用最后一部分查找符号
            std::string lookupName = typeName;
            size_t dotPos = typeName.find('.');
            if (dotPos != std::string::npos) {
                std::string shortName = typeName.substr(dotPos + 1);
                auto* dotSym = symTab_.lookupModule(shortName);
                if (dotSym) lookupName = shortName;
            }
            // 检查是否是类名 → 映射为类结构体指针
            auto* clsSym = symTab_.lookupModule(lookupName);
            if (clsSym && clsSym->kind == SymbolKind::Class) {
                // P6.4: 接口类 → vb6_iface_<Name> 包装类型 (非指针)
                if (clsSym->isInterface) {
                    usedVb6IfaceTypes_.insert(cIdent(clsSym->name));  // 收集用于前向声明
                    return "vb6_iface_" + cIdent(clsSym->name);
                }
                // Fix 010: 收集类类型名用于前向声明
                usedClassTypes_.insert(cIdent(clsSym->name));
                return "vb6_cls_" + cIdent(clsSym->name) + "*";
            }
            // P6.3: 检查是否是COM coclass/接口 → 映射为接口指针类型 (前期绑定)
            if (clsSym && (clsSym->kind == SymbolKind::ComClass || clsSym->kind == SymbolKind::ComInterface)) {
                // 生成类型化接口指针: vb6_ComIface_<InterfaceName>*
                // 运行时通过vb6_ComQI获取, vtable直接调用
                std::string ifaceName = clsSym->name;
                if (clsSym->kind == SymbolKind::ComClass && !clsSym->comDefaultIfaceName.empty()) {
                    ifaceName = clsSym->comDefaultIfaceName;
                }
                std::string cIfaceName = cIdent(ifaceName);
                usedComIfaceTypes_.insert(cIfaceName);  // 收集接口类型名用于typedef
                return "vb6_ComIface_" + cIfaceName + "*";
            }
            // 检查是否是用户定义类型 (UDT) → vb6_type_<Name>
            auto* udtSym = symTab_.lookup(lookupName);
            if (udtSym && udtSym->kind == SymbolKind::UserDefinedType) {
                // Fix 010: 收集UDT类型名用于前向声明
                usedUdtTypes_.insert(cIdent(simple.name));
                return "vb6_type_" + cIdent(simple.name);
            }
            // 检查是否是枚举类型 → 基础类型int32_t (VB6枚举底层是Long)
            if (udtSym && udtSym->kind == SymbolKind::EnumType) {
                return "int32_t";
            }
            // Fix 010b: VB6标准库与外部COM库的类型映射
            // 以下类型不在项目符号表中, 但均为VB6/COM标准类型
            // 注意: 能在符号表中找到的用户类型会已在上面被处理

            // VB6语言类型别名
            // Fix 081e: LongPtr now has its own Vb6Type::LongPtr → intptr_t
            // (handled by resolveTypeName + mapType, this fallback is for edge cases)
            if (lookupName == "LongPtr" || lookupName == "LongLong") {
                return "intptr_t";
            }
            // VB6内置枚举类型 (Vb前缀): VbCompareMethod, VbTriState, VbFileAttribute等
            // VB6枚举底层是Long (int32_t)
            if (lookupName.size() >= 2 && lookupName.compare(0, 2, "Vb") == 0) {
                return "int32_t";
            }
            // COM类型别名 (OLE_前缀): OLE_COLOR, OLE_HANDLE等, 通常为DWORD
            if (lookupName.size() >= 4 && lookupName.compare(0, 4, "OLE_") == 0) {
                return "int32_t";
            }
            // ADODB等外部COM库枚举类型: 名称以Enum结尾
            // 如 EventStatusEnum, ExecuteOptionEnum, CursorTypeEnum等
            if (lookupName.size() >= 4 &&
                lookupName.compare(lookupName.size() - 4, 4, "Enum") == 0) {
                return "int32_t";
            }
            // ADODB等外部COM库对象类型 → void* (COM对象指针)
            static const std::unordered_set<std::string> comObjTypes = {
                "Connection", "Recordset", "Command", "Parameter",
                "Field", "Fields", "Error", "Errors", "Property",
                "Properties", "Stream"
            };
            if (comObjTypes.count(lookupName)) {
                return "void*";
            }
            // Fix 010c: VB6标准枚举类型别名 (不带Vb前缀的常用枚举)
            // 这些类型在VB6中等价于对应的Vb*枚举, 底层都是Long
            static const std::unordered_set<std::string> vb6EnumAliases = {
                "CompareMethod", "TriState", "FirstDayOfWeek", "FirstWeekOfYear",
                "MsgBoxResult", "MsgBoxStyle", "FileAttribute", "DateFormat",
                "Calendar", "DateTimeFormat", "CallType", "VariantType",
                "VarType", "QueryDef", "EditModeEnum", "FieldAttributeEnum"
            };
            if (vb6EnumAliases.count(lookupName)) {
                return "int32_t";
            }
            // 兜底: 未知类型 (如窗体模块名、外部COM类型别名等) → void*
            // Fix 010c: 窗体模块(.frm)未注册为Class符号, 但VB6中可作为类型使用
            // 任何不是内置类型/UDT/枚举/类/COM类型的名称都视为通用对象指针
            return "void*";
        }
        case ASTNodeKind::ArrayTypeRef:
            return "vb6_SafeArray1D*";  // SAFEARRAY指针
        case ASTNodeKind::FixedStringTypeRef:
            return "BSTR";
        default:
            return "vb6_VARIANT";
    }
}

// Fix 081e: Declare函数返回类型映射
// 在VB6 Declare语句中, 返回值Long常用于返回句柄/指针(HDC/HBITMAP/HWND等)。
// x64下int32_t只有4字节,无法容纳8字节指针,导致截断和后续崩溃。
// LongPtr已经通过mapType映射为intptr_t, 这里只需将Long返回值也映射为intptr_t。
// 映射为intptr_t: x86下4字节(兼容), x64下8字节(与指针同大小)。
std::string CCodeGen::mapDeclareType(ASTNode* typeRef) {
    if (!typeRef) return "vb6_VARIANT";
    std::string base = mapTypeRef(typeRef);
    if (base == "int32_t") {
        // 检查是否是Long类型 (LongPtr已通过mapType返回intptr_t)
        if (typeRef->kind == ASTNodeKind::SimpleTypeRef) {
            auto& simple = static_cast<SimpleTypeRef&>(*typeRef);
            if (simple.name == "Long") {
                return "intptr_t";
            }
        }
    }
    return base;
}

// Fix 010b: 常量折叠 — 将VB6 AST表达式求值为int64_t编译期常量
// 用于enum成员值 (如 2^0 → 1, 2^1|2^2 → 6, &H10 → 16)
bool CCodeGen::tryEvalConstInt(ASTNode* expr, int64_t& result) {
    if (!expr) return false;

    switch (expr->kind) {
    case ASTNodeKind::LiteralExpr: {
        auto* lit = static_cast<LiteralExpr*>(expr);
        switch (lit->literalKind) {
        case LiteralKind::Integer:
            result = lit->intValue;
            return true;
        case LiteralKind::Long:
            result = lit->longValue;
            return true;
        case LiteralKind::Boolean:
            result = lit->boolValue ? 1 : 0;
            return true;
        case LiteralKind::Double:
            result = (int64_t)lit->doubleValue;
            return true;
        case LiteralKind::Single:
            result = (int64_t)lit->floatValue;
            return true;
        default:
            return false;
        }
    }
    case ASTNodeKind::UnaryExpr: {
        auto* unary = static_cast<UnaryExpr*>(expr);
        int64_t val;
        if (!tryEvalConstInt(unary->operand.get(), val)) return false;
        switch (unary->op) {
        case UnaryOp::Negate:
            result = -val;
            return true;
        case UnaryOp::Not:
            result = ~val;
            return true;
        }
        return false;
    }
    case ASTNodeKind::BinaryExpr: {
        auto* bin = static_cast<BinaryExpr*>(expr);
        int64_t l, r;
        if (!tryEvalConstInt(bin->left.get(), l)) return false;
        if (!tryEvalConstInt(bin->right.get(), r)) return false;
        switch (bin->op) {
        case BinaryOp::Add: result = l + r; return true;
        case BinaryOp::Sub: result = l - r; return true;
        case BinaryOp::Mul: result = l * r; return true;
        case BinaryOp::Div:
            if (r == 0) return false;
            result = l / r; return true;
        case BinaryOp::IntDiv:
            if (r == 0) return false;
            result = l / r; return true;
        case BinaryOp::Mod:
            if (r == 0) return false;
            result = l % r; return true;
        case BinaryOp::Pow: {
            // 整数幂运算
            if (r < 0) return false;
            int64_t base = l, exp = r;
            int64_t pw = 1;
            while (exp > 0) {
                if (exp & 1) pw *= base;
                base *= base;
                exp >>= 1;
            }
            result = pw;
            return true;
        }
        case BinaryOp::Or:  result = l | r; return true;
        case BinaryOp::And: result = l & r; return true;
        case BinaryOp::Xor: result = l ^ r; return true;
        case BinaryOp::Eqv: result = ~(l ^ r); return true;
        case BinaryOp::Imp: result = (~l) | r; return true;
        default:
            return false;  // 比较、连接等不适用于enum常量
        }
    }
    case ASTNodeKind::IndexOrCallExpr: {
        // 处理 vb6_Pow(base, exp) 调用
        auto* call = static_cast<IndexOrCallExpr*>(expr);
        if (!call->callee) return false;
        // 提取被调用者名称
        std::string fnName;
        if (auto* id = dynamic_cast<IdentifierExpr*>(call->callee.get())) {
            fnName = id->name;
        } else {
            return false;
        }
        // 转小写比较
        std::string fnLower = fnName;
        for (auto& c : fnLower) c = (char)tolower(c);
        if (fnLower == "pow" && call->positional.size() == 2) {
            int64_t base, exp;
            if (!tryEvalConstInt(call->positional[0].get(), base)) return false;
            if (!tryEvalConstInt(call->positional[1].get(), exp)) return false;
            if (exp < 0) return false;
            int64_t pw = 1;
            while (exp > 0) {
                if (exp & 1) pw *= base;
                base *= base;
                exp >>= 1;
            }
            result = pw;
            return true;
        }
        return false;
    }
    case ASTNodeKind::IdentifierExpr: {
        // Fix 010c: 查找符号表中的常量 (跨模块Public Const)
        auto* id = static_cast<IdentifierExpr*>(expr);
        auto* sym = symTab_.lookup(id->name);
        if (sym && sym->kind == SymbolKind::Constant && sym->hasConstValue) {
            result = sym->constIntValue;
            return true;
        }
        // 也检查枚举成员
        if (sym && sym->kind == SymbolKind::EnumMember) {
            result = sym->constIntValue;
            return true;
        }
        return false;
    }
    default:
        return false;
    }
}

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
            return "0LL";
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

// ============================================================
// 表达式求值 (累加器模式: lastExpr_接收结果)
// ============================================================


} // namespace vb6c3
