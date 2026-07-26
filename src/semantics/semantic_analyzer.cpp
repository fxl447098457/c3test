#include "semantics/semantic_analyzer.hpp"
#include <algorithm>
#include <cctype>
#include <tuple>
#include <initializer_list>

namespace vb6c3 {

// ============================================================
// 手动分派辅助 (AST节点无accept, 按kind分派到visitor)
// ============================================================

// 声明节点分派
static void dispatchDecl(Decl& decl, SemanticAnalyzer& analyzer) {
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
static void dispatchStmt(Stmt& stmt, SemanticAnalyzer& analyzer) {
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
static void dispatchExpr(Expr& expr, SemanticAnalyzer& analyzer) {
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
        case ASTNodeKind::TypeDecl:
        case ASTNodeKind::EnumDecl:
        case ASTNodeKind::DeclareDecl:
        case ASTNodeKind::EventDecl:
            dispatchDecl(decl, *this);
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
                    "Collection", "Forms", "ErrObject", "App", "Screen", "Printer", "Clipboard"
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

// ============================================================
// 声明 Visitor (Pass1注册, Pass2分析体)
// ============================================================

void SemanticAnalyzer::visit(Module& node) {
    // 不在这里遍历, analyze()负责两遍逻辑
}

void SemanticAnalyzer::visit(SubDecl& node) {
    if (pass_ == 1) {
        // Pass1: 注册Sub符号
        auto sym = std::make_unique<Symbol>(
            SymbolKind::Sub, node.name,
            Vb6Type::Void, node.loc, node.access
        );
        sym->isStatic = node.isStatic;

        // 注册参数
        for (auto& param : node.params) {
            ParameterInfo pi;
            pi.name = param->name;
            pi.type = resolveTypeOrDefault(param->name, param->asType.get());
            pi.isByVal = param->isByVal;
            pi.isOptional = param->isOptional;
            pi.isParamArray = param->isParamArray;
            // P14.1.4: 传播Optional默认值
            if (param->isOptional) {
                pi.hasDefaultValue = (param->defaultValue != nullptr);
                pi.defaultValueExpr = evalOptionalDefault(param->defaultValue.get(), pi.type);
            }
            sym->params.push_back(std::move(pi));
        }

        // Fix 033: Pass1 填充 Class 符号的 memberParams (类型解析正确, 避免 class init phase 的 Variant 回归)
        if (currentModule_->isClassModule) {
            auto* classSym = symTab_.lookupModule(Symbol::toLower(currentModule_->moduleName));
            if (classSym && classSym->kind == SymbolKind::Class) {
                // Sub: 覆盖 (与 memberProcKinds Sub 行为一致; VB6 不允许 Sub 与同名 Property 共存)
                classSym->memberParams[Symbol::toLower(node.name)] = sym->params;
            }
        }

        symTab_.define(std::move(sym));
    } else {
        // Pass2: 分析过程体
        if (verbose_) std::cerr << "[Sem]   Sub: " << node.name << std::endl;
        auto* sym = symTab_.lookupModule(node.name);
        if (!sym) return;  // 注册失败则跳过

        // Fix 047: Re-resolve parameter types in Pass 2 after cross-module resolution.
        // Enum/UDT types from other modules are not visible during Pass 1, so params
        // declared as As SomeEnum get resolved to Variant in Pass 1. In Pass 2, after
        // runCrossModuleResolution() has injected external symbols, re-resolve to get
        // the correct type (e.g., Enum → Long). Also update classSym->memberParams.
        if (sym->params.size() == node.params.size()) {
            bool paramsChanged = false;
            for (size_t i = 0; i < node.params.size(); i++) {
                Vb6Type newType = resolveTypeOrDefault(node.params[i]->name, node.params[i]->asType.get());
                if (newType != sym->params[i].type) {
                    sym->params[i].type = newType;
                    paramsChanged = true;
                }
            }
            if (paramsChanged && currentModule_->isClassModule) {
                auto* classSym = symTab_.lookupModule(Symbol::toLower(currentModule_->moduleName));
                if (classSym && classSym->kind == SymbolKind::Class) {
                    classSym->memberParams[Symbol::toLower(node.name)] = sym->params;
                }
            }
        }

        currentProc_ = sym;
        symTab_.pushScope(ScopeKind::Procedure);

        // 注册参数到过程作用域
        for (auto& param : node.params) {
            auto paramSym = std::make_unique<Symbol>(
                SymbolKind::Parameter, param->name,
                resolveTypeOrDefault(param->name, param->asType.get()),
                param->loc, AccessLevel::Private
            );
            symTab_.define(std::move(paramSym));
        }

        // 分析过程体
        analyzeStmtList(node.body);

        symTab_.popScope();
        // P12.5: GoSub标签边界验证（GoSub目标必须在当前过程内）
        for (auto& [gosubLabel, gosubLoc] : gosubTargetLabels_) {
            bool found = false;
            for (auto& declLabel : declaredLabels_) {
                if (Symbol::toLower(declLabel) == Symbol::toLower(gosubLabel)) {
                    found = true; break;
                }
            }
            if (!found) {
                diag_.error(DiagnosticID::SemUndeclaredIdentifier, gosubLoc,
                    "GoSub target label '" + gosubLabel + "' not found in current procedure");
            }
        }
        declaredLabels_.clear();
        gosubTargetLabels_.clear();
        currentProc_ = nullptr;
    }
}

void SemanticAnalyzer::visit(FunctionDecl& node) {
    if (pass_ == 1) {
        // Pass1: 注册Function符号
        Vb6Type retType = resolveTypeOrDefault(node.name, node.returnType.get());
        auto sym = std::make_unique<Symbol>(
            SymbolKind::Function, node.name,
            retType, node.loc, node.access
        );
        sym->isStatic = node.isStatic;

        // Fix 015: 记录Function的返回类型名(若返回类/UDT等命名类型)
        // 用于cgen解析 method chaining: db.Sql(s).Exec(...) 链式调用时
        // 需要根据 Sql 的返回类名(cDataBase)分发 .Exec → vb6_cDataBase_Exec
        if (node.returnType && node.returnType->kind == ASTNodeKind::SimpleTypeRef) {
            sym->variableTypeName = static_cast<SimpleTypeRef*>(node.returnType.get())->name;
        }

        // 注册参数
        for (auto& param : node.params) {
            ParameterInfo pi;
            pi.name = param->name;
            pi.type = resolveTypeOrDefault(param->name, param->asType.get());
            pi.isByVal = param->isByVal;
            pi.isOptional = param->isOptional;
            pi.isParamArray = param->isParamArray;
            // P14.1.4: 传播Optional默认值
            if (param->isOptional) {
                pi.hasDefaultValue = (param->defaultValue != nullptr);
                pi.defaultValueExpr = evalOptionalDefault(param->defaultValue.get(), pi.type);
            }
            sym->params.push_back(std::move(pi));
        }

        // Fix 033: Pass1 填充 Class 符号的 memberParams (类型解析正确)
        if (currentModule_->isClassModule) {
            auto* classSym = symTab_.lookupModule(Symbol::toLower(currentModule_->moduleName));
            if (classSym && classSym->kind == SymbolKind::Class) {
                // Function: 覆盖 (与 memberProcKinds Function 行为一致)
                classSym->memberParams[Symbol::toLower(node.name)] = sym->params;
            }
        }

        symTab_.define(std::move(sym));
    } else {
        // Pass2: 分析过程体
        if (verbose_) std::cerr << "[Sem]   Function: " << node.name << std::endl;
        auto* sym = symTab_.lookupModule(node.name);
        if (!sym) return;

        // Fix 047: Re-resolve parameter types AND return type in Pass 2.
        // Enum/UDT types from other modules are not visible during Pass 1.
        if (sym->params.size() == node.params.size()) {
            bool paramsChanged = false;
            for (size_t i = 0; i < node.params.size(); i++) {
                Vb6Type newType = resolveTypeOrDefault(node.params[i]->name, node.params[i]->asType.get());
                if (newType != sym->params[i].type) {
                    sym->params[i].type = newType;
                    paramsChanged = true;
                }
            }
            // Also re-resolve return type
            Vb6Type newRetType = resolveTypeOrDefault(node.name, node.returnType.get());
            if (newRetType != sym->type) {
                sym->type = newRetType;
                paramsChanged = true;
            }
            if (paramsChanged && currentModule_->isClassModule) {
                auto* classSym = symTab_.lookupModule(Symbol::toLower(currentModule_->moduleName));
                if (classSym && classSym->kind == SymbolKind::Class) {
                    classSym->memberParams[Symbol::toLower(node.name)] = sym->params;
                }
            }
        }

        currentProc_ = sym;
        symTab_.pushScope(ScopeKind::Procedure);

        // 注册参数
        for (auto& param : node.params) {
            auto paramSym = std::make_unique<Symbol>(
                SymbolKind::Parameter, param->name,
                resolveTypeOrDefault(param->name, param->asType.get()),
                param->loc, AccessLevel::Private
            );
            symTab_.define(std::move(paramSym));
        }

        analyzeStmtList(node.body);

        symTab_.popScope();
        // P12.5: GoSub标签边界验证（GoSub目标必须在当前过程内）
        for (auto& [gosubLabel, gosubLoc] : gosubTargetLabels_) {
            bool found = false;
            for (auto& declLabel : declaredLabels_) {
                if (Symbol::toLower(declLabel) == Symbol::toLower(gosubLabel)) {
                    found = true; break;
                }
            }
            if (!found) {
                diag_.error(DiagnosticID::SemUndeclaredIdentifier, gosubLoc,
                    "GoSub target label '" + gosubLabel + "' not found in current procedure");
            }
        }
        declaredLabels_.clear();
        gosubTargetLabels_.clear();
        currentProc_ = nullptr;
    }
}

void SemanticAnalyzer::visit(PropertyDecl& node) {
    if (pass_ == 1) {
        SymbolKind sk;
        switch (node.propKind) {
            case ProcKind::PropertyGet:  sk = SymbolKind::PropertyGet; break;
            case ProcKind::PropertyLet:  sk = SymbolKind::PropertyLet; break;
            case ProcKind::PropertySet:  sk = SymbolKind::PropertySet; break;
            default:                     sk = SymbolKind::PropertyGet; break;
        }

        Vb6Type retType = resolveTypeOrDefault(node.name, node.returnType.get());
        auto sym = std::make_unique<Symbol>(sk, node.name, retType, node.loc, node.access);

        // Fix 015: 记录 Property Get 的返回类型名(若返回类/UDT等命名类型)
        // 用于 cgen 解析 method chaining: obj.GetContainer().Method() 链式调用
        if (sk == SymbolKind::PropertyGet
            && node.returnType && node.returnType->kind == ASTNodeKind::SimpleTypeRef) {
            sym->variableTypeName = static_cast<SimpleTypeRef*>(node.returnType.get())->name;
        }

        for (auto& param : node.params) {
            ParameterInfo pi;
            pi.name = param->name;
            pi.type = resolveTypeOrDefault(param->name, param->asType.get());
            pi.isByVal = param->isByVal;
            pi.isOptional = param->isOptional;
            pi.isParamArray = param->isParamArray;
            // P14.1.4: 传播Optional默认值
            if (param->isOptional) {
                pi.hasDefaultValue = (param->defaultValue != nullptr);
                pi.defaultValueExpr = evalOptionalDefault(param->defaultValue.get(), pi.type);
            }
            sym->params.push_back(std::move(pi));
        }

        // Fix 033: Pass1 填充 Class 符号的 memberParams (类型解析正确, 按 Get > Function > Sub > Let > Set 优先级)
        if (currentModule_->isClassModule) {
            auto* classSym = symTab_.lookupModule(Symbol::toLower(currentModule_->moduleName));
            if (classSym && classSym->kind == SymbolKind::Class) {
                std::string lower = Symbol::toLower(node.name);
                // memberProcKinds 在 class init 阶段已确定最终胜出者, 据此判定当前 Property 是否为胜出者
                auto it = classSym->memberProcKinds.find(lower);
                bool wins = false;
                if (node.propKind == ProcKind::PropertyGet) {
                    wins = true;  // Get 总是胜出
                } else if (node.propKind == ProcKind::PropertyLet) {
                    wins = (it != classSym->memberProcKinds.end()
                            && it->second == ProcKind::PropertyLet);
                } else if (node.propKind == ProcKind::PropertySet) {
                    wins = (it != classSym->memberProcKinds.end()
                            && it->second == ProcKind::PropertySet);
                }
                if (wins) {
                    classSym->memberParams[lower] = sym->params;
                }
            }
        }

        symTab_.define(std::move(sym));
    } else {
        // Pass2: 分析过程体 — 使用lookupModuleByKind精确查找同名Property
        SymbolKind sk;
        switch (node.propKind) {
            case ProcKind::PropertyGet:  sk = SymbolKind::PropertyGet; break;
            case ProcKind::PropertyLet:  sk = SymbolKind::PropertyLet; break;
            case ProcKind::PropertySet:  sk = SymbolKind::PropertySet; break;
            default:                     sk = SymbolKind::PropertyGet; break;
        }
        auto* sym = symTab_.lookupModuleByKind(node.name, sk);
        if (!sym) return;

        // Fix 047: Re-resolve parameter types AND return type in Pass 2.
        // Enum/UDT types from other modules are not visible during Pass 1.
        if (sym->params.size() == node.params.size()) {
            bool paramsChanged = false;
            for (size_t i = 0; i < node.params.size(); i++) {
                Vb6Type newType = resolveTypeOrDefault(node.params[i]->name, node.params[i]->asType.get());
                if (newType != sym->params[i].type) {
                    sym->params[i].type = newType;
                    paramsChanged = true;
                }
            }
            // Also re-resolve return type (for PropertyGet)
            if (sk == SymbolKind::PropertyGet) {
                Vb6Type newRetType = resolveTypeOrDefault(node.name, node.returnType.get());
                if (newRetType != sym->type) {
                    sym->type = newRetType;
                    paramsChanged = true;
                }
            }
            if (paramsChanged && currentModule_->isClassModule) {
                auto* classSym = symTab_.lookupModule(Symbol::toLower(currentModule_->moduleName));
                if (classSym && classSym->kind == SymbolKind::Class) {
                    std::string lower = Symbol::toLower(node.name);
                    auto it = classSym->memberProcKinds.find(lower);
                    bool wins = false;
                    if (node.propKind == ProcKind::PropertyGet) {
                        wins = true;
                    } else if (node.propKind == ProcKind::PropertyLet) {
                        wins = (it != classSym->memberProcKinds.end()
                                && it->second == ProcKind::PropertyLet);
                    } else if (node.propKind == ProcKind::PropertySet) {
                        wins = (it != classSym->memberProcKinds.end()
                                && it->second == ProcKind::PropertySet);
                    }
                    if (wins) {
                        classSym->memberParams[lower] = sym->params;
                    }
                }
            }
        }

        currentProc_ = sym;
        symTab_.pushScope(ScopeKind::Procedure);

        for (auto& param : node.params) {
            auto paramSym = std::make_unique<Symbol>(
                SymbolKind::Parameter, param->name,
                resolveTypeOrDefault(param->name, param->asType.get()),
                param->loc, AccessLevel::Private
            );
            symTab_.define(std::move(paramSym));
        }

        analyzeStmtList(node.body);

        symTab_.popScope();
        // P12.5: GoSub标签边界验证（GoSub目标必须在当前过程内）
        for (auto& [gosubLabel, gosubLoc] : gosubTargetLabels_) {
            bool found = false;
            for (auto& declLabel : declaredLabels_) {
                if (Symbol::toLower(declLabel) == Symbol::toLower(gosubLabel)) {
                    found = true; break;
                }
            }
            if (!found) {
                diag_.error(DiagnosticID::SemUndeclaredIdentifier, gosubLoc,
                    "GoSub target label '" + gosubLabel + "' not found in current procedure");
            }
        }
        declaredLabels_.clear();
        gosubTargetLabels_.clear();
        currentProc_ = nullptr;
    }
}

void SemanticAnalyzer::visit(TypeDecl& node) {
    if (pass_ == 1) {
        auto sym = std::make_unique<Symbol>(
            SymbolKind::UserDefinedType, node.name,
            Vb6Type::UserDefinedType, node.loc, node.access
        );
        // P20-21: 注册UDT成员信息
        for (const auto& memberPtr : node.members) {
            Symbol::UdtMemberInfo mi;
            mi.name = memberPtr->name;
            if (memberPtr->type) {
                mi.type = resolveTypeRef(memberPtr->type.get());
                // 若类型是UDT/Enum等命名类型, 保存类型引用名
                if (auto* stRef = dynamic_cast<SimpleTypeRef*>(memberPtr->type.get())) {
                    if (mi.type == Vb6Type::UserDefinedType) {
                        mi.typeRefName = stRef->name;
                    }
                }
            }
            if (memberPtr->arraySize) {
                mi.arraySize = 1;
            }
            // Fix 037: 传播动态数组标记 (`() As Type`) — UdtMemberInfo.isArrayDynamic
            if (memberPtr->isArrayDynamic) {
                mi.isArrayDynamic = true;
            }
            sym->udtMembers.push_back(std::move(mi));
        }
        symTab_.define(std::move(sym));
    }
}

void SemanticAnalyzer::visit(TypeMember& node) {
    // P20-21: UDT成员已在TypeDecl中统一处理
}

void SemanticAnalyzer::visit(EnumDecl& node) {
    if (pass_ == 1) {
        auto sym = std::make_unique<Symbol>(
            SymbolKind::EnumType, node.name,
            Vb6Type::Long, node.loc, node.access
        );
        symTab_.define(std::move(sym));

        // 注册Enum成员为常量
        int64_t nextValue = 0;
        for (auto& member : node.members) {
            auto memberSym = std::make_unique<Symbol>(
                SymbolKind::EnumMember, member->name,
                Vb6Type::Long, member->loc, node.access
            );
            memberSym->hasConstValue = true;
            memberSym->constType = Vb6Type::Long;

            // 如果有显式值
            if (member->value) {
                if (auto* lit = dynamic_cast<LiteralExpr*>(member->value.get())) {
                    if (lit->literalKind == LiteralKind::Long)
                        nextValue = lit->longValue;
                    else if (lit->literalKind == LiteralKind::Integer)
                        nextValue = lit->intValue;
                } else if (auto* unary = dynamic_cast<UnaryExpr*>(member->value.get())) {
                    // Handle negative literals: -1, -42, etc.
                    if (auto* inner = dynamic_cast<LiteralExpr*>(unary->operand.get())) {
                        int64_t v = 0;
                        if (inner->literalKind == LiteralKind::Long) v = inner->longValue;
                        else if (inner->literalKind == LiteralKind::Integer) v = inner->intValue;
                        if (unary->op == UnaryOp::Negate) nextValue = -v;
                        else nextValue = v;
                    }
                }
            }
            memberSym->constIntValue = nextValue;
            nextValue++;

            symTab_.define(std::move(memberSym));
        }
    }
}

void SemanticAnalyzer::visit(EnumMember& node) {
    // 在EnumDecl中处理
}

void SemanticAnalyzer::visit(DeclareDecl& node) {
    if (pass_ == 1) {
        SymbolKind sk = (node.procKind == ProcKind::Sub)
                        ? SymbolKind::DeclareSub : SymbolKind::DeclareFunc;

        Vb6Type retType = Vb6Type::Void;
        if (node.procKind == ProcKind::Function) {
            retType = resolveTypeRef(node.returnType.get());
        }

        auto sym = std::make_unique<Symbol>(sk, node.name, retType, node.loc, node.access);

        for (auto& param : node.params) {
            ParameterInfo pi;
            pi.name = param->name;
            pi.type = resolveTypeOrDefault(param->name, param->asType.get());
            pi.isByVal = param->isByVal;
            pi.isOptional = param->isOptional;
            pi.isParamArray = param->isParamArray;
            // P14.1.4: 传播Optional默认值
            if (param->isOptional) {
                pi.hasDefaultValue = (param->defaultValue != nullptr);
                pi.defaultValueExpr = evalOptionalDefault(param->defaultValue.get(), pi.type);
            }
            sym->params.push_back(std::move(pi));
        }

        symTab_.define(std::move(sym));
    }
    // 外部声明无过程体
}

void SemanticAnalyzer::visit(EventDecl& node) {
    if (pass_ == 1) {
        auto sym = std::make_unique<Symbol>(
            SymbolKind::Event, node.name,
            Vb6Type::Void, node.loc, node.access
        );
        for (auto& param : node.params) {
            ParameterInfo pi;
            pi.name = param->name;
            pi.type = resolveTypeOrDefault(param->name, param->asType.get());
            pi.isByVal = param->isByVal;
            pi.isOptional = param->isOptional;
            pi.isParamArray = param->isParamArray;
            // P14.1.4: 传播Optional默认值
            if (param->isOptional) {
                pi.hasDefaultValue = (param->defaultValue != nullptr);
                pi.defaultValueExpr = evalOptionalDefault(param->defaultValue.get(), pi.type);
            }
            sym->params.push_back(std::move(pi));
        }
        symTab_.define(std::move(sym));
    }
}

void SemanticAnalyzer::visit(ConstDecl& node) {
    if (pass_ == 1) {
        registerConstant(node);
    }
}

void SemanticAnalyzer::visit(VariableDecl& node) {
    if (pass_ == 1) {
        registerVariable(node);
    }
}

void SemanticAnalyzer::visit(ParameterDecl& node) {
    // 参数在SubDecl/FunctionDecl中处理
}

// ============================================================
// 语句 Visitor (仅Pass2)
// ============================================================

void SemanticAnalyzer::visit(Block& node) {
    analyzeStmtList(node.stmts);
}

void SemanticAnalyzer::analyzeStmtList(StmtList& stmts) {
    for (auto& stmt : stmts) {
        if (verbose_) std::cerr << "[Sem]     stmt: " << stmt->kindName() << std::endl;
        dispatchStmt(*stmt, *this);
    }
}

void SemanticAnalyzer::visit(AssignmentStmt& node) {
    if (verbose_) std::cerr << "[Sem]       assign target=" << (node.target ? node.target->kindName() : "NULL")
                             << " value=" << (node.value ? node.value->kindName() : "NULL") << std::endl;
    if (!node.value || !node.target) {
        // 解析器应保证 target/value 都非空; 若为空则跳过分析
        return;
    }
    Vb6Type valueType = analyzeExpr(*node.value);
    Vb6Type targetType = analyzeExpr(*node.target);

    // 类型兼容性检查
    if (targetType != Vb6Type::Unknown && valueType != Vb6Type::Unknown) {
        checkAssignment(targetType, valueType, node.loc, "assignment");
    }
}

void SemanticAnalyzer::visit(SetStmt& node) {
    Vb6Type valueType = analyzeExpr(*node.value);
    Vb6Type targetType = analyzeExpr(*node.target);

    // Set要求对象类型
    if (valueType != Vb6Type::Object && valueType != Vb6Type::Variant &&
        valueType != Vb6Type::Unknown) {
        diag_.warn(DiagnosticID::SemTypeMismatch, node.loc,
            "Set语句需要对象类型, 实际为 " + std::string(TypeSystem::typeToString(valueType)));
    }
}

void SemanticAnalyzer::visit(LetStmt& node) {
    Vb6Type valueType = analyzeExpr(*node.value);
    Vb6Type targetType = analyzeExpr(*node.target);
    checkAssignment(targetType, valueType, node.loc, "Let");
}

void SemanticAnalyzer::visit(IfStmt& node) {
    // 条件应为Boolean或可转Boolean
    Vb6Type condType = analyzeExpr(*node.condition);
    if (condType != Vb6Type::Unknown && condType != Vb6Type::Boolean &&
        condType != Vb6Type::Variant && !TypeSystem::isNumeric(condType)) {
        diag_.warn(DiagnosticID::SemTypeMismatch, node.loc,
            "If条件应为Boolean, 实际为 " + std::string(TypeSystem::typeToString(condType)));
    }

    analyzeStmtList(node.thenBody);

    for (auto& elseif : node.elseIfs) {
        dispatchStmt(*elseif, *this);
    }

    analyzeStmtList(node.elseBody);
}

void SemanticAnalyzer::visit(ElseIfClause& node) {
    Vb6Type condType = analyzeExpr(*node.condition);
    analyzeStmtList(node.body);
}

void SemanticAnalyzer::visit(ForStmt& node) {
    // 循环变量
    if (optionExplicit_) {
        auto* sym = symTab_.lookup(node.varName);
        if (!sym) {
            diag_.warn(DiagnosticID::SemUndeclaredIdentifier, node.loc,
                "未声明的变量: '" + node.varName + "' (可能来自其他模块)");
        } else {
            sym->isReferenced = true;
        }
    } else {
        markReferenced(node.varName);
    }

    // 起始/终止/步长应为数值
    Vb6Type startType = analyzeExpr(*node.start);
    Vb6Type endType = analyzeExpr(*node.end);

    if (node.step) {
        Vb6Type stepType = analyzeExpr(*node.step);
    }

    // 块级作用域 (VB6不创建块作用域, 但可以在For内声明变量)
    analyzeStmtList(node.body);
}

void SemanticAnalyzer::visit(ForEachStmt& node) {
    markReferenced(node.varName);
    Vb6Type collType = analyzeExpr(*node.collection);
    analyzeStmtList(node.body);
}

void SemanticAnalyzer::visit(DoLoopStmt& node) {
    if (node.condition) {
        Vb6Type condType = analyzeExpr(*node.condition);
    }
    analyzeStmtList(node.body);
}

void SemanticAnalyzer::visit(WhileWendStmt& node) {
    Vb6Type condType = analyzeExpr(*node.condition);
    analyzeStmtList(node.body);
}

void SemanticAnalyzer::visit(SelectCaseStmt& node) {
    Vb6Type testType = analyzeExpr(*node.testExpr);
    for (auto& caseClause : node.cases) {
        dispatchStmt(*caseClause, *this);
    }
    analyzeStmtList(node.elseCase);
}

void SemanticAnalyzer::visit(CaseClause& node) {
    for (auto& cv : node.values) {
        if (cv.value) analyzeExpr(*cv.value);
        if (cv.toValue) analyzeExpr(*cv.toValue);
    }
    analyzeStmtList(node.body);
}

void SemanticAnalyzer::visit(WithStmt& node) {
    Vb6Type objType = analyzeExpr(*node.object);
    withStack_.push_back(objType);

    analyzeStmtList(node.body);

    withStack_.pop_back();
}

void SemanticAnalyzer::visit(GoToStmt& node) {
    // GoTo标签存在性检查 (Pass2简单记录, 后续验证)
}
void SemanticAnalyzer::visit(GoSubStmt& node) {
    // P12.5: GoSub标签记录（验证在过程结束时进行）
    if (pass_ == 2) {
        gosubTargetLabels_.push_back({node.labelName, node.loc});
    }
}
// P17.4: OnGoSub label validation
void SemanticAnalyzer::visit(OnGoSubStmt& node) {
    if (pass_ == 2) {
        for (auto& label : node.labels) {
            gosubTargetLabels_.push_back({label, node.loc});
        }
    }
}

void SemanticAnalyzer::visit(OnGoToStmt& node) {
    // P18-A: OnGoTo labels validated in GoSub validation pass (same as OnGoSub)
    if (pass_ == 2) {
        for (auto& label : node.labels) {
            gosubTargetLabels_.push_back({label, node.loc});
        }
    }
}

void SemanticAnalyzer::visit(MidStmt& node) {
    // P18-A: Mid$ statement — validate expressions
    if (node.target) analyzeExpr(*node.target);
    if (node.start)  analyzeExpr(*node.start);
    if (node.hasLength && node.length) analyzeExpr(*node.length);
    if (node.value)  analyzeExpr(*node.value);
}

void SemanticAnalyzer::visit(ReturnStmt& node) {
    // Return from GoSub - 无需特殊语义检查
    // 运行时由vb6_gosub_stack处理
}

void SemanticAnalyzer::visit(OnErrorStmt& node) {
    // On Error语句无需类型检查
}

void SemanticAnalyzer::visit(ResumeStmt& node) {
    // Resume语句无需类型检查
}

void SemanticAnalyzer::visit(ErrorStmt& node) {
    if (node.errorNumber) analyzeExpr(*node.errorNumber);
}

void SemanticAnalyzer::visit(ExitStmt& node) {
    // Exit语句无需类型检查
}

void SemanticAnalyzer::visit(CallStmt& node) {
    analyzeExpr(*node.callee);
}

void SemanticAnalyzer::visit(ReDimStmt& node) {
    markReferenced(node.varName);
    for (auto& dim : node.dimensions) {
        if (dim.lower) analyzeExpr(*dim.lower);
        if (dim.upper) analyzeExpr(*dim.upper);
    }
}

void SemanticAnalyzer::visit(LabelStmt& node) {
    // 注册标签
    if (pass_ == 2) {
        std::string lower = Symbol::toLower(node.labelName);
        bool found = false;
        for (auto& label : declaredLabels_) {
            if (Symbol::toLower(label) == lower) { found = true; break; }
        }
        if (!found) {
            declaredLabels_.push_back(node.labelName);
        }
    }
}

void SemanticAnalyzer::visit(OptionStmt& node) {
    if (node.optionKind == OptionKind::Explicit) {
        optionExplicit_ = true;
    }
}

void SemanticAnalyzer::visit(LocalDeclStmt& node) {
    // 过程体内的局部声明
    if (pass_ == 2 && node.decl) {
        switch (node.decl->kind) {
            case ASTNodeKind::VariableDecl: {
                auto& varDecl = static_cast<VariableDecl&>(*node.decl);
                auto sym = std::make_unique<Symbol>(
                    SymbolKind::Variable, varDecl.name,
                    resolveTypeOrDefault(varDecl.name, varDecl.asType.get()),
                    varDecl.loc, varDecl.access
                );
                sym->isStatic = varDecl.isStatic;
                sym->isArray = !varDecl.dimensions.empty() || varDecl.isDynamicArray;
                sym->dimCount = (int32_t)varDecl.dimensions.size();  // P8.1: 多维数组维度数
                symTab_.define(std::move(sym));
                break;
            }
            case ASTNodeKind::ConstDecl: {
                auto& constDecl = static_cast<ConstDecl&>(*node.decl);
                auto sym = std::make_unique<Symbol>(
                    SymbolKind::Constant, constDecl.name,
                    resolveTypeOrDefault(constDecl.name, constDecl.asType.get()),
                    constDecl.loc, constDecl.access
                );
                // 简化: 常量值推导 (与registerConstant类似)
                if (sym->type == Vb6Type::Unknown || sym->type == Vb6Type::Empty)
                    sym->type = Vb6Type::Variant;
                symTab_.define(std::move(sym));
                break;
            }
            default:
                break;
        }
    }
}

// ============================================================
// 表达式 Visitor
// ============================================================

Vb6Type SemanticAnalyzer::analyzeExpr(Expr& expr) {
    lastExprType_ = Vb6Type::Unknown;
    if (verbose_) std::cerr << "[Sem]       expr: " << expr.kindName() << std::endl;
    dispatchExpr(expr, *this);
    return lastExprType_;
}

void SemanticAnalyzer::visit(BinaryExpr& node) {
    Vb6Type leftType = analyzeExpr(*node.left);
    Vb6Type rightType = analyzeExpr(*node.right);

    // 根据运算符推导结果类型
    switch (node.op) {
        case BinaryOp::Concat:
            // & 运算符: 字符串连接
            lastExprType_ = Vb6Type::String;
            break;
        case BinaryOp::Eq: case BinaryOp::Neq:
        case BinaryOp::Lt: case BinaryOp::Gt:
        case BinaryOp::Le: case BinaryOp::Ge:
        case BinaryOp::Like: case BinaryOp::Is:
            // 比较运算符 → Boolean
            lastExprType_ = Vb6Type::Boolean;
            break;
        case BinaryOp::And: case BinaryOp::Or: case BinaryOp::Xor:
        case BinaryOp::Eqv: case BinaryOp::Imp:
            // 逻辑运算符: 如果两边都是Boolean → Boolean, 否则 → 数值提升
            if (leftType == Vb6Type::Boolean && rightType == Vb6Type::Boolean) {
                lastExprType_ = Vb6Type::Boolean;
            } else if (TypeSystem::isNumeric(leftType) && TypeSystem::isNumeric(rightType)) {
                lastExprType_ = TypeSystem::promote(leftType, rightType);
            } else {
                lastExprType_ = Vb6Type::Variant;
            }
            break;
        case BinaryOp::Add: case BinaryOp::Sub:
        case BinaryOp::Mul: case BinaryOp::Div:
        case BinaryOp::IntDiv: case BinaryOp::Mod: case BinaryOp::Pow:
            // 算术运算符 → 数值提升
            if (TypeSystem::isNumeric(leftType) && TypeSystem::isNumeric(rightType)) {
                lastExprType_ = TypeSystem::promote(leftType, rightType);
                // 整除和取模 → Long
                if (node.op == BinaryOp::IntDiv || node.op == BinaryOp::Mod) {
                    lastExprType_ = Vb6Type::Long;
                }
            } else if (leftType == Vb6Type::String && rightType == Vb6Type::String && node.op == BinaryOp::Add) {
                // 字符串 + 字符串 → String (VB6允许但推荐用&)
                lastExprType_ = Vb6Type::String;
            } else {
                lastExprType_ = Vb6Type::Variant;
            }
            break;
    }
}

void SemanticAnalyzer::visit(UnaryExpr& node) {
    Vb6Type operandType = analyzeExpr(*node.operand);

    switch (node.op) {
        case UnaryOp::Negate:
            // -x: 数值类型不变, Boolean → Integer
            if (operandType == Vb6Type::Boolean) {
                lastExprType_ = Vb6Type::Integer;
            } else if (TypeSystem::isNumeric(operandType)) {
                lastExprType_ = operandType;
            } else {
                lastExprType_ = Vb6Type::Variant;
            }
            break;
        case UnaryOp::Not:
            // Not x: Boolean → Boolean, 数值 → 数值(按位取反)
            if (operandType == Vb6Type::Boolean) {
                lastExprType_ = Vb6Type::Boolean;
            } else if (TypeSystem::isIntegral(operandType)) {
                lastExprType_ = operandType;
            } else {
                lastExprType_ = Vb6Type::Variant;
            }
            break;
    }
}

void SemanticAnalyzer::visit(LiteralExpr& node) {
    switch (node.literalKind) {
        case LiteralKind::Integer:  lastExprType_ = Vb6Type::Integer; break;
        case LiteralKind::Long:     lastExprType_ = Vb6Type::Long; break;
        case LiteralKind::Single:   lastExprType_ = Vb6Type::Single; break;
        case LiteralKind::Double:   lastExprType_ = Vb6Type::Double; break;
        case LiteralKind::Currency: lastExprType_ = Vb6Type::Currency; break;
        case LiteralKind::Decimal:  lastExprType_ = Vb6Type::Decimal; break;
        case LiteralKind::String:   lastExprType_ = Vb6Type::String; break;
        case LiteralKind::Date:     lastExprType_ = Vb6Type::Date; break;
        case LiteralKind::Boolean:  lastExprType_ = Vb6Type::Boolean; break;
        case LiteralKind::Nothing:  lastExprType_ = Vb6Type::Object; break;
        case LiteralKind::Empty:    lastExprType_ = Vb6Type::Empty; break;
        case LiteralKind::Null:     lastExprType_ = Vb6Type::Null; break;
    }
}

void SemanticAnalyzer::visit(IdentifierExpr& node) {
    std::string lower = Symbol::toLower(node.name);

    // 在符号表中查找
    auto* sym = symTab_.lookup(node.name);
    if (sym) {
        sym->isReferenced = true;
        lastExprType_ = sym->type;
    } else {
        // 未找到标识符
        if (optionExplicit_ && pass_ == 2) {
            diag_.warn(DiagnosticID::SemUndeclaredIdentifier, node.loc,
                "未声明的标识符: '" + node.name + "' (可能来自其他模块)");
        }
        lastExprType_ = Vb6Type::Variant;  // 宽松模式: 推导为Variant
    }
}

void SemanticAnalyzer::visit(MemberAccessExpr& node) {
    Vb6Type objType = analyzeExpr(*node.object);
    // P20-21: 如果object是UDT, 查找成员类型
    if (objType == Vb6Type::UserDefinedType) {
        // 查找UDT符号获取成员类型
        if (auto* ident = dynamic_cast<IdentifierExpr*>(node.object.get())) {
            auto* udtSym = symTab_.lookup(ident->name);
            if (udtSym && udtSym->kind == SymbolKind::UserDefinedType) {
                std::string memberLower = node.memberName;
                std::transform(memberLower.begin(), memberLower.end(), memberLower.begin(), ::tolower);
                for (const auto& mi : udtSym->udtMembers) {
                    std::string miLower = mi.name;
                    std::transform(miLower.begin(), miLower.end(), miLower.begin(), ::tolower);
                    if (miLower == memberLower) {
                        lastExprType_ = mi.type;
                        return;
                    }
                }
            }
        }
    }
    // P24-04: ComModule全局函数访问 (VBMAN.Version)
    if (auto* ident = dynamic_cast<IdentifierExpr*>(node.object.get())) {
        auto* modSym = symTab_.lookup(ident->name);
        if (modSym && modSym->kind == SymbolKind::ComModule) {
            std::string memberLower = node.memberName;
            std::transform(memberLower.begin(), memberLower.end(), memberLower.begin(), ::tolower);
            auto it = modSym->comModuleFunctions.find(memberLower);
            if (it != modSym->comModuleFunctions.end()) {
                lastExprType_ = it->second.returnType;
                return;
            }
        }
    }
    // P24-04: ComGlobalNs — VB_GlobalNameSpace promoted函数访问 (如 VBMAN.Version)
    // VBMAN是提升到全局的函数, VBMAN()返回cVBMAN COM对象, .Version是cVBMAN的方法
    // 所以 VBMAN.Version = VBMAN().Version, 即先调用 promoted函数获取对象, 再访问成员
    if (auto* ident = dynamic_cast<IdentifierExpr*>(node.object.get())) {
        auto* gnsSym = symTab_.lookup(ident->name);
        if (gnsSym && gnsSym->kind == SymbolKind::ComGlobalNs) {
            // promoted函数返回Object (COM对象), 成员访问走晚绑定
            lastExprType_ = Vb6Type::Variant;
            return;
        }
    }
    // 简化: 非UDT或未找到成员, 推导为Variant
    lastExprType_ = Vb6Type::Variant;
}

void SemanticAnalyzer::visit(DictionaryAccessExpr& node) {
    Vb6Type objType = analyzeExpr(*node.object);
    lastExprType_ = Vb6Type::Variant;
}

void SemanticAnalyzer::visit(IndexOrCallExpr& node) {
    // 分析被调用者
    Vb6Type calleeType = Vb6Type::Unknown;

    // 判断是函数调用还是数组索引
    if (auto* ident = dynamic_cast<IdentifierExpr*>(node.callee.get())) {
        auto* sym = symTab_.lookup(ident->name);
        if (sym) {
            sym->isReferenced = true;

            if (sym->kind == SymbolKind::Function ||
                sym->kind == SymbolKind::DeclareFunc) {
                // 函数调用: 返回类型即为表达式类型
                calleeType = sym->type;
                checkCallArgs(sym, node);
            } else if (sym->kind == SymbolKind::Sub ||
                       sym->kind == SymbolKind::DeclareSub) {
                // Sub作为表达式调用 (VB6允许, 返回Variant)
                calleeType = Vb6Type::Variant;
                checkCallArgs(sym, node);
            } else if (sym->isArray) {
                // 数组索引: 元素类型
                uint16_t baseType = static_cast<uint16_t>(sym->type) &
                                    ~static_cast<uint16_t>(Vb6Type::Array);
                calleeType = static_cast<Vb6Type>(baseType);
            } else {
                calleeType = sym->type;
            }
        } else {
            // 可能是内置函数 (MsgBox, Len, etc.) 或未声明
            // 简化: 推导为Variant
            calleeType = Vb6Type::Variant;

            // 检查Option Explicit
            if (optionExplicit_ && pass_ == 2 &&
                node.positional.empty()) {
                // 非数组索引、无参数 → 可能是未声明变量
                // 但无法确定, 这里不做报错
            }
        }
    } else {
        // 成员调用 (obj.method args)
        analyzeExpr(*node.callee);
        calleeType = Vb6Type::Variant;
    }

    // 分析参数
    for (auto& arg : node.positional) {
        analyzeExpr(*arg);
    }
    for (auto& namedArg : node.named) {
        analyzeExpr(*namedArg.value);
    }

    lastExprType_ = calleeType;
}

void SemanticAnalyzer::visit(NewExpr& node) {
    // 查找类名是否在符号表中注册(类符号或作为对象类型引用)
    auto* sym = symTab_.lookup(node.className);
    if (sym && sym->kind == SymbolKind::Class) {
        node.className = sym->name;  // 规范化大小写
    }
    // 即使未找到类符号，也不报错(可能是COM对象或外部类，运行时解析)
    lastExprType_ = Vb6Type::Object;
}

void SemanticAnalyzer::visit(TypeOfExpr& node) {
    analyzeExpr(*node.object);
    lastExprType_ = Vb6Type::Boolean;
}

void SemanticAnalyzer::visit(AddressOfExpr& node) {
    markReferenced(node.funcName);
    lastExprType_ = Vb6Type::Long;  // 函数指针
}

void SemanticAnalyzer::visit(MeExpr& node) {
    // Me代表当前对象实例 (类模块或窗体模块)
    if (currentModule_ && (currentModule_->isClassModule || currentModule_->isFormModule)) {
        lastExprType_ = Vb6Type::Object;
    } else {
        diag_.error(DiagnosticID::SemTypeMismatch, node.loc,
            "'Me' can only be used in class/form modules");
        lastExprType_ = Vb6Type::Object;
    }
}

void SemanticAnalyzer::visit(WithMemberExpr& node) {
    // .member → 访问With对象的成员
    // 简化: 推导为Variant
    lastExprType_ = Vb6Type::Variant;
}

// ============================================================
// 类型引用 Visitor
// ============================================================

void SemanticAnalyzer::visit(SimpleTypeRef& node) {
    // 类型引用不会产生表达式类型
}

void SemanticAnalyzer::visit(ArrayTypeRef& node) {
    // 分析数组维度表达式
    for (auto& dim : node.dimensions) {
        if (dim.lower) analyzeExpr(*dim.lower);
        if (dim.upper) analyzeExpr(*dim.upper);
    }
}

void SemanticAnalyzer::visit(FixedStringTypeRef& node) {
    if (node.length) analyzeExpr(*node.length);
}

// ---- 文件I/O语句 ----

void SemanticAnalyzer::visit(OpenStmt& node) {
    analyzeExpr(*node.pathName);
    if (node.recordLength) analyzeExpr(*node.recordLength);
    analyzeExpr(*node.fileNumber);
}

void SemanticAnalyzer::visit(GetStmt& node) {
    analyzeExpr(*node.fileNumber);
    if (node.recordNumber) analyzeExpr(*node.recordNumber);
    analyzeExpr(*node.varName);
}

void SemanticAnalyzer::visit(PutStmt& node) {
    analyzeExpr(*node.fileNumber);
    if (node.recordNumber) analyzeExpr(*node.recordNumber);
    analyzeExpr(*node.varName);
}

// ============================================================
// 辅助方法
// ============================================================

void SemanticAnalyzer::checkAssignment(Vb6Type targetType, Vb6Type valueType,
                                        const SourceLocation& loc,
                                        const std::string& context) {
    if (!TypeSystem::canImplicitConvert(valueType, targetType)) {
        diag_.warn(DiagnosticID::SemTypeMismatch, loc,
            context + ": 无法将 " + TypeSystem::typeToString(valueType) +
            " 隐式转换为 " + TypeSystem::typeToString(targetType));
    }
}

void SemanticAnalyzer::checkCallArgs(Symbol* procSym, IndexOrCallExpr& callNode) {
    if (!procSym) return;

    // 内置函数未注册参数信息, 跳过参数校验
    if (procSym->isBuiltin) return;

    size_t expectedParams = 0;
    size_t optionalParams = 0;
    bool hasParamArray = false;

    for (auto& p : procSym->params) {
        if (p.isParamArray) {
            hasParamArray = true;
            continue;
        }
        if (p.isOptional) {
            optionalParams++;
        }
        expectedParams++;
    }

    size_t providedArgs = callNode.positional.size() + callNode.named.size();

    if (hasParamArray) {
        // ParamArray: 至少需要 required 个参数
        size_t required = expectedParams - optionalParams;
        if (providedArgs < required) {
            diag_.error(DiagnosticID::SemWrongNumberOfArguments, callNode.loc,
                "参数数量错误: '" + procSym->name + "' 至少需要 " +
                std::to_string(required) + " 个参数, 实际提供 " +
                std::to_string(providedArgs));
        }
    } else {
        size_t required = expectedParams - optionalParams;
        if (providedArgs < required || providedArgs > expectedParams) {
            diag_.error(DiagnosticID::SemWrongNumberOfArguments, callNode.loc,
                "参数数量错误: '" + procSym->name + "' 需要 " +
                std::to_string(required) + "-" + std::to_string(expectedParams) +
                " 个参数, 实际提供 " + std::to_string(providedArgs));
        }
    }

    // 命名参数检查
    for (auto& namedArg : callNode.named) {
        bool found = false;
        std::string lowerName = Symbol::toLower(namedArg.name);
        for (auto& p : procSym->params) {
            if (Symbol::toLower(p.name) == lowerName) {
                found = true;
                break;
            }
        }
        if (!found) {
            diag_.error(DiagnosticID::SemNamedArgNotFound, callNode.loc,
                "命名参数未找到: '" + namedArg.name + "' (过程 '" + procSym->name + "')");
        }
    }

    // 重复命名参数检查
    std::vector<std::string> seenNamedArgs;
    for (auto& namedArg : callNode.named) {
        std::string lowerName = Symbol::toLower(namedArg.name);
        for (auto& seen : seenNamedArgs) {
            if (Symbol::toLower(seen) == lowerName) {
                diag_.error(DiagnosticID::SemDuplicateNamedArg, callNode.loc,
                    "重复的命名参数: '" + namedArg.name + "'");
                break;
            }
        }
        seenNamedArgs.push_back(namedArg.name);
    }
}

void SemanticAnalyzer::markReferenced(const std::string& name) {
    auto* sym = symTab_.lookup(name);
    if (sym) {
        sym->isReferenced = true;
    }
}

std::string SemanticAnalyzer::makeInternalName(const std::string& prefix,
                                                const std::string& name) {
    return prefix + "_" + name;
}

void SemanticAnalyzer::checkUnreferencedSymbols() {
    // 遍历模块级符号, 检查未引用的变量/常量
    auto* modScope = symTab_.moduleScope();
    for (auto& [key, sym] : modScope->symbols()) {
        if (!sym->isReferenced &&
            (sym->kind == SymbolKind::Variable || sym->kind == SymbolKind::Constant)) {
            // 仅在 verbose 模式下警告, VB6默认允许未使用变量
            diag_.warn(DiagnosticID::SemUndeclaredIdentifier, sym->location,
                "未使用的" + std::string(sym->kindName()) + ": '" + sym->name + "'");
        }
    }
}

void SemanticAnalyzer::registerBuiltins() {
    // VB6 内置常量
    auto addConst = [&](const char* name, Vb6Type type, long long intVal) {
        auto sym = std::make_unique<Symbol>(SymbolKind::Constant, name, type,
            SourceLocation{}, AccessLevel::Public);
        sym->hasConstValue = true;
        sym->constType = type;
        sym->constIntValue = intVal;
        sym->isBuiltin = true;
        symTab_.define(std::move(sym));
    };

    // MsgBox 返回值常量
    addConst("vbOK", Vb6Type::Long, 1);
    addConst("vbCancel", Vb6Type::Long, 2);
    addConst("vbAbort", Vb6Type::Long, 3);
    addConst("vbRetry", Vb6Type::Long, 4);
    addConst("vbIgnore", Vb6Type::Long, 5);
    addConst("vbYes", Vb6Type::Long, 6);
    addConst("vbNo", Vb6Type::Long, 7);

    // MsgBox 按钮参数常量
    addConst("vbOKOnly", Vb6Type::Long, 0);
    addConst("vbOKCancel", Vb6Type::Long, 1);
    addConst("vbAbortRetryIgnore", Vb6Type::Long, 2);
    addConst("vbYesNoCancel", Vb6Type::Long, 3);
    addConst("vbYesNo", Vb6Type::Long, 4);
    addConst("vbRetryCancel", Vb6Type::Long, 5);
    addConst("vbCritical", Vb6Type::Long, 16);
    addConst("vbQuestion", Vb6Type::Long, 32);
    addConst("vbExclamation", Vb6Type::Long, 48);
    addConst("vbInformation", Vb6Type::Long, 64);
    addConst("vbDefaultButton1", Vb6Type::Long, 0);
    addConst("vbDefaultButton2", Vb6Type::Long, 256);
    addConst("vbDefaultButton3", Vb6Type::Long, 512);
    addConst("vbDefaultButton4", Vb6Type::Long, 768);
    addConst("vbApplicationModal", Vb6Type::Long, 0);
    addConst("vbSystemModal", Vb6Type::Long, 4096);

    // 颜色常量
    addConst("vbBlack", Vb6Type::Long, 0);
    addConst("vbRed", Vb6Type::Long, 255);
    addConst("vbGreen", Vb6Type::Long, 65280);
    addConst("vbYellow", Vb6Type::Long, 65535);
    addConst("vbBlue", Vb6Type::Long, 16711680);
    addConst("vbMagenta", Vb6Type::Long, 16711935);
    addConst("vbCyan", Vb6Type::Long, 16776960);
    addConst("vbWhite", Vb6Type::Long, 16777215);

    // 字符串常量
    auto addStrConst = [&](const char* name, const char* val) {
        auto sym = std::make_unique<Symbol>(SymbolKind::Constant, name, Vb6Type::String,
            SourceLocation{}, AccessLevel::Public);
        sym->hasConstValue = true;
        sym->constType = Vb6Type::String;
        sym->constStringValue = val;
        sym->isBuiltin = true;
        symTab_.define(std::move(sym));
    };
    addStrConst("vbCr", "\r");
    addStrConst("vbLf", "\n");
    addStrConst("vbCrLf", "\r\n");
    addStrConst("vbNewLine", "\r\n");
    addStrConst("vbNullChar", std::string(1, '\0').c_str());
    addStrConst("vbTab", "\t");
    addStrConst("vbBack", "\b");
    addStrConst("vbFormFeed", "\f");
    addStrConst("vbVerticalTab", "\v");

    // 布尔常量 (虽然词法器已识别 True/False, 但也作为符号注册)
    addConst("vbTrue", Vb6Type::Boolean, -1);
    addConst("vbFalse", Vb6Type::Boolean, 0);

    // 杂项常量
    addConst("vbEmpty", Vb6Type::Long, 0);
    addConst("vbNull", Vb6Type::Long, 1);
    addConst("vbInteger", Vb6Type::Long, 2);
    addConst("vbLong", Vb6Type::Long, 3);
    addConst("vbSingle", Vb6Type::Long, 4);
    addConst("vbDouble", Vb6Type::Long, 5);
    addConst("vbCurrency", Vb6Type::Long, 6);
    addConst("vbDate", Vb6Type::Long, 7);
    addConst("vbString", Vb6Type::Long, 8);
    addConst("vbObject", Vb6Type::Long, 9);
    addConst("vbError", Vb6Type::Long, 10);
    addConst("vbBoolean", Vb6Type::Long, 11);
    addConst("vbVariant", Vb6Type::Long, 12);
    addConst("vbDBNull", Vb6Type::Long, 17);
    addConst("vbByte", Vb6Type::Long, 17);

    // P21-25: Missing VB6 built-in constants

    // Comparison constants
    addConst("vbBinaryCompare", Vb6Type::Long, 0);
    addConst("vbTextCompare", Vb6Type::Long, 1);
    addConst("vbDatabaseCompare", Vb6Type::Long, 2);

    // String conversion constants (StrConv)
    addConst("vbUpperCase", Vb6Type::Long, 1);
    addConst("vbLowerCase", Vb6Type::Long, 2);
    addConst("vbProperCase", Vb6Type::Long, 3);
    addConst("vbWide", Vb6Type::Long, 4);
    addConst("vbNarrow", Vb6Type::Long, 8);
    addConst("vbKatakana", Vb6Type::Long, 16);
    addConst("vbHiragana", Vb6Type::Long, 32);
    addConst("vbUnicode", Vb6Type::Long, 64);
    addConst("vbFromUnicode", Vb6Type::Long, 128);

    // File I/O constants
    addConst("vbNormal", Vb6Type::Long, 0);
    addConst("vbReadOnly", Vb6Type::Long, 1);
    addConst("vbHidden", Vb6Type::Long, 2);
    addConst("vbSystem", Vb6Type::Long, 4);
    addConst("vbVolume", Vb6Type::Long, 8);
    addConst("vbDirectory", Vb6Type::Long, 16);
    addConst("vbArchive", Vb6Type::Long, 32);
    addConst("vbAlias", Vb6Type::Long, 64);

    // File open mode constants
    addConst("vbInput", Vb6Type::Long, 0);
    addConst("vbOutput", Vb6Type::Long, 1);
    addConst("vbRandom", Vb6Type::Long, 4);
    addConst("vbAppend", Vb6Type::Long, 8);
    addConst("vbBinary", Vb6Type::Long, 32);

    // VarType constants (extended)
    addConst("vbDecimal", Vb6Type::Long, 14);
    addConst("vbUserDefinedType", Vb6Type::Long, 36);
    addConst("vbArray", Vb6Type::Long, 8192);
    addConst("vbDataObject", Vb6Type::Long, 13);

    // MsgBox additional constants
    addConst("vbMsgBoxSetForeground", Vb6Type::Long, 65536);
    addConst("vbMsgBoxRight", Vb6Type::Long, 524288);
    addConst("vbMsgBoxRtlReading", Vb6Type::Long, 1048576);
    addConst("vbDefaultButton5", Vb6Type::Long, 1024);

    // Date/time constants
    addConst("vbUseSystemDayOfWeek", Vb6Type::Long, 0);
    addConst("vbSunday", Vb6Type::Long, 1);
    addConst("vbMonday", Vb6Type::Long, 2);
    addConst("vbTuesday", Vb6Type::Long, 3);
    addConst("vbWednesday", Vb6Type::Long, 4);
    addConst("vbThursday", Vb6Type::Long, 5);
    addConst("vbFriday", Vb6Type::Long, 6);
    addConst("vbSaturday", Vb6Type::Long, 7);

    // FirstWeekOfYear constants
    addConst("vbFirstJan1", Vb6Type::Long, 1);
    addConst("vbFirstFourDays", Vb6Type::Long, 2);
    addConst("vbFirstFullWeek", Vb6Type::Long, 3);

    // Calendar constants
    addConst("vbCalGreg", Vb6Type::Long, 0);
    addConst("vbCalHijri", Vb6Type::Long, 1);

    // QueryClose constants (for forms)
    addConst("vbFormControlMenu", Vb6Type::Long, 0);
    addConst("vbFormCode", Vb6Type::Long, 1);
    addConst("vbAppWindows", Vb6Type::Long, 2);
    addConst("vbAppTaskManager", Vb6Type::Long, 3);
    addConst("vbFormMDIForm", Vb6Type::Long, 4);
    addConst("vbFormOwner", Vb6Type::Long, 5);

    // Print method constants (Tab, Spc handled as functions)
    addConst("vbObjectError", Vb6Type::Long, -2147221504);

    // Shift/Ctrl/Alt mask constants (for KeyDown/KeyUp)
    addConst("vbShiftMask", Vb6Type::Long, 1);
    addConst("vbCtrlMask", Vb6Type::Long, 2);
    addConst("vbAltMask", Vb6Type::Long, 4);

    // MouseButton constants
    addConst("vbLeftButton", Vb6Type::Long, 1);
    addConst("vbRightButton", Vb6Type::Long, 2);
    addConst("vbMiddleButton", Vb6Type::Long, 4);

    // Key code constants (commonly used)
    addConst("vbKeyLButton", Vb6Type::Long, 1);
    addConst("vbKeyRButton", Vb6Type::Long, 2);
    addConst("vbKeyCancel", Vb6Type::Long, 3);
    addConst("vbKeyMButton", Vb6Type::Long, 4);
    addConst("vbKeyBack", Vb6Type::Long, 8);
    addConst("vbKeyTab", Vb6Type::Long, 9);
    addConst("vbKeyClear", Vb6Type::Long, 12);
    addConst("vbKeyReturn", Vb6Type::Long, 13);
    addConst("vbKeyShift", Vb6Type::Long, 16);
    addConst("vbKeyControl", Vb6Type::Long, 17);
    addConst("vbKeyMenu", Vb6Type::Long, 18);
    addConst("vbKeyPause", Vb6Type::Long, 19);
    addConst("vbKeyCapital", Vb6Type::Long, 20);
    addConst("vbKeyEscape", Vb6Type::Long, 27);
    addConst("vbKeySpace", Vb6Type::Long, 32);
    addConst("vbKeyPageUp", Vb6Type::Long, 33);
    addConst("vbKeyPageDown", Vb6Type::Long, 34);
    addConst("vbKeyEnd", Vb6Type::Long, 35);
    addConst("vbKeyHome", Vb6Type::Long, 36);
    addConst("vbKeyLeft", Vb6Type::Long, 37);
    addConst("vbKeyUp", Vb6Type::Long, 38);
    addConst("vbKeyRight", Vb6Type::Long, 39);
    addConst("vbKeyDown", Vb6Type::Long, 40);
    addConst("vbKeySelect", Vb6Type::Long, 41);
    addConst("vbKeyPrint", Vb6Type::Long, 42);
    addConst("vbKeyExecute", Vb6Type::Long, 43);
    addConst("vbKeySnapshot", Vb6Type::Long, 44);
    addConst("vbKeyInsert", Vb6Type::Long, 45);
    addConst("vbKeyDelete", Vb6Type::Long, 46);
    addConst("vbKeyHelp", Vb6Type::Long, 47);
    addConst("vbKeyNumlock", Vb6Type::Long, 144);

    // Number key constants
    addConst("vbKey0", Vb6Type::Long, 48);
    addConst("vbKey1", Vb6Type::Long, 49);
    addConst("vbKey2", Vb6Type::Long, 50);
    addConst("vbKey3", Vb6Type::Long, 51);
    addConst("vbKey4", Vb6Type::Long, 52);
    addConst("vbKey5", Vb6Type::Long, 53);
    addConst("vbKey6", Vb6Type::Long, 54);
    addConst("vbKey7", Vb6Type::Long, 55);
    addConst("vbKey8", Vb6Type::Long, 56);
    addConst("vbKey9", Vb6Type::Long, 57);

    // Letter key constants
    addConst("vbKeyA", Vb6Type::Long, 65);
    addConst("vbKeyB", Vb6Type::Long, 66);
    addConst("vbKeyC", Vb6Type::Long, 67);
    addConst("vbKeyD", Vb6Type::Long, 68);
    addConst("vbKeyE", Vb6Type::Long, 69);
    addConst("vbKeyF", Vb6Type::Long, 70);
    addConst("vbKeyG", Vb6Type::Long, 71);
    addConst("vbKeyH", Vb6Type::Long, 72);
    addConst("vbKeyI", Vb6Type::Long, 73);
    addConst("vbKeyJ", Vb6Type::Long, 74);
    addConst("vbKeyK", Vb6Type::Long, 75);
    addConst("vbKeyL", Vb6Type::Long, 76);
    addConst("vbKeyM", Vb6Type::Long, 77);
    addConst("vbKeyN", Vb6Type::Long, 78);
    addConst("vbKeyO", Vb6Type::Long, 79);
    addConst("vbKeyP", Vb6Type::Long, 80);
    addConst("vbKeyQ", Vb6Type::Long, 81);
    addConst("vbKeyR", Vb6Type::Long, 82);
    addConst("vbKeyS", Vb6Type::Long, 83);
    addConst("vbKeyT", Vb6Type::Long, 84);
    addConst("vbKeyU", Vb6Type::Long, 85);
    addConst("vbKeyV", Vb6Type::Long, 86);
    addConst("vbKeyW", Vb6Type::Long, 87);
    addConst("vbKeyX", Vb6Type::Long, 88);
    addConst("vbKeyY", Vb6Type::Long, 89);
    addConst("vbKeyZ", Vb6Type::Long, 90);

    // Numpad key constants
    addConst("vbKeyNumpad0", Vb6Type::Long, 96);
    addConst("vbKeyNumpad1", Vb6Type::Long, 97);
    addConst("vbKeyNumpad2", Vb6Type::Long, 98);
    addConst("vbKeyNumpad3", Vb6Type::Long, 99);
    addConst("vbKeyNumpad4", Vb6Type::Long, 100);
    addConst("vbKeyNumpad5", Vb6Type::Long, 101);
    addConst("vbKeyNumpad6", Vb6Type::Long, 102);
    addConst("vbKeyNumpad7", Vb6Type::Long, 103);
    addConst("vbKeyNumpad8", Vb6Type::Long, 104);
    addConst("vbKeyNumpad9", Vb6Type::Long, 105);
    addConst("vbKeyMultiply", Vb6Type::Long, 106);
    addConst("vbKeyAdd", Vb6Type::Long, 107);
    addConst("vbKeySeparator", Vb6Type::Long, 108);
    addConst("vbKeySubtract", Vb6Type::Long, 109);
    addConst("vbKeyDecimal", Vb6Type::Long, 110);
    addConst("vbKeyDivide", Vb6Type::Long, 111);

    // Function key constants
    addConst("vbKeyF1", Vb6Type::Long, 112);
    addConst("vbKeyF2", Vb6Type::Long, 113);
    addConst("vbKeyF3", Vb6Type::Long, 114);
    addConst("vbKeyF4", Vb6Type::Long, 115);
    addConst("vbKeyF5", Vb6Type::Long, 116);
    addConst("vbKeyF6", Vb6Type::Long, 117);
    addConst("vbKeyF7", Vb6Type::Long, 118);
    addConst("vbKeyF8", Vb6Type::Long, 119);
    addConst("vbKeyF9", Vb6Type::Long, 120);
    addConst("vbKeyF10", Vb6Type::Long, 121);
    addConst("vbKeyF11", Vb6Type::Long, 122);
    addConst("vbKeyF12", Vb6Type::Long, 123);
    addConst("vbKeyF13", Vb6Type::Long, 124);
    addConst("vbKeyF14", Vb6Type::Long, 125);
    addConst("vbKeyF15", Vb6Type::Long, 126);
    addConst("vbKeyF16", Vb6Type::Long, 127);

    // P22: Shell constants (for Shell function window style)
    addConst("vbHide", Vb6Type::Long, 0);
    addConst("vbNormalFocus", Vb6Type::Long, 1);
    addConst("vbMinimizedFocus", Vb6Type::Long, 2);
    addConst("vbMaximizedFocus", Vb6Type::Long, 3);
    addConst("vbNormalNoFocus", Vb6Type::Long, 4);
    addConst("vbMinimizedNoFocus", Vb6Type::Long, 6);
    // P22: Date format constants (for FormatDateTime)
    addConst("vbGeneralDate", Vb6Type::Long, 0);
    addConst("vbLongDate", Vb6Type::Long, 1);
    addConst("vbShortDate", Vb6Type::Long, 2);
    addConst("vbLongTime", Vb6Type::Long, 3);
    addConst("vbShortTime", Vb6Type::Long, 4);
    // P22: System color constants (OLE system colors)
    addConst("vbScrollBars", Vb6Type::Long, -2147483648LL);
    addConst("vbDesktop", Vb6Type::Long, -2147483647LL);
    addConst("vbActiveTitleBar", Vb6Type::Long, -2147483646LL);
    addConst("vbInactiveTitleBar", Vb6Type::Long, -2147483645LL);
    addConst("vbMenu", Vb6Type::Long, -2147483644LL);
    addConst("vbWindowBackground", Vb6Type::Long, -2147483643LL);
    addConst("vbWindowFrame", Vb6Type::Long, -2147483642LL);
    addConst("vbMenuText", Vb6Type::Long, -2147483641LL);
    addConst("vbWindowText", Vb6Type::Long, -2147483640LL);
    addConst("vbTitleBarText", Vb6Type::Long, -2147483639LL);
    addConst("vbActiveBorder", Vb6Type::Long, -2147483638LL);
    addConst("vbInactiveBorder", Vb6Type::Long, -2147483637LL);
    addConst("vbApplicationWorkspace", Vb6Type::Long, -2147483636LL);
    addConst("vbHighlight", Vb6Type::Long, -2147483635LL);
    addConst("vbHighlightText", Vb6Type::Long, -2147483634LL);
    addConst("vbButtonFace", Vb6Type::Long, -2147483633LL);
    addConst("vbButtonShadow", Vb6Type::Long, -2147483632LL);
    addConst("vbGrayText", Vb6Type::Long, -2147483631LL);
    addConst("vbButtonText", Vb6Type::Long, -2147483630LL);
    addConst("vbInactiveCaptionText", Vb6Type::Long, -2147483629LL);
    addConst("vb3DHighlight", Vb6Type::Long, -2147483628LL);
    addConst("vb3DDKShadow", Vb6Type::Long, -2147483627LL);
    addConst("vb3DLight", Vb6Type::Long, -2147483626LL);
    addConst("vb3DFace", Vb6Type::Long, -2147483625LL);
    addConst("vb3DShadow", Vb6Type::Long, -2147483624LL);
    // P22: IME status constants
    addConst("vbIMEModeNoControl", Vb6Type::Long, 0);
    addConst("vbIMEModeOn", Vb6Type::Long, 1);
    addConst("vbIMEModeOff", Vb6Type::Long, 2);
    addConst("vbIMEModeDisable", Vb6Type::Long, 3);
    addConst("vbIMEModeHiragana", Vb6Type::Long, 4);
    addConst("vbIMEModeKatakana", Vb6Type::Long, 5);
    addConst("vbIMEModeKatakanaHalf", Vb6Type::Long, 6);
    addConst("vbIMEModeAlphaFull", Vb6Type::Long, 7);
    addConst("vbIMEModeAlpha", Vb6Type::Long, 8);
    addConst("vbIMEModeHangulFull", Vb6Type::Long, 9);
    addConst("vbIMEModeHangul", Vb6Type::Long, 10);

    // P22-09: Printer constants
    addConst("vbPRORPortrait", Vb6Type::Long, 1);        // Printer orientation: Portrait
    addConst("vbPRORLandscape", Vb6Type::Long, 2);       // Printer orientation: Landscape
    addConst("vbPRPQDraft", Vb6Type::Long, -1);          // Print quality: Draft
    addConst("vbPRPQLow", Vb6Type::Long, -2);            // Print quality: Low
    addConst("vbPRPQMedium", Vb6Type::Long, -3);         // Print quality: Medium
    addConst("vbPRPQHigh", Vb6Type::Long, -4);           // Print quality: High
    addConst("vbPRCMMillimeters", Vb6Type::Long, 1);     // Page scale: Millimeters
    addConst("vbPRCMCentimeters", Vb6Type::Long, 2);     // Page scale: Centimeters
    addConst("vbPRCMInches", Vb6Type::Long, 3);          // Page scale: Inches
    addConst("vbPRCMCharacters", Vb6Type::Long, 4);      // Page scale: Characters
    addConst("vbPRBPSingle", Vb6Type::Long, 1);          // Binary performation: Single
    addConst("vbPRBPSDouble", Vb6Type::Long, 2);         // Binary performation: Double
    addConst("vbPRBPTriple", Vb6Type::Long, 3);          // Binary performation: Triple
    addConst("vbPRDPHorizontal", Vb6Type::Long, 1);      // Duplex: Horizontal
    addConst("vbPRDPVertical", Vb6Type::Long, 2);        // Duplex: Vertical

    // P22-09: Other missing constants
    addConst("vbUseSystem", Vb6Type::Long, -1);           // Use system setting
    addConst("vbUseCompareOption", Vb6Type::Long, -1);   // Use Option Compare setting
    addConst("Win16", Vb6Type::Long, 0);                  // Obsolete: always False
    addConst("Win32", Vb6Type::Long, -1);                 // True on 32-bit Windows
    addConst("vbDot", Vb6Type::Long, 46);                 // "." character code


    // P22-09: MsgBox additional constants
    addConst("vbMsgBoxHelpButton", Vb6Type::Long, 16384); // &H4000


    // --- P23-04: Low-frequency constants (85 additions) ---

    // VbCallType (CallType)
    addConst("vbMethod", Vb6Type::Long, 2);
    addConst("vbGet", Vb6Type::Long, 3);
    addConst("vbLet", Vb6Type::Long, 1);
    addConst("vbSet", Vb6Type::Long, 4);

    // VbCalendar

    // VbQueryClose

    // Clipboard format constants
    addConst("vbCFText", Vb6Type::Long, 1);
    addConst("vbCFBitmap", Vb6Type::Long, 2);
    addConst("vbCFMetafile", Vb6Type::Long, 3);
    addConst("vbCFDIB", Vb6Type::Long, 8);
    addConst("vbCFPalette", Vb6Type::Long, 9);
    addConst("vbCFRTF", Vb6Type::Long, -16639);       // &HFFFFBF01
    addConst("vbCFEMetafile", Vb6Type::Long, 14);

    // DriveType constants
    addConst("vbDriveTypeRemovable", Vb6Type::Long, 1);
    addConst("vbDriveTypeFixed", Vb6Type::Long, 2);
    addConst("vbDriveTypeNetwork", Vb6Type::Long, 3);
    addConst("vbDriveTypeCDRom", Vb6Type::Long, 4);
    addConst("vbDriveTypeRAMDisk", Vb6Type::Long, 5);

    // VbAppWinStyle
    addConst("vbAppWinStyleNormal", Vb6Type::Long, 1);
    addConst("vbAppWinStyleMinimize", Vb6Type::Long, 2);
    addConst("vbAppWinStyleMaximize", Vb6Type::Long, 3);

    // VbIMEStatus (distinct from IMEMode)
    addConst("vbIMEOn", Vb6Type::Long, 1);
    addConst("vbIMEOff", Vb6Type::Long, 0);
    addConst("vbIMEDisable", Vb6Type::Long, 2);
    addConst("vbIMEHiragana", Vb6Type::Long, 4);
    addConst("vbIMEKatakana", Vb6Type::Long, 5);
    addConst("vbIMEKatakanaHalf", Vb6Type::Long, 6);
    addConst("vbIMEAlphaFull", Vb6Type::Long, 7);
    addConst("vbIMEAlpha", Vb6Type::Long, 8);

    // Shape control constants
    addConst("vbShapeRectangle", Vb6Type::Long, 0);
    addConst("vbShapeSquare", Vb6Type::Long, 1);
    addConst("vbShapeOval", Vb6Type::Long, 2);
    addConst("vbShapeCircle", Vb6Type::Long, 3);
    addConst("vbShapeRoundedRectangle", Vb6Type::Long, 4);
    addConst("vbShapeRoundedSquare", Vb6Type::Long, 5);

    // BorderStyle (Shape/Line)
    addConst("vbTransparent", Vb6Type::Long, 0);
    addConst("vbBSSolid", Vb6Type::Long, 1);
    addConst("vbBSDash", Vb6Type::Long, 2);
    addConst("vbBSDot", Vb6Type::Long, 3);
    addConst("vbBSDashDot", Vb6Type::Long, 4);
    addConst("vbBSDashDotDot", Vb6Type::Long, 5);
    addConst("vbBSInsideSolid", Vb6Type::Long, 6);

    // FillStyle constants
    addConst("vbFSSolid", Vb6Type::Long, 0);
    addConst("vbFSTransparent", Vb6Type::Long, 1);
    addConst("vbFSHorizontalLine", Vb6Type::Long, 2);
    addConst("vbFSVerticalLine", Vb6Type::Long, 3);
    addConst("vbFSUpwardDiagonal", Vb6Type::Long, 4);
    addConst("vbFSDownwardDiagonal", Vb6Type::Long, 5);
    addConst("vbFSCross", Vb6Type::Long, 6);
    addConst("vbFSDiagonalCross", Vb6Type::Long, 7);

    // MousePointer constants
    addConst("vbDefault", Vb6Type::Long, 0);
    addConst("vbArrow", Vb6Type::Long, 1);
    addConst("vbCrosshair", Vb6Type::Long, 2);
    addConst("vbIbeam", Vb6Type::Long, 3);
    addConst("vbIconPointer", Vb6Type::Long, 4);
    addConst("vbSizePointer", Vb6Type::Long, 5);
    addConst("vbSizeNESW", Vb6Type::Long, 6);
    addConst("vbSizeNS", Vb6Type::Long, 7);
    addConst("vbSizeNWSE", Vb6Type::Long, 8);
    addConst("vbSizeEW", Vb6Type::Long, 9);
    addConst("vbUpArrow", Vb6Type::Long, 10);
    addConst("vbHourglass", Vb6Type::Long, 11);
    addConst("vbNoDrop", Vb6Type::Long, 12);
    addConst("vbArrowHourglass", Vb6Type::Long, 13);
    addConst("vbArrowQuestion", Vb6Type::Long, 14);
    addConst("vbSizeAll", Vb6Type::Long, 15);
    addConst("vbCustom", Vb6Type::Long, 99);

    // Alignment constants
    addConst("vbLeftJustify", Vb6Type::Long, 0);
    addConst("vbRightJustify", Vb6Type::Long, 1);
    addConst("vbCenter", Vb6Type::Long, 2);

    // ScrollBar constants
    addConst("vbSBNone", Vb6Type::Long, 0);
    addConst("vbSBHorizontal", Vb6Type::Long, 1);
    addConst("vbSBVertical", Vb6Type::Long, 2);
    addConst("vbSBBoth", Vb6Type::Long, 3);

    // ScaleMode constants
    addConst("vbTwips", Vb6Type::Long, 1);
    addConst("vbPoints", Vb6Type::Long, 2);
    addConst("vbPixels", Vb6Type::Long, 3);
    addConst("vbCharacters", Vb6Type::Long, 4);
    addConst("vbInches", Vb6Type::Long, 5);
    addConst("vbMillimeters", Vb6Type::Long, 6);
    addConst("vbCentimeters", Vb6Type::Long, 7);
    addConst("vbHimetric", Vb6Type::Long, 8);
    addConst("vbContainerPosition", Vb6Type::Long, 9);
    addConst("vbContainerSize", Vb6Type::Long, 10);
    addConst("vbUser", Vb6Type::Long, 0);

    // WindowState constants
    addConst("vbMinimized", Vb6Type::Long, 1);
    addConst("vbMaximized", Vb6Type::Long, 2);

    // Fix 056: CheckBox constants
    addConst("vbUnchecked", Vb6Type::Long, 0);
    addConst("vbChecked", Vb6Type::Long, 1);
    addConst("vbGrayed", Vb6Type::Long, 2);

    // String constants (additional)
    addStrConst("vbNullString", "");

    // 内置对象: Debug, Err, Screen, App, Printer
    auto addObj = [&](const char* name) {
        auto sym = std::make_unique<Symbol>(SymbolKind::Variable, name, Vb6Type::Object,
            SourceLocation{}, AccessLevel::Public);
        sym->isBuiltin = true;
        symTab_.define(std::move(sym));
    };
    addObj("Debug");
    addObj("Err");
    addObj("Screen");
    addObj("App");
    addObj("Printer");
    addObj("Forms");
    addObj("Clipboard");

    // 内置函数 (声明为 Function 符号, 避免未声明错误)
    auto addBuiltinFunc = [&](const char* name, Vb6Type retType) {
        auto sym = std::make_unique<Symbol>(SymbolKind::Function, name, retType,
            SourceLocation{}, AccessLevel::Public);
        sym->isBuiltin = true;
        symTab_.define(std::move(sym));
    };
    // Fix 030b: 带参数签名的内置函数注册 — 填充 sym->params, 让 IndexOrCallExpr 的
    // calleeParams 查找命中, 从而触发 Fix 024 P2 (ByVal Variant 正向包装) 和
    // Fix 029 (ByVal 具体类型反向提取). params: vector of (name, type, isByVal, isOptional).
    auto addBuiltinFuncWithParams = [&](const char* name, Vb6Type retType,
                                        std::initializer_list<std::tuple<const char*, Vb6Type, bool, bool>> params) {
        auto sym = std::make_unique<Symbol>(SymbolKind::Function, name, retType,
            SourceLocation{}, AccessLevel::Public);
        sym->isBuiltin = true;
        for (auto& p : params) {
            ParameterInfo pi;
            pi.name = std::get<0>(p);
            pi.type = std::get<1>(p);
            pi.isByVal = std::get<2>(p);
            pi.isOptional = std::get<3>(p);
            sym->params.push_back(std::move(pi));
        }
        symTab_.define(std::move(sym));
    };
    // Fix 034: Variant|Array 复合类型 (Variant 含数组), 供多个内置函数的数组参数
    // 声明使用 — 反向提取时 paramIsArray 分支会触发 vb6_VariantToSafeArray1D.
    const Vb6Type kVariantArray = static_cast<Vb6Type>(
        static_cast<uint16_t>(Vb6Type::Variant) | static_cast<uint16_t>(Vb6Type::Array));
    // Fix 034: 字符串/转换/数值/类型检查/数学/文件/日期等内置函数补全参数签名 —
    // 让 IndexOrCallExpr 的 calleeParams 查找命中, 触发 Fix 024 P2 (ByVal Variant 正向包装) 和
    // Fix 029 (ByVal 具体类型反向提取), 消除大量 C2440 (函数实参 → VARIANT/BSTR/double 等).
    // RTL C 签名参考 src/rtl/core/vb6rtl.h. VB6 Optional 参数标记 isOptional=true (语义保留),
    // 但 RTL C 函数不接受 Optional padding 和 IsMissing _has_ 尾叜 — cgen 用硬编码补默认值
    // (见 cgen_expr.cpp line 3140+ 和 line 3263+).
    // 字符串函数 (RTL 签名: vb6_Len/Left/Right/Mid/InStr/InStrRev 等均接受 BSTR)
    addBuiltinFuncWithParams("Len", Vb6Type::Long,
        {{"s", Vb6Type::String, true, false}});
    addBuiltinFuncWithParams("Left", Vb6Type::String,
        {{"s", Vb6Type::String, true, false}, {"n", Vb6Type::Long, true, false}});
    addBuiltinFuncWithParams("Right", Vb6Type::String,
        {{"s", Vb6Type::String, true, false}, {"n", Vb6Type::Long, true, false}});
    addBuiltinFuncWithParams("Mid", Vb6Type::String,
        {{"s", Vb6Type::String, true, false}, {"start", Vb6Type::Long, true, false},
         {"len", Vb6Type::Long, true, true}});
    // InStr: VB6 双形态 InStr(s1, s2) 和 InStr(start, s1, s2). cgen 用 argList="1, "+argList
    // 把 2-arg 形状调整为 3-arg (start=1). 为避免 2-arg 形态下 arg[0]=s1 被 calleeParams[0]
    // (start:Long) 错误包装 (Variant→vb6_VariantToLong 而非 vb6_VariantToString),
    // 不声明 calleeParams — 让 cgen 硬编码重排实参位置后直接调用.
    addBuiltinFunc("InStr", Vb6Type::Long);
    addBuiltinFuncWithParams("InStrRev", Vb6Type::Long,
        {{"string1", Vb6Type::String, true, false}, {"string2", Vb6Type::String, true, false},
         {"start", Vb6Type::Long, true, true}, {"compare", Vb6Type::Long, true, true}});
    addBuiltinFuncWithParams("LTrim", Vb6Type::String, {{"s", Vb6Type::String, true, false}});
    addBuiltinFuncWithParams("RTrim", Vb6Type::String, {{"s", Vb6Type::String, true, false}});
    addBuiltinFuncWithParams("Trim", Vb6Type::String, {{"s", Vb6Type::String, true, false}});
    addBuiltinFuncWithParams("LCase", Vb6Type::String, {{"s", Vb6Type::String, true, false}});
    addBuiltinFuncWithParams("UCase", Vb6Type::String, {{"s", Vb6Type::String, true, false}});
    addBuiltinFuncWithParams("Replace", Vb6Type::String,
        {{"expr", Vb6Type::String, true, false}, {"find", Vb6Type::String, true, false},
         {"rep", Vb6Type::String, true, false}, {"start", Vb6Type::Long, true, true},
         {"count", Vb6Type::Long, true, true}, {"compare", Vb6Type::Long, true, true}});
    addBuiltinFuncWithParams("Split", Vb6Type::Variant,
        {{"expr", Vb6Type::String, true, false}, {"delimiter", Vb6Type::String, true, true},
         {"limit", Vb6Type::Long, true, true}, {"compare", Vb6Type::Long, true, true}});
    addBuiltinFuncWithParams("Join", Vb6Type::String,
        {{"arr", kVariantArray, true, false}, {"delimiter", Vb6Type::String, true, true}});
    addBuiltinFuncWithParams("StrComp", Vb6Type::Long,
        {{"s1", Vb6Type::String, true, false}, {"s2", Vb6Type::String, true, false},
         {"compare", Vb6Type::Long, true, true}});
    addBuiltinFuncWithParams("StrReverse", Vb6Type::String, {{"s", Vb6Type::String, true, false}});
    addBuiltinFuncWithParams("Space", Vb6Type::String, {{"n", Vb6Type::Long, true, false}});
    // String(n, charCode) — charCode 在 VB6 可为 String 或 Integer; RTL 取 int32_t.
    addBuiltinFuncWithParams("String", Vb6Type::String,
        {{"n", Vb6Type::Long, true, false}, {"charCode", Vb6Type::Long, true, false}});
    addBuiltinFuncWithParams("Asc", Vb6Type::Long, {{"s", Vb6Type::String, true, false}});
    addBuiltinFuncWithParams("Chr", Vb6Type::String, {{"code", Vb6Type::Long, true, false}});
    addBuiltinFuncWithParams("Val", Vb6Type::Double, {{"s", Vb6Type::String, true, false}});
    addBuiltinFuncWithParams("Str", Vb6Type::String, {{"n", Vb6Type::Long, true, false}});
    // Format: 保留 addBuiltinFunc (无 calleeParams) — cgen_expr.cpp:3487 会按 inferExprType
    // 把 args[0] 包装成 vb6_VariantLong/Double/String/Int. 若此处加 ByVal Variant calleeParams,
    // Fix 024 P2 会先用 vb6_VariantFromValue 包装(返回 VARIANT), 然后 Format 特殊包装
    // vb6_VariantLong(VARIANT) 传 int32_t 参数 → C2440. 双重包装冲突, 故不注册参数.
    addBuiltinFunc("Format", Vb6Type::String);
    // 类型转换函数 — cgen 特殊处理决定参数策略.
    // CStr/CInt/CLng/CDbl: cgen_expr.cpp:3383/3428 有 callee-rewrite 特殊分支
    //   - CStr: 根据 arg 类型改写 callee 为 vb6_CStrLong/Dbl/Bool/Date (具体类型参数)
    //   - CInt/CLng/CDbl: 已知 Variant 变量改写 callee 为 vb6_CIntV/CLngV/CDblV (VARIANT 参数)
    // 若此处加 calleeParams, Fix 024 P2/Fix 029 会先 wrapping, 与 callee-rewrite 冲突 → C2440.
    // 故保持 addBuiltinFunc (无 params), 让特殊分支独立工作.
    addBuiltinFunc("CStr", Vb6Type::String);
    addBuiltinFunc("CInt", Vb6Type::Integer);
    addBuiltinFunc("CLng", Vb6Type::Long);
    addBuiltinFuncWithParams("CSng", Vb6Type::Single, {{"v", Vb6Type::Double, true, false}});
    addBuiltinFunc("CDbl", Vb6Type::Double);
    addBuiltinFuncWithParams("CBool", Vb6Type::Boolean, {{"v", Vb6Type::Double, true, false}});
    addBuiltinFuncWithParams("CDate", Vb6Type::Date, {{"v", Vb6Type::Variant, true, false}});
    addBuiltinFuncWithParams("CByte", Vb6Type::Byte, {{"v", Vb6Type::Double, true, false}});
    addBuiltinFuncWithParams("CCur", Vb6Type::Currency, {{"v", Vb6Type::Double, true, false}});
    addBuiltinFuncWithParams("CDec", Vb6Type::Variant, {{"v", Vb6Type::Variant, true, false}});
    addBuiltinFunc("CVar", Vb6Type::Variant);
    addBuiltinFuncWithParams("Hex", Vb6Type::String, {{"n", Vb6Type::Long, true, false}});
    addBuiltinFuncWithParams("Oct", Vb6Type::String, {{"n", Vb6Type::Long, true, false}});
    addBuiltinFuncWithParams("CVErr", Vb6Type::Variant, {{"errorNumber", Vb6Type::Long, true, false}});
    // 数值函数 — RTL C 签名: 所有数值函数接受 double.
    addBuiltinFuncWithParams("Abs", Vb6Type::Double, {{"x", Vb6Type::Double, true, false}});
    addBuiltinFuncWithParams("Int", Vb6Type::Double, {{"x", Vb6Type::Double, true, false}});
    addBuiltinFuncWithParams("Fix", Vb6Type::Double, {{"x", Vb6Type::Double, true, false}});
    // Fix 029: Sgn 返回 Long 但接受 double. Variant 实参 → vb6_VariantToDouble 提取.
    addBuiltinFuncWithParams("Sgn", Vb6Type::Long, {{"x", Vb6Type::Double, true, false}});
    addBuiltinFuncWithParams("Sqr", Vb6Type::Double, {{"x", Vb6Type::Double, true, false}});
    addBuiltinFuncWithParams("Round", Vb6Type::Double,
        {{"x", Vb6Type::Double, true, false}, {"decimals", Vb6Type::Long, true, true}});
    // Rnd/Randomize: Optional 参数. 不补 calleeParams — 现有 cgen 不补默认值会 C2198,
    // 但补了 calleeParams 又会被 !calleeIsBuiltin guard 跳过 padding. Fix 034 额外在
    // cgen_expr.cpp 中为这两个加硬编码 padding (无 calleeParams, 走老路径).
    addBuiltinFunc("Rnd", Vb6Type::Single);
    // 转换与类型检查 — RTL: IsXxx 接受 vb6_VARIANT by-value.
    addBuiltinFuncWithParams("IsNumeric", Vb6Type::Boolean, {{"v", Vb6Type::Variant, true, false}});
    addBuiltinFuncWithParams("IsDate", Vb6Type::Boolean, {{"v", Vb6Type::Variant, true, false}});
    addBuiltinFuncWithParams("IsEmpty", Vb6Type::Boolean, {{"v", Vb6Type::Variant, true, false}});
    addBuiltinFuncWithParams("IsNull", Vb6Type::Boolean, {{"v", Vb6Type::Variant, true, false}});
    addBuiltinFuncWithParams("IsObject", Vb6Type::Boolean, {{"v", Vb6Type::Variant, true, false}});
    addBuiltinFuncWithParams("IsArray", Vb6Type::Boolean, {{"v", Vb6Type::Variant, true, false}});
    addBuiltinFunc("IsNothing", Vb6Type::Boolean);
    addBuiltinFuncWithParams("IsError", Vb6Type::Boolean, {{"v", Vb6Type::Variant, true, false}});
    addBuiltinFuncWithParams("TypeName", Vb6Type::String, {{"v", Vb6Type::Variant, true, false}});
    addBuiltinFuncWithParams("VarType", Vb6Type::Long, {{"v", Vb6Type::Variant, true, false}});
    // 数组
    // Fix 030b: UBound/LBound 注册带参数签名 — 让 Fix 029 反向提取能在 Variant 实参上
    // 触发 vb6_VariantToSafeArray1D, 修正 C2440 (vb6_VARIANT → vb6_SafeArray1D*) 子类.
    // RTL: int32_t vb6_UBound(vb6_SafeArray1D* safeArray, int32_t dimension);
    // VB6: UBound(arrayname[, dimension]) — arrayname 接受 Variant 含数组或类型化数组.
    // 第 1 参数用 Variant|Array (不纯 Variant, 让反向提取 paramIsArray 分支命中).
    addBuiltinFuncWithParams("UBound", Vb6Type::Long,
        {{"array", kVariantArray, true, false}, {"dimension", Vb6Type::Long, true, true}});
    addBuiltinFuncWithParams("LBound", Vb6Type::Long,
        {{"array", kVariantArray, true, false}, {"dimension", Vb6Type::Long, true, true}});
    addBuiltinFunc("Array", Vb6Type::Variant);
    // 数学 — RTL: sin/cos/tan/atan/exp/log 均接受 double.
    addBuiltinFuncWithParams("Sin", Vb6Type::Double, {{"x", Vb6Type::Double, true, false}});
    addBuiltinFuncWithParams("Cos", Vb6Type::Double, {{"x", Vb6Type::Double, true, false}});
    addBuiltinFuncWithParams("Tan", Vb6Type::Double, {{"x", Vb6Type::Double, true, false}});
    addBuiltinFuncWithParams("Atn", Vb6Type::Double, {{"x", Vb6Type::Double, true, false}});
    addBuiltinFuncWithParams("Exp", Vb6Type::Double, {{"x", Vb6Type::Double, true, false}});
    addBuiltinFuncWithParams("Log", Vb6Type::Double, {{"x", Vb6Type::Double, true, false}});
    // 文件 — RTL: LOF/EOF/Loc/Seek 接受 int32_t filenumber; GetAttr/FileLen/FileDateTime/Environ 接受 BSTR.
    addBuiltinFunc("FreeFile", Vb6Type::Long);
    addBuiltinFuncWithParams("LOF", Vb6Type::Long, {{"filenumber", Vb6Type::Long, true, false}});
    addBuiltinFuncWithParams("EOF", Vb6Type::Boolean, {{"filenumber", Vb6Type::Long, true, false}});
    addBuiltinFuncWithParams("Loc", Vb6Type::Long, {{"filenumber", Vb6Type::Long, true, false}});
    // GetAttr 在 vbcrtl.h 中声明为接受 BSTR pathname.
    addBuiltinFuncWithParams("GetAttr", Vb6Type::Long, {{"pathname", Vb6Type::String, true, false}});
    addBuiltinFunc("SetAttr", Vb6Type::Void);
    addBuiltinFuncWithParams("Seek", Vb6Type::Long, {{"filenumber", Vb6Type::Long, true, false}});
    addBuiltinFuncWithParams("FileLen", Vb6Type::Long, {{"pathname", Vb6Type::String, true, false}});
    // FileAttr(filenumber, attribute) — 2 个 ByRefLong 参数
    addBuiltinFuncWithParams("FileAttr", Vb6Type::Long,
        {{"filenumber", Vb6Type::Long, true, false}, {"attribute", Vb6Type::Long, true, false}});
    addBuiltinFuncWithParams("FileDateTime", Vb6Type::Date, {{"pathname", Vb6Type::String, true, false}});
    addBuiltinFuncWithParams("Dir", Vb6Type::String,
        {{"pathname", Vb6Type::String, true, false}, {"attributes", Vb6Type::Long, true, true}});
    addBuiltinFuncWithParams("CurDir", Vb6Type::String, {{"drive", Vb6Type::String, true, true}});
    addBuiltinFuncWithParams("Shell", Vb6Type::Long,
        {{"pathname", Vb6Type::String, true, false}, {"windowstyle", Vb6Type::Long, true, true}});
    addBuiltinFuncWithParams("Environ", Vb6Type::String, {{"envstring", Vb6Type::String, true, false}});
    addBuiltinFunc("Command", Vb6Type::String);
    // 日期时间 — RTL: Year/Month/Day/Hour/Minute/Second 接受 double (Date),
    // DateAdd(interval, number, date), DateDiff(interval, d1, d2, ...), DateSerial(y,m,d),
    // DateValue(BSTR), TimeSerial(h,m,s), TimeValue(BSTR), Weekday(date[, firstDayOfWeek]).
    addBuiltinFunc("Now", Vb6Type::Date);
    addBuiltinFunc("Date", Vb6Type::Date);
    addBuiltinFunc("Time", Vb6Type::Date);
    addBuiltinFuncWithParams("DateAdd", Vb6Type::Date,
        {{"interval", Vb6Type::String, true, false}, {"number", Vb6Type::Double, true, false},
         {"date", Vb6Type::Double, true, false}});
    addBuiltinFuncWithParams("DateDiff", Vb6Type::Long,
        {{"interval", Vb6Type::String, true, false}, {"date1", Vb6Type::Double, true, false},
         {"date2", Vb6Type::Double, true, false}, {"firstDayOfWeek", Vb6Type::Long, true, true},
         {"firstWeekOfYear", Vb6Type::Long, true, true}});
    addBuiltinFuncWithParams("DatePart", Vb6Type::Long,
        {{"interval", Vb6Type::String, true, false}, {"date", Vb6Type::Double, true, false},
         {"firstDayOfWeek", Vb6Type::Long, true, true}, {"firstWeekOfYear", Vb6Type::Long, true, true}});
    addBuiltinFuncWithParams("DateSerial", Vb6Type::Double,
        {{"year", Vb6Type::Long, true, false}, {"month", Vb6Type::Long, true, false},
         {"day", Vb6Type::Long, true, false}});
    addBuiltinFuncWithParams("DateValue", Vb6Type::Date, {{"dateStr", Vb6Type::String, true, false}});
    addBuiltinFuncWithParams("TimeSerial", Vb6Type::Date,
        {{"hour", Vb6Type::Long, true, false}, {"minute", Vb6Type::Long, true, false},
         {"second", Vb6Type::Long, true, false}});
    addBuiltinFuncWithParams("TimeValue", Vb6Type::Date, {{"timeStr", Vb6Type::String, true, false}});
    addBuiltinFuncWithParams("Year", Vb6Type::Long, {{"date", Vb6Type::Double, true, false}});
    addBuiltinFuncWithParams("Month", Vb6Type::Long, {{"date", Vb6Type::Double, true, false}});
    addBuiltinFuncWithParams("Day", Vb6Type::Long, {{"date", Vb6Type::Double, true, false}});
    addBuiltinFuncWithParams("Hour", Vb6Type::Long, {{"time", Vb6Type::Double, true, false}});
    addBuiltinFuncWithParams("Minute", Vb6Type::Long, {{"time", Vb6Type::Double, true, false}});
    addBuiltinFuncWithParams("Second", Vb6Type::Long, {{"time", Vb6Type::Double, true, false}});
    addBuiltinFuncWithParams("Weekday", Vb6Type::Long,
        {{"date", Vb6Type::Double, true, false}, {"firstDayOfWeek", Vb6Type::Long, true, true}});

    // 交互
    addBuiltinFunc("MsgBox", Vb6Type::Long);
    addBuiltinFunc("InputBox", Vb6Type::String);
    // RTL: vb6_RGB(int32_t r, int32_t g, int32_t b); vb6_QBColor(int32_t n).
    addBuiltinFuncWithParams("RGB", Vb6Type::Long,
        {{"r", Vb6Type::Long, true, false}, {"g", Vb6Type::Long, true, false},
         {"b", Vb6Type::Long, true, false}});
    addBuiltinFuncWithParams("QBColor", Vb6Type::Long, {{"n", Vb6Type::Long, true, false}});
    // 杂项
    addBuiltinFunc("DoEvents", Vb6Type::Long);
    addBuiltinFunc("Erl", Vb6Type::Long);
    // Tab/Spc 用于 Print 语句, RTL 接受 int32_t.
    addBuiltinFuncWithParams("Tab", Vb6Type::String, {{"column", Vb6Type::Long, true, false}});
    addBuiltinFuncWithParams("Spc", Vb6Type::String, {{"count", Vb6Type::Long, true, false}});
    // RTL: vb6_CreateObject(const wchar_t* progId); vb6_GetObject(pathName, progId).
    addBuiltinFuncWithParams("CreateObject", Vb6Type::Object,
        {{"progId", Vb6Type::String, true, false}});
    addBuiltinFuncWithParams("GetObject", Vb6Type::Object,
        {{"pathName", Vb6Type::String, true, true}, {"progId", Vb6Type::String, true, true}});
    // LoadPicture(pathname) 返回 void* (Object). RTL: vb6_LoadPictureEx(BSTR).
    addBuiltinFuncWithParams("LoadPicture", Vb6Type::Object, {{"pathname", Vb6Type::String, true, false}});
    addBuiltinFunc("SavePicture", Vb6Type::Void);
    addBuiltinFunc("SaveSetting", Vb6Type::Void);
    addBuiltinFunc("GetSetting", Vb6Type::String);
    addBuiltinFunc("DeleteSetting", Vb6Type::Void);
    addBuiltinFunc("GetAllSettings", Vb6Type::Variant);
    addBuiltinFunc("Load", Vb6Type::Void);
    addBuiltinFunc("Unload", Vb6Type::Void);
    addBuiltinFunc("SendKeys", Vb6Type::Void);
    addBuiltinFunc("AppActivate", Vb6Type::Void);
    addBuiltinFunc("IsMissing", Vb6Type::Boolean);
    addBuiltinFunc("IIf", Vb6Type::Variant);
    addBuiltinFunc("Beep", Vb6Type::Void);
// P14.3.5: CallByName(obj, procName$, callType, [args...])
addBuiltinFunc("CallByName", Vb6Type::Variant);
    // P18-D: 兼容性填平新增内置函数 — RTL: AscW/AscB 接受 BSTR, ChrW/ChrB 接受 int32_t,
    // StrConv(text, conversion, localeID) 全 3 参.
    addBuiltinFuncWithParams("AscW", Vb6Type::Long, {{"s", Vb6Type::String, true, false}});
    addBuiltinFuncWithParams("ChrW", Vb6Type::String, {{"code", Vb6Type::Long, true, false}});
    addBuiltinFuncWithParams("AscB", Vb6Type::Long, {{"s", Vb6Type::String, true, false}});
    addBuiltinFuncWithParams("ChrB", Vb6Type::String, {{"code", Vb6Type::Long, true, false}});
    addBuiltinFunc("Timer", Vb6Type::Single);
    addBuiltinFuncWithParams("StrConv", Vb6Type::String,
        {{"text", Vb6Type::String, true, false}, {"conversion", Vb6Type::Long, true, false},
         {"localeID", Vb6Type::Long, true, true}});
    addBuiltinFunc("Filter", Vb6Type::Variant);
    addBuiltinFunc("VarPtr", Vb6Type::Long);
    addBuiltinFunc("StrPtr", Vb6Type::Long);
    addBuiltinFunc("ObjPtr", Vb6Type::Long);
    addBuiltinFunc("LSet", Vb6Type::String);
    addBuiltinFunc("RSet", Vb6Type::String);
    addBuiltinFunc("WeekdayName", Vb6Type::String);
    addBuiltinFunc("MonthName", Vb6Type::String);
    addBuiltinFunc("FormatCurrency", Vb6Type::String);
    addBuiltinFunc("FormatNumber", Vb6Type::String);
    addBuiltinFunc("FormatPercent", Vb6Type::String);
    addBuiltinFunc("FormatDateTime", Vb6Type::String);
    // P18-E: Financial / Choose / Switch / Lock / Reset / Partition
    addBuiltinFunc("Choose", Vb6Type::Variant);
    addBuiltinFunc("Switch", Vb6Type::Variant);
    addBuiltinFunc("SLN", Vb6Type::Double);
    addBuiltinFunc("SYD", Vb6Type::Double);
    addBuiltinFunc("DDB", Vb6Type::Double);
    addBuiltinFunc("FV", Vb6Type::Double);
    addBuiltinFunc("PV", Vb6Type::Double);
    addBuiltinFunc("Pmt", Vb6Type::Double);
    addBuiltinFunc("IPmt", Vb6Type::Double);
    addBuiltinFunc("PPmt", Vb6Type::Double);
    addBuiltinFunc("RATE", Vb6Type::Double);
    addBuiltinFunc("NPer", Vb6Type::Double);
    addBuiltinFunc("IRR", Vb6Type::Double);
    addBuiltinFunc("MIRR", Vb6Type::Double);
    addBuiltinFunc("NPV", Vb6Type::Double);
    addBuiltinFunc("Partition", Vb6Type::String);
}

// ============================================================
// P14.1.4: Optional参数默认值求值
// ============================================================
std::string SemanticAnalyzer::evalOptionalDefault(ASTNode* defaultValue, Vb6Type paramType) {
    // 无显式默认值 -> 返回空字符串, cgen将使用类型零值
    if (!defaultValue) return "";
    
    // 将AST字面量表达式转换为C表达式字符串
    auto* lit = dynamic_cast<LiteralExpr*>(defaultValue);
    if (lit) {
        switch (lit->literalKind) {
            case LiteralKind::Integer:
            case LiteralKind::Long:
                return lit->rawText;  // "10", "-1" 等
            case LiteralKind::Single:
            case LiteralKind::Double:
                return lit->rawText;  // "3.14" 等
            case LiteralKind::String:
                // VB6 "hello" -> C vb6_BSTR_FromStr(L"hello")
                {
                    std::string sInner = lit->rawText;
                    if (sInner.size() >= 2 && sInner.front() == '"' && sInner.back() == '"')
                        sInner = sInner.substr(1, sInner.size() - 2);
                    return "vb6_BSTR_FromStr(L\"" + sInner + "\")";
                }
            case LiteralKind::Boolean:
                // VB6 True = -1, False = 0
                return (lit->rawText == "True" || lit->rawText == "-1") ? "-1" : "0";
            case LiteralKind::Nothing:
                return "NULL";
            case LiteralKind::Empty:
                return "vb6_VariantEmpty()";
            case LiteralKind::Null:
                return "vb6_VariantNull()";
            default:
                break;
        }
    }
    
    // UnaryExpr: 递归求值操作数, 加前缀 (P20-20)
    auto* unary = dynamic_cast<UnaryExpr*>(defaultValue);
    if (unary) {
        std::string inner = evalOptionalDefault(unary->operand.get(), paramType);
        if (inner.empty()) return "";
        switch (unary->op) {
            case UnaryOp::Negate:
                // 数值型: "(-1)" "(-3.14)"
                if (inner.find("vb6_") == 0) return "";  // 非数值C表达式, 暂不处理
                return "(-" + inner + ")";
            case UnaryOp::Not:
                // Not表达式: 暂不常见做默认值, 返回空
                return "";
        }
    }

    // IdentifierExpr: 解析VB6内建常量 (P20-20)
    auto* ident = dynamic_cast<IdentifierExpr*>(defaultValue);
    if (ident) {
        const std::string& n = ident->name;
        // 转小写比较
        std::string nLower = n;
        for (auto& c : nLower) c = (char)tolower((unsigned char)c);

        // 字符串常量
        if (nLower == "vbcrlf" || nLower == "vbnewline")
            return "vb6_BSTR_FromStr(L\"\\r\\n\")";
        if (nLower == "vbcr")
            return "vb6_BSTR_FromStr(L\"\\r\")";
        if (nLower == "vblf")
            return "vb6_BSTR_FromStr(L\"\\n\")";
        if (nLower == "vbtab")
            return "vb6_BSTR_FromStr(L\"\\t\")";
        if (nLower == "vbnullstring")
            return "vb6_BSTR_FromStr(L\"\")";
        if (nLower == "vbback")
            return "vb6_BSTR_FromStr(L\"\\b\")";
        if (nLower == "vbformfeed")
            return "vb6_BSTR_FromStr(L\"\\f\")";
        if (nLower == "vbverticaltab")
            return "vb6_BSTR_FromStr(L\"\\v\")";

        // 数值/枚举常量
        if (nLower == "vbtrue")  return "-1";
        if (nLower == "vbfalse") return "0";
        if (nLower == "vbyes")   return "6";
        if (nLower == "vbno")    return "7";
        if (nLower == "vbok")    return "1";
        if (nLower == "vbcancel") return "2";
        if (nLower == "vbabort")  return "3";
        if (nLower == "vbretry")  return "4";
        if (nLower == "vbignore") return "5";

        // 特殊值
        if (nLower == "vbempty")    return "vb6_VariantEmpty()";
        if (nLower == "vbnull")     return "vb6_VariantNull()";
        if (nLower == "vbnothing")  return "NULL";

        // 项目级Const: 通过符号表查找Constant符号 (P20-20)
        {
            Symbol* sym = symTab_.lookup(n);
            if (sym && sym->kind == SymbolKind::Constant && sym->hasConstValue) {
                switch (sym->constType) {
                    case Vb6Type::Long:
                    case Vb6Type::Integer: { return std::to_string(sym->constIntValue); }
                    case Vb6Type::Single:
                    case Vb6Type::Double: { return std::to_string(sym->constFloatValue); }
                    case Vb6Type::String: {
                        // C-escape constStringValue before embedding in C string literal
                        std::string cEsc;
                        cEsc.reserve(sym->constStringValue.size() + 16);
                        for (char ec : sym->constStringValue) {
                            switch (ec) {
                                case '\\': cEsc += "\\\\"; break;
                                case '"':  cEsc += "\\\""; break;
                                case '\n': cEsc += "\\n"; break;
                                case '\r': cEsc += "\\r"; break;
                                case '\t': cEsc += "\\t"; break;
                                default:   cEsc += ec; break;
                            }
                        }
                        return std::string("vb6_BSTR_FromStr(L\"") + cEsc + "\")";
                    }
                    case Vb6Type::Boolean: { return sym->constBoolValue ? "-1" : "0"; }
                    default: break;
                }
            }
        }

        // 未知标识符, 暂不处理
        return "";
    }
    
    // 其他非字面量表达式 -> 暂不支持, 返回空让cgen用类型零值
    return "";
}

} // namespace vb6c3
