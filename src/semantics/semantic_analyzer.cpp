#include "semantics/semantic_analyzer.hpp"
#include "semantics/semantic_analyzer_internal.h"
#include <algorithm>
#include <cctype>
#include <tuple>
#include <initializer_list>

namespace vb6c3 {

// ============================================================
// 手动分派辅助 (AST节点无accept, 按kind分派到visitor)
// ============================================================

// 声明节点分派
void dispatchDecl(Decl& decl, SemanticAnalyzer& analyzer) {
    switch (decl.kind) {
        case ASTNodeKind::SubDecl:       analyzer.visit(static_cast<SubDecl&>(decl)); break;
        case ASTNodeKind::FunctionDecl:  analyzer.visit(static_cast<FunctionDecl&>(decl)); break;
        case ASTNodeKind::PropertyDecl:  analyzer.visit(static_cast<PropertyDecl&>(decl)); break;
        case ASTNodeKind::TypeDecl:      analyzer.visit(static_cast<TypeDecl&>(decl)); break;
        case ASTNodeKind::TypeMember:    analyzer.visit(static_cast<TypeMember&>(decl)); break;
        case ASTNodeKind::EnumDecl:      analyzer.visit(static_cast<EnumDecl&>(decl)); break;
        case ASTNodeKind::EnumMember:    analyzer.visit(static_cast<EnumMember&>(decl)); break;
        case ASTNodeKind::DeclareDecl:   analyzer.visit(static_cast<DeclareDecl&>(decl)); break;
        case ASTNodeKind::EventDecl:     analyzer.visit(static_cast<EventDecl&>(decl)); break;
        case ASTNodeKind::ConstDecl:     analyzer.visit(static_cast<ConstDecl&>(decl)); break;
        case ASTNodeKind::VariableDecl:  analyzer.visit(static_cast<VariableDecl&>(decl)); break;
        case ASTNodeKind::ParameterDecl: analyzer.visit(static_cast<ParameterDecl&>(decl)); break;
        default: break;
    }
}

// 语句节点分派
void dispatchStmt(Stmt& stmt, SemanticAnalyzer& analyzer) {
    switch (stmt.kind) {
        case ASTNodeKind::Block:            analyzer.visit(static_cast<Block&>(stmt)); break;
        case ASTNodeKind::AssignmentStmt:   analyzer.visit(static_cast<AssignmentStmt&>(stmt)); break;
        case ASTNodeKind::SetStmt:          analyzer.visit(static_cast<SetStmt&>(stmt)); break;
        case ASTNodeKind::LetStmt:          analyzer.visit(static_cast<LetStmt&>(stmt)); break;
        case ASTNodeKind::IfStmt:           analyzer.visit(static_cast<IfStmt&>(stmt)); break;
        case ASTNodeKind::ElseIfClause:     analyzer.visit(static_cast<ElseIfClause&>(stmt)); break;
        case ASTNodeKind::ForStmt:          analyzer.visit(static_cast<ForStmt&>(stmt)); break;
        case ASTNodeKind::ForEachStmt:      analyzer.visit(static_cast<ForEachStmt&>(stmt)); break;
        case ASTNodeKind::DoLoopStmt:       analyzer.visit(static_cast<DoLoopStmt&>(stmt)); break;
        case ASTNodeKind::WhileWendStmt:    analyzer.visit(static_cast<WhileWendStmt&>(stmt)); break;
        case ASTNodeKind::SelectCaseStmt:   analyzer.visit(static_cast<SelectCaseStmt&>(stmt)); break;
        case ASTNodeKind::CaseClause:       analyzer.visit(static_cast<CaseClause&>(stmt)); break;
        case ASTNodeKind::WithStmt:         analyzer.visit(static_cast<WithStmt&>(stmt)); break;
        case ASTNodeKind::GoToStmt:         analyzer.visit(static_cast<GoToStmt&>(stmt)); break;
        case ASTNodeKind::OnErrorStmt:      analyzer.visit(static_cast<OnErrorStmt&>(stmt)); break;
        case ASTNodeKind::ResumeStmt:       analyzer.visit(static_cast<ResumeStmt&>(stmt)); break;
        case ASTNodeKind::ErrorStmt:        analyzer.visit(static_cast<ErrorStmt&>(stmt)); break;
        case ASTNodeKind::ExitStmt:         analyzer.visit(static_cast<ExitStmt&>(stmt)); break;
        case ASTNodeKind::CallStmt:         analyzer.visit(static_cast<CallStmt&>(stmt)); break;
        case ASTNodeKind::ReDimStmt:        analyzer.visit(static_cast<ReDimStmt&>(stmt)); break;
        case ASTNodeKind::LabelStmt:        analyzer.visit(static_cast<LabelStmt&>(stmt)); break;
        case ASTNodeKind::OptionStmt:       analyzer.visit(static_cast<OptionStmt&>(stmt)); break;
        case ASTNodeKind::LocalDeclStmt:    analyzer.visit(static_cast<LocalDeclStmt&>(stmt)); break;
        case ASTNodeKind::RaiseEventStmt:   /* P6.5: RaiseEvent在Pass2语义分析中验证事件存在性 */ break;
        // 文件I/O语句
        case ASTNodeKind::OpenStmt:        analyzer.visit(static_cast<OpenStmt&>(stmt)); break;
        case ASTNodeKind::GetStmt:         analyzer.visit(static_cast<GetStmt&>(stmt)); break;
        case ASTNodeKind::PutStmt:         analyzer.visit(static_cast<PutStmt&>(stmt)); break;
        case ASTNodeKind::GoSubStmt:        analyzer.visit(static_cast<GoSubStmt&>(stmt)); break;
case ASTNodeKind::OnGoSubStmt:      /* P17.4: OnGoSub labels validated in GoSub validation pass */ break;
        case ASTNodeKind::OnGoToStmt:      /* P18-A: OnGoTo labels validated in GoSub validation pass */ break;
        case ASTNodeKind::MidStmt:        /* P18-A: Mid$ statement */ break;
        case ASTNodeKind::ReturnStmt:       analyzer.visit(static_cast<ReturnStmt&>(stmt)); break;
        // 其他语句暂不处理
        default: break;
    }
}

// 表达式节点分派
void dispatchExpr(Expr& expr, SemanticAnalyzer& analyzer) {
    switch (expr.kind) {
        case ASTNodeKind::BinaryExpr:          analyzer.visit(static_cast<BinaryExpr&>(expr)); break;
        case ASTNodeKind::UnaryExpr:           analyzer.visit(static_cast<UnaryExpr&>(expr)); break;
        case ASTNodeKind::LiteralExpr:         analyzer.visit(static_cast<LiteralExpr&>(expr)); break;
        case ASTNodeKind::IdentifierExpr:      analyzer.visit(static_cast<IdentifierExpr&>(expr)); break;
        case ASTNodeKind::MemberAccessExpr:    analyzer.visit(static_cast<MemberAccessExpr&>(expr)); break;
        case ASTNodeKind::DictionaryAccessExpr:analyzer.visit(static_cast<DictionaryAccessExpr&>(expr)); break;
        case ASTNodeKind::IndexOrCallExpr:     analyzer.visit(static_cast<IndexOrCallExpr&>(expr)); break;
        case ASTNodeKind::NewExpr:             analyzer.visit(static_cast<NewExpr&>(expr)); break;
        case ASTNodeKind::TypeOfExpr:          analyzer.visit(static_cast<TypeOfExpr&>(expr)); break;
        case ASTNodeKind::AddressOfExpr:       analyzer.visit(static_cast<AddressOfExpr&>(expr)); break;
        case ASTNodeKind::MeExpr:              analyzer.visit(static_cast<MeExpr&>(expr)); break;
        case ASTNodeKind::WithMemberExpr:      analyzer.visit(static_cast<WithMemberExpr&>(expr)); break;
        default: break;
    }
}

// ============================================================
// 构造/析构
// ============================================================

SemanticAnalyzer::SemanticAnalyzer(Diagnostics& diag, bool verbose)
    : diag_(diag), symTab_(diag), verbose_(verbose) {
    for (int i = 0; i < 26; i++) defTypeMap_[i] = Vb6Type::Variant;
}

// ============================================================
// 两遍扫描入口
// ============================================================

bool SemanticAnalyzer::analyze(Module& module) {
    currentModule_ = &module;

    // 注册内置对象、函数和常量
    registerBuiltins();

    // 如果是类模块，注册类符号 (让其他模块可以通过类名引用)
    if (module.isClassModule) {
        auto classSym = std::make_unique<Symbol>(
            SymbolKind::Class, module.moduleName, Vb6Type::Object,
            SourceLocation{module.filename, 1, 1}, AccessLevel::Public
        );
        classSym->instancing = module.instancing;
        // 收集类成员名称
        for (auto& decl : module.declarations) {
            switch (decl->kind) {
                // Fix 092m: 类数据字段类型表 — 供 cgen 的 With 块字段写按目标字段类型
                // 选择 COM 解包函数 (见 Symbol::memberFieldTypes 注释). 仅记录
                // As <简单类型名> 的字段; 其它 (数组/UDT/未声明) 由 cgen 回退默认.
                case ASTNodeKind::VariableDecl: {
                    auto& v092m = static_cast<VariableDecl&>(*decl);
                    // Fix 092p: 登记字段声明原名 — C 结构体成员名按声明生成, 访问点需
                    // 把源码里的大小写变体 (.socket) 规范化回该名.
                    classSym->memberFieldNames[Symbol::toLower(v092m.name)] = v092m.name;
                    if (v092m.asType && v092m.asType->kind == ASTNodeKind::SimpleTypeRef) {
                        classSym->memberFieldTypes[Symbol::toLower(v092m.name)] =
                            static_cast<SimpleTypeRef*>(v092m.asType.get())->name;
                    }
                    break;
                }
                case ASTNodeKind::SubDecl: {
                    auto& s = static_cast<SubDecl&>(*decl);
                    classSym->memberNames.push_back(s.name);
                    // Fix 016: Sub 写入 memberProcKinds (覆盖任意前值)
                    classSym->memberProcKinds[Symbol::toLower(s.name)] = ProcKind::Sub;
                    // Fix 033: memberParams 改在 Pass1 填充 (class init 阶段类型解析不完整, 导致 Variant 回归)
                    break;
                }
                case ASTNodeKind::FunctionDecl: {
                    auto& f = static_cast<FunctionDecl&>(*decl);
                    classSym->memberNames.push_back(f.name);
                    // Fix 015: 记录 Function 返回类型名(仅类型为命名类型 SimpleTypeRef 时)
                    // 用于跨模块 storageKey 冲突场景下的 method chaining 返回类型推断
                    if (f.returnType && f.returnType->kind == ASTNodeKind::SimpleTypeRef) {
                        classSym->memberReturnTypes[Symbol::toLower(f.name)] =
                            static_cast<SimpleTypeRef*>(f.returnType.get())->name;
                    }
                    // Fix 016: Function 写入 memberProcKinds (覆盖任意前值 — 同类内
                    // 不允许 Function 与同名 Property 共存, 故此处覆盖无冲突风险)
                    classSym->memberProcKinds[Symbol::toLower(f.name)] = ProcKind::Function;
                    // Fix 033: memberParams 改在 Pass1 填充 (class init 阶段类型解析不完整)
                    break;
                }
                case ASTNodeKind::PropertyDecl: {
                    auto& p = static_cast<PropertyDecl&>(*decl);
                    // 同名Property Get/Let/Set只加一次memberNames
                    {
                        std::string lower = Symbol::toLower(p.name);
                        bool found = false;
                        for (auto& mn : classSym->memberNames) {
                            if (Symbol::toLower(mn) == lower) { found = true; break; }
                        }
                        if (!found) classSym->memberNames.push_back(p.name);
                    }
                    // Fix 015: 仅 Property Get 有返回值; Let/Set 无返回类型不记录.
                    // 用前判 propKind==PropertyGet 避免被后续 Let/Set 覆盖 Get 的返回类型.
                    if (p.propKind == ProcKind::PropertyGet
                        && p.returnType
                        && p.returnType->kind == ASTNodeKind::SimpleTypeRef) {
                        classSym->memberReturnTypes[Symbol::toLower(p.name)] =
                            static_cast<SimpleTypeRef*>(p.returnType.get())->name;
                    }
                    // Fix 016: 按 ProcKind 写入 memberProcKinds, 同名共存时按
                    // 读上下文优先级 Get > Function > Sub > Let > Set 选择, 即:
                    // - Get 总是覆盖 (最高优先级)
                    // - Let 仅在键不存在或现有是 Let/Set 时写入 (不覆盖 Get/Function/Sub)
                    // - Set 仅在键不存在或现有是 Set/Let 时写入 (不覆盖 Get/Function/Sub/Let)
                    // Fix 033: memberParams 改在 Pass1 填充 (此处类型解析不完整, 且 memberProcKinds
                    //         在本阶段结束后已确定最终胜出者, Pass1 可直接据其判定 wins)
                    {
                        std::string lower = Symbol::toLower(p.name);
                        auto it = classSym->memberProcKinds.find(lower);
                        bool wins = false;
                        if (p.propKind == ProcKind::PropertyGet) {
                            wins = true;
                        } else if (p.propKind == ProcKind::PropertyLet) {
                            if (it == classSym->memberProcKinds.end()
                                || it->second == ProcKind::PropertyLet
                                || it->second == ProcKind::PropertySet) {
                                wins = true;
                            }
                        } else if (p.propKind == ProcKind::PropertySet) {
                            if (it == classSym->memberProcKinds.end()
                                || it->second == ProcKind::PropertySet) {
                                wins = true;
                            }
                        }
                        if (wins) {
                            if (p.propKind == ProcKind::PropertyGet) {
                                classSym->memberProcKinds[lower] = ProcKind::PropertyGet;
                            } else if (p.propKind == ProcKind::PropertyLet) {
                                classSym->memberProcKinds[lower] = ProcKind::PropertyLet;
                            } else if (p.propKind == ProcKind::PropertySet) {
                                classSym->memberProcKinds[lower] = ProcKind::PropertySet;
                            }
                        }
                    }
                    break;
                }
                case ASTNodeKind::EventDecl: {
                    auto& e = static_cast<EventDecl&>(*decl);
                    classSym->memberNames.push_back(e.name);
                    classSym->eventNames.push_back(e.name);  // P6.5: 收集事件名
                    break;
                }
                default:
                    break;
            }
        }
        // 收集 Implements 列表
        for (auto& impl : module.implements) {
            classSym->implementsNames.push_back(impl->interfaceName);
        }
        symTab_.define(std::move(classSym));
    }

    // P22: 收集 DefType 声明 (字母->隐式类型映射)
    for (auto& dt : module.defTypes) {
        defTypeActive_ = true;
        Vb6Type dtType = Vb6Type::Variant;
        switch (dt->defKind) {
            case DefTypeKind::Bool: dtType = Vb6Type::Boolean; break;
            case DefTypeKind::Byte: dtType = Vb6Type::Byte; break;
            case DefTypeKind::Int:  dtType = Vb6Type::Integer; break;
            case DefTypeKind::Lng:  dtType = Vb6Type::Long; break;
            case DefTypeKind::Cur:  dtType = Vb6Type::Currency; break;
            case DefTypeKind::Sng:  dtType = Vb6Type::Single; break;
            case DefTypeKind::Dbl:  dtType = Vb6Type::Double; break;
            case DefTypeKind::Date: dtType = Vb6Type::Date; break;
            case DefTypeKind::Str:  dtType = Vb6Type::String; break;
            case DefTypeKind::Obj:  dtType = Vb6Type::Object; break;
            case DefTypeKind::Var:  dtType = Vb6Type::Variant; break;
        }
        for (auto& range : dt->ranges) {
            char from = toupper(range.from);
            char to = toupper(range.to);
            for (char c = from; c <= to; c++) {
                int idx = c - 'A';
                if (idx >= 0 && idx < 26) defTypeMap_[idx] = dtType;
            }
        }
    }

    // --- Pass 1: 收集所有模块级声明 ---
    pass_ = 1;
    if (verbose_) {
        std::cerr << "[Sem] Pass 1: collecting declarations..." << std::endl;
    }
    // Fix 049: 预扫描模块级 Type/Enum 声明。
    // VB6 允许模块级 Dim/Const/参数 在类型声明 (Private Type/Public Enum) 之前使用该类型,
    // 编译器会先做整模块的类型收集. 若按语句顺序线性处理 (如 cAsyncSocket.cls 中
    // "Private m_uWindowState() As UcsHelperWindowStateType" 位于该 Type 声明之前),
    // resolveTypeRef 找不到符号而静默回退 Variant, 导致: 1) 生成 VB6_SA_AT(vb6_VARIANT,...)
    // 的字段访问; 2) 对 Variant 收件人的 .成员 解析退化为跨模块类查找 (如 .Pos 误解析到
    // cToast.Pos, 参数个数不符, MSVC C2198/C2039/C2223 错误)。
    for (auto& decl : module.declarations) {
        if (decl->kind == ASTNodeKind::TypeDecl || decl->kind == ASTNodeKind::EnumDecl) {
            dispatchDecl(*decl, *this);
        }
    }
    for (auto& decl : module.declarations) {
        registerDecl(*decl);
    }
    // 处理 Option 语句
    for (auto& opt : module.options) {
        visit(*opt);
    }

    // --- Pass 2: 分析过程体 ---
    pass_ = 2;
    if (verbose_) {
        std::cerr << "[Sem] Pass 2: analyzing procedure bodies..." << std::endl;
    }
    for (auto& decl : module.declarations) {
        dispatchDecl(*decl, *this);
    }

    // Pass 2 结束后，检查未引用的变量 (仅警告)
    if (verbose_) {
        checkUnreferencedSymbols();
    }

    // Pass 2 结束后，验证 Implements 语句
    if (module.isClassModule && !module.implements.empty()) {
        auto* classSym = symTab_.lookupModule(module.moduleName);
        if (classSym && classSym->kind == SymbolKind::Class) {
            for (auto& impl : module.implements) {
                const std::string& ifaceName = impl->interfaceName;
                // 查找接口类符号
                auto* ifaceSym = symTab_.lookupModule(ifaceName);
                if (!ifaceSym || ifaceSym->kind != SymbolKind::Class) {
                    diag_.warn(DiagnosticID::SemUndeclaredIdentifier, impl->loc,
                        "Implements: interface '" + ifaceName + "' not found");
                    continue;
                }
                // 标记接口类
                ifaceSym->isInterface = true;
                // 收集接口方法名
                std::vector<std::string> ifaceMethodNames;
                for (auto& m : ifaceSym->memberNames) {
                    std::string lower = Symbol::toLower(m);
                    ifaceMethodNames.push_back(lower);
                    ifaceSym->interfaceMethodNames.push_back(lower);
                }
                // 验证实现类包含 InterfaceName_MethodName 方法
                for (auto& m : ifaceSym->memberNames) {
                    std::string required = ifaceName + "_" + m;
                    std::string lowerRequired = Symbol::toLower(required);
                    bool found = false;
                    for (auto& cm : classSym->memberNames) {
                        if (Symbol::toLower(cm) == lowerRequired) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        diag_.warn(DiagnosticID::SemUndeclaredIdentifier, impl->loc,
                            "Implements " + ifaceName + ": method '" + required +
                            "' not implemented in class '" + module.moduleName + "'");
                    }
                }
            }
        }
    }

    return !diag_.hasErrors();
}

void SemanticAnalyzer::dumpSymbols(std::ostream& os) const {
    os << "=== Symbol Table Dump ===" << std::endl;

    // 遍历所有作用域的符号
    auto dumpScope = [&](const Scope* scope, const char* label) {
        os << "--- " << label << " (depth=" << symTab_.scopeDepth() << ") ---" << std::endl;
        for (auto& [key, sym] : scope->symbols()) {
            os << "  " << sym->kindName() << " " << sym->name;
            os << " : " << TypeSystem::typeToString(sym->type);
            if (sym->isArray) os << "()";
            os << " [" << (sym->access == AccessLevel::Public ? "Public" : "Private") << "]";
            if (sym->isReferenced) os << " (used)";
            else os << " (unused)";
            os << std::endl;
            // 过程参数
            if (!sym->params.empty()) {
                os << "    params: ";
                for (size_t i = 0; i < sym->params.size(); i++) {
                    if (i > 0) os << ", ";
                    auto& p = sym->params[i];
                    os << (p.isByVal ? "ByVal " : "ByRef ") << p.name;
                    os << " As " << TypeSystem::typeToString(p.type);
                    if (p.isOptional) os << " Optional";
                    if (p.isParamArray) os << " ParamArray";
                }
                os << std::endl;
            }
            // 常量值
            if (sym->hasConstValue) {
                os << "    value: ";
                switch (sym->constType) {
                    case Vb6Type::Boolean:
                        os << (sym->constBoolValue ? "True" : "False");
                        break;
                    case Vb6Type::String:
                        os << "\"" << sym->constStringValue << "\"";
                        break;
                    case Vb6Type::Double:
                    case Vb6Type::Single:
                        os << sym->constFloatValue;
                        break;
                    default:
                        os << sym->constIntValue;
                        break;
                }
                os << std::endl;
            }
        }
    };

    // 模块级符号
    dumpScope(symTab_.moduleScope(), "Module Scope");

    os << "=== End Symbol Table ===" << std::endl;
}

// ============================================================
// 注册声明 (Pass 1)
// ============================================================

void SemanticAnalyzer::registerDecl(Decl& decl) {
    switch (decl.kind) {
        case ASTNodeKind::SubDecl:
        case ASTNodeKind::FunctionDecl:
        case ASTNodeKind::PropertyDecl:
        case ASTNodeKind::DeclareDecl:
        case ASTNodeKind::EventDecl:
            dispatchDecl(decl, *this);
            break;
        case ASTNodeKind::TypeDecl:
        case ASTNodeKind::EnumDecl:
            // 已在 Fix 049 预扫描中注册 (VB6 允许类型声明位于使用之后)
            break;
        case ASTNodeKind::VariableDecl:
            registerVariable(static_cast<VariableDecl&>(decl));
            break;
        case ASTNodeKind::ConstDecl:
            registerConstant(static_cast<ConstDecl&>(decl));
            break;
        default:
            break;
    }
}

void SemanticAnalyzer::registerVariable(VariableDecl& decl) {
    auto sym = std::make_unique<Symbol>(
        SymbolKind::Variable, decl.name,
        resolveTypeOrDefault(decl.name, decl.asType.get()),
        decl.loc, decl.access
    );
    sym->isStatic = decl.isStatic;
    sym->isArray = !decl.dimensions.empty() || decl.isDynamicArray;
    sym->dimCount = (int32_t)decl.dimensions.size();  // P8.1: 多维数组维度数
    // P6.5: 记录WithEvents标志和源类名
    if (decl.isWithEvents) {
        sym->isWithEvents = true;
        if (decl.asType && decl.asType->kind == ASTNodeKind::SimpleTypeRef) {
            sym->withEventsSourceClass = static_cast<SimpleTypeRef*>(decl.asType.get())->name;
        }
    }
    // Fix 010r-11: 记录变量的声明类型名 (用于跨模块解析)
    // 当变量声明为 As ClassName 时，存储类名以便 consuming 模块的 cgen 能正确识别类实例变量
    if (decl.asType && decl.asType->kind == ASTNodeKind::SimpleTypeRef) {
        sym->variableTypeName = static_cast<SimpleTypeRef*>(decl.asType.get())->name;
    }
    symTab_.define(std::move(sym));
}

void SemanticAnalyzer::registerConstant(ConstDecl& decl) {
    auto sym = std::make_unique<Symbol>(
        SymbolKind::Constant, decl.name,
        resolveTypeOrDefault(decl.name, decl.asType.get()),
        decl.loc, decl.access
    );

    // Fix 046: If no explicit As Type, reset to Unknown so value derivation
    // can infer the type from the literal value. Without this, resolveTypeOrDefault
    // returns Variant (default), and the "if (sym->type == Vb6Type::Unknown)" checks
    // in value derivation never fire, leaving all untyped constants as Variant.
    // This causes false-positive Variant detection in codegen (e.g., vb6_VariantToLong
    // wrapping integer constants like HWND_TOPMOST = -1, keybd_event keys = 18, etc.).
    if (!decl.asType && sym->type == Vb6Type::Variant) {
        sym->type = Vb6Type::Unknown;
    }

    // 如果有值表达式，推导常量类型
    if (decl.value) {
        // 简单的常量值推导 (从字面量)
        if (auto* lit = dynamic_cast<LiteralExpr*>(decl.value.get())) {
            sym->hasConstValue = true;
            switch (lit->literalKind) {
                case LiteralKind::Boolean:
                    sym->constType = Vb6Type::Boolean;
                    sym->constBoolValue = lit->boolValue;
                    if (sym->type == Vb6Type::Unknown)
                        sym->type = Vb6Type::Boolean;
                    break;
                case LiteralKind::Integer:
                    sym->constType = Vb6Type::Integer;
                    sym->constIntValue = lit->intValue;
                    if (sym->type == Vb6Type::Unknown)
                        sym->type = Vb6Type::Integer;
                    break;
                case LiteralKind::Long:
                    sym->constType = Vb6Type::Long;
                    sym->constIntValue = lit->longValue;
                    if (sym->type == Vb6Type::Unknown)
                        sym->type = Vb6Type::Long;
                    break;
                case LiteralKind::Single:
                    sym->constType = Vb6Type::Single;
                    sym->constFloatValue = lit->floatValue;
                    if (sym->type == Vb6Type::Unknown)
                        sym->type = Vb6Type::Single;
                    break;
                case LiteralKind::Double:
                    sym->constType = Vb6Type::Double;
                    sym->constFloatValue = lit->doubleValue;
                    if (sym->type == Vb6Type::Unknown)
                        sym->type = Vb6Type::Double;
                    break;
                case LiteralKind::String:
                    sym->constType = Vb6Type::String;
                    // Strip outer quotes and fold VB6 "" escape (two quotes → one)
                    {
                        std::string cv = lit->rawText;
                        if (cv.size() >= 2 && cv.front() == '"' && cv.back() == '"') {
                            cv = cv.substr(1, cv.size() - 2);
                        }
                        std::string cvFolded;
                        cvFolded.reserve(cv.size());
                        for (size_t ci = 0; ci < cv.size(); ci++) {
                            if (cv[ci] == '"' && ci + 1 < cv.size() && cv[ci + 1] == '"') {
                                cvFolded += '"';
                                ci++;
                            } else {
                                cvFolded += cv[ci];
                            }
                        }
                        sym->constStringValue = cvFolded;
                    }
                    if (sym->type == Vb6Type::Unknown)
                        sym->type = Vb6Type::String;
                    break;
                default:
                    sym->constType = Vb6Type::Variant;
                    if (sym->type == Vb6Type::Unknown)
                        sym->type = Vb6Type::Variant;
                    break;
            }
        } else if (auto* ident = dynamic_cast<IdentifierExpr*>(decl.value.get())) {
            // Const值引用另一个符号 - Pass2处理
        } else if (auto* unary = dynamic_cast<UnaryExpr*>(decl.value.get())) {
            // 负数: -42 -> Long
            if (unary->op == UnaryOp::Negate) {
                if (auto* innerLit = dynamic_cast<LiteralExpr*>(unary->operand.get())) {
                    sym->hasConstValue = true;
                    if (innerLit->literalKind == LiteralKind::Long) {
                        sym->constType = Vb6Type::Long;
                        sym->constIntValue = -innerLit->longValue;
                        if (sym->type == Vb6Type::Unknown)
                            sym->type = Vb6Type::Long;
                    } else if (innerLit->literalKind == LiteralKind::Integer) {
                        sym->constType = Vb6Type::Integer;
                        sym->constIntValue = -innerLit->intValue;
                        if (sym->type == Vb6Type::Unknown)
                            sym->type = Vb6Type::Integer;
                    }
                }
            }
        }
    }

    // 没有As Type部分且未推导出类型，默认Variant
    if (sym->type == Vb6Type::Unknown || sym->type == Vb6Type::Empty) {
        sym->type = Vb6Type::Variant;
    }

    symTab_.define(std::move(sym));
}

// ============================================================
// 类型引用解析
// ============================================================

Vb6Type SemanticAnalyzer::resolveTypeOrDefault(const std::string& name, ASTNode* typeRef) {
    Vb6Type t = resolveTypeRef(typeRef);
    // P22: If no explicit type and DefType is active, use DefType inference
    if (!typeRef && defTypeActive_ && !name.empty()) {
        char first = toupper(name[0]);
        int idx = first - 'A';
        if (idx >= 0 && idx < 26 && defTypeMap_[idx] != Vb6Type::Variant) {
            return defTypeMap_[idx];
        }
    }
    return t;
}
Vb6Type SemanticAnalyzer::resolveTypeRef(ASTNode* typeRef) {
    if (!typeRef) return Vb6Type::Variant;  // VB6默认: Variant

    switch (typeRef->kind) {
        case ASTNodeKind::SimpleTypeRef: {
            auto& simple = static_cast<SimpleTypeRef&>(*typeRef);
            Vb6Type t = typeSys_.resolveTypeName(simple.name);
            if (t == Vb6Type::Unknown) {
                // Fix 069: "As Any" 是 VB6 Declare 语句中故意使用的基础类型,
                // 映射为 Vb6Type::Unknown 且不应被回退为 Variant.
                // 原代码将所有 Unknown 统一回退为 Variant, 导致 As Any 参数
                // 在 calleeParams 中类型为 Variant → 调用点生成 VARIANT 复合字面量
                // 而非 (void*) 指针 → MSVC C2440 类型转换错误.
                std::string lowerName = Symbol::toLower(simple.name);
                if (lowerName == "any") {
                    return Vb6Type::Unknown;
                }
                // Fix 040a: VB6内置对象类型 (Collection, ErrObject等) → Object (void*).
                // 与 cgen_base.cpp mapTypeRef 的 vb6BuiltinObjTypes 集合保持一致.
                // 若返回 Variant, 则 ByVal Collection 参数会被 Fix 024 P2 错误地用
                // vb6_VariantFromValue() 包装 void* 指针 → C2172 (实参不是指针).
                static const std::unordered_set<std::string> vb6BuiltinObjTypes = {
                    "Collection", "Forms", "ErrObject", "App", "Screen", "Printer", "Clipboard",
                    // VB6 内建对象类型: 通用控件/窗体都是对象引用 (void*).
                    // 未识别会被宽松回退为 Variant, 与 cgen_base mapTypeRef 的
                    // "未知类型 → void*" 兜底不一致 → ByRef 对象实参被误包成
                    // VARIANT 复合字面量 (BalloonTooltips cTT.CreateToolTip
                    // ParentControl As Control → *objControl 读到 vt).
                    "Control", "Form"
                };
                // Fix 040a: strip VBA. prefix (e.g. VBA.ErrObject → ErrObject)
                std::string typeName = simple.name;
                if (typeName.size() > 4 && typeName.compare(0, 4, "VBA.") == 0) {
                    typeName = typeName.substr(4);
                }
                if (vb6BuiltinObjTypes.count(typeName)) {
                    return Vb6Type::Object;
                }
                                // 可能是用户自定义类型 -> 在符号表中查找
                std::string lower = Symbol::toLower(simple.name);
                if (auto* sym = symTab_.lookupModule(lower)) {
                    if (sym->kind == SymbolKind::UserDefinedType)
                        return Vb6Type::UserDefinedType;
                    if (sym->kind == SymbolKind::EnumType)
                        return Vb6Type::Long;  // Enum成员是Long
                    if (sym->kind == SymbolKind::ComClass || sym->kind == SymbolKind::ComInterface || sym->kind == SymbolKind::ComModule || sym->kind == SymbolKind::ComGlobalNs)
                        return Vb6Type::Object;
                    if (sym->kind == SymbolKind::Class)
                        return Vb6Type::Object;
                }
                // 限定类型名 (如 Scripting.Dictionary): 用最后一部分查找
                size_t dotPos = simple.name.find('.');
                if (dotPos != std::string::npos) {
                    std::string shortName = simple.name.substr(dotPos + 1);
                    std::string shortLower = Symbol::toLower(shortName);
                    if (auto* sym2 = symTab_.lookupModule(shortLower)) {
                        if (sym2->kind == SymbolKind::ComClass || sym2->kind == SymbolKind::ComInterface || sym2->kind == SymbolKind::ComModule)
                            return Vb6Type::Object;
                        if (sym2->kind == SymbolKind::Class)
                            return Vb6Type::Object;
                    }
                }
                // Fix 050: COM 枚举类型 (如 DataTypeEnum, CursorTypeEnum, LockTypeEnum)
                // 来自引用的 COM 类型库 (如 ADODB)，不在项目符号表中。
                // VB6 中所有 Enum 类型都是 Long (32位整数)，因此以 "Enum" 结尾的
                // 未识别类型名应返回 Long 而非 Variant，避免 vb6_VARIANT→int32_t C2440 错误。
                {
                    std::string lowerName = Symbol::toLower(simple.name);
                    if (lowerName.size() > 4 &&
                        lowerName.compare(lowerName.size() - 4, 4, "enum") == 0) {
                        return Vb6Type::Long;
                    }
                }
                // 未识别类型 → Variant (宽松策略)
                return Vb6Type::Variant;
            }
            return t;
        }
        case ASTNodeKind::ArrayTypeRef: {
            auto& arr = static_cast<ArrayTypeRef&>(*typeRef);
            // 数组元素类型
            Vb6Type elemType = resolveTypeRef(arr.elementType.get());
            return static_cast<Vb6Type>(static_cast<uint16_t>(elemType) |
                                         static_cast<uint16_t>(Vb6Type::Array));
        }
        case ASTNodeKind::FixedStringTypeRef:
            return Vb6Type::String;  // 定长字符串仍为String类型
        default:
            return Vb6Type::Variant;
    }
}
} // namespace vb6c3
