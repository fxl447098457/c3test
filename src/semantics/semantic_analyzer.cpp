#include "semantics/semantic_analyzer.hpp"
#include <algorithm>
#include <cctype>

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
    : diag_(diag), symTab_(diag), verbose_(verbose) {}

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
                    break;
                }
                case ASTNodeKind::FunctionDecl: {
                    auto& f = static_cast<FunctionDecl&>(*decl);
                    classSym->memberNames.push_back(f.name);
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
        resolveTypeRef(decl.asType.get()),
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
    symTab_.define(std::move(sym));
}

void SemanticAnalyzer::registerConstant(ConstDecl& decl) {
    auto sym = std::make_unique<Symbol>(
        SymbolKind::Constant, decl.name,
        resolveTypeRef(decl.asType.get()),
        decl.loc, decl.access
    );

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
                    sym->constStringValue = lit->rawText;
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

Vb6Type SemanticAnalyzer::resolveTypeRef(ASTNode* typeRef) {
    if (!typeRef) return Vb6Type::Variant;  // VB6默认: Variant

    switch (typeRef->kind) {
        case ASTNodeKind::SimpleTypeRef: {
            auto& simple = static_cast<SimpleTypeRef&>(*typeRef);
            Vb6Type t = typeSys_.resolveTypeName(simple.name);
            if (t == Vb6Type::Unknown) {
                // 可能是用户自定义类型 -> 在符号表中查找
                std::string lower = Symbol::toLower(simple.name);
                if (auto* sym = symTab_.lookupModule(lower)) {
                    if (sym->kind == SymbolKind::UserDefinedType)
                        return Vb6Type::UserDefinedType;
                    if (sym->kind == SymbolKind::EnumType)
                        return Vb6Type::Long;  // Enum成员是Long
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
            pi.type = resolveTypeRef(param->asType.get());
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
    } else {
        // Pass2: 分析过程体
        if (verbose_) std::cerr << "[Sem]   Sub: " << node.name << std::endl;
        auto* sym = symTab_.lookupModule(node.name);
        if (!sym) return;  // 注册失败则跳过

        currentProc_ = sym;
        symTab_.pushScope(ScopeKind::Procedure);

        // 注册参数到过程作用域
        for (auto& param : node.params) {
            auto paramSym = std::make_unique<Symbol>(
                SymbolKind::Parameter, param->name,
                resolveTypeRef(param->asType.get()),
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
        Vb6Type retType = resolveTypeRef(node.returnType.get());
        auto sym = std::make_unique<Symbol>(
            SymbolKind::Function, node.name,
            retType, node.loc, node.access
        );
        sym->isStatic = node.isStatic;

        // 注册参数
        for (auto& param : node.params) {
            ParameterInfo pi;
            pi.name = param->name;
            pi.type = resolveTypeRef(param->asType.get());
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
    } else {
        // Pass2: 分析过程体
        if (verbose_) std::cerr << "[Sem]   Function: " << node.name << std::endl;
        auto* sym = symTab_.lookupModule(node.name);
        if (!sym) return;

        currentProc_ = sym;
        symTab_.pushScope(ScopeKind::Procedure);

        // 注册参数
        for (auto& param : node.params) {
            auto paramSym = std::make_unique<Symbol>(
                SymbolKind::Parameter, param->name,
                resolveTypeRef(param->asType.get()),
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

        Vb6Type retType = resolveTypeRef(node.returnType.get());
        auto sym = std::make_unique<Symbol>(sk, node.name, retType, node.loc, node.access);

        for (auto& param : node.params) {
            ParameterInfo pi;
            pi.name = param->name;
            pi.type = resolveTypeRef(param->asType.get());
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

        currentProc_ = sym;
        symTab_.pushScope(ScopeKind::Procedure);

        for (auto& param : node.params) {
            auto paramSym = std::make_unique<Symbol>(
                SymbolKind::Parameter, param->name,
                resolveTypeRef(param->asType.get()),
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
        symTab_.define(std::move(sym));
    }
    // UDT成员在Pass1注册到类型符号 (暂简化, 后续扩展)
}

void SemanticAnalyzer::visit(TypeMember& node) {
    // UDT成员分析 (暂不实现, 留待扩展)
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
            pi.type = resolveTypeRef(param->asType.get());
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
            pi.type = resolveTypeRef(param->asType.get());
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
            diag_.error(DiagnosticID::SemUndeclaredIdentifier, node.loc,
                "未声明的变量: '" + node.varName + "'");
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
                    resolveTypeRef(varDecl.asType.get()),
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
                    resolveTypeRef(constDecl.asType.get()),
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
            diag_.error(DiagnosticID::SemUndeclaredIdentifier, node.loc,
                "未声明的标识符: '" + node.name + "'");
        }
        lastExprType_ = Vb6Type::Variant;  // 宽松模式: 推导为Variant
    }
}

void SemanticAnalyzer::visit(MemberAccessExpr& node) {
    Vb6Type objType = analyzeExpr(*node.object);
    // VB6的.访问在编译期通常无法确定类型 (晚期绑定)
    // 简化: 推导为Variant
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
    // 如果当前模块是类模块, Me代表类实例
    if (currentModule_ && currentModule_->isClassModule) {
        lastExprType_ = Vb6Type::Object;
    } else {
        diag_.warn(DiagnosticID::SemTypeMismatch, node.loc,
            "'Me' can only be used in class modules");
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
    // 字符串函数
    addBuiltinFunc("Len", Vb6Type::Long);
    addBuiltinFunc("Left", Vb6Type::String);
    addBuiltinFunc("Right", Vb6Type::String);
    addBuiltinFunc("Mid", Vb6Type::String);
    addBuiltinFunc("InStr", Vb6Type::Long);
    addBuiltinFunc("InStrRev", Vb6Type::Long);
    addBuiltinFunc("LTrim", Vb6Type::String);
    addBuiltinFunc("RTrim", Vb6Type::String);
    addBuiltinFunc("Trim", Vb6Type::String);
    addBuiltinFunc("LCase", Vb6Type::String);
    addBuiltinFunc("UCase", Vb6Type::String);
    addBuiltinFunc("Replace", Vb6Type::String);
    addBuiltinFunc("Split", Vb6Type::Variant);
    addBuiltinFunc("Join", Vb6Type::String);
    addBuiltinFunc("StrComp", Vb6Type::Long);
    addBuiltinFunc("StrReverse", Vb6Type::String);
    addBuiltinFunc("Space", Vb6Type::String);
    addBuiltinFunc("String", Vb6Type::String);
    addBuiltinFunc("Asc", Vb6Type::Long);
    addBuiltinFunc("Chr", Vb6Type::String);
    addBuiltinFunc("Val", Vb6Type::Double);
    addBuiltinFunc("Str", Vb6Type::String);
    addBuiltinFunc("Format", Vb6Type::String);
    addBuiltinFunc("CStr", Vb6Type::String);
    addBuiltinFunc("CInt", Vb6Type::Integer);
    addBuiltinFunc("CLng", Vb6Type::Long);
    addBuiltinFunc("CSng", Vb6Type::Single);
    addBuiltinFunc("CDbl", Vb6Type::Double);
    addBuiltinFunc("CBool", Vb6Type::Boolean);
    addBuiltinFunc("CDate", Vb6Type::Date);
    addBuiltinFunc("CByte", Vb6Type::Byte);
    addBuiltinFunc("CCur", Vb6Type::Currency);
    addBuiltinFunc("CDec", Vb6Type::Variant);  // P18-A: CDec returns Variant (Decimal subtype)
    addBuiltinFunc("CVar", Vb6Type::Variant);
    addBuiltinFunc("CVErr", Vb6Type::Variant);
    // 数值函数
    addBuiltinFunc("Abs", Vb6Type::Double);
    addBuiltinFunc("Int", Vb6Type::Double);
    addBuiltinFunc("Fix", Vb6Type::Double);
    addBuiltinFunc("Sgn", Vb6Type::Long);
    addBuiltinFunc("Sqr", Vb6Type::Double);
    addBuiltinFunc("Round", Vb6Type::Double);
    addBuiltinFunc("Rnd", Vb6Type::Single);
    // 转换与类型检查
    addBuiltinFunc("IsNumeric", Vb6Type::Boolean);
    addBuiltinFunc("IsDate", Vb6Type::Boolean);
    addBuiltinFunc("IsEmpty", Vb6Type::Boolean);
    addBuiltinFunc("IsNull", Vb6Type::Boolean);
    addBuiltinFunc("IsObject", Vb6Type::Boolean);
    addBuiltinFunc("IsArray", Vb6Type::Boolean);
    addBuiltinFunc("IsNothing", Vb6Type::Boolean);
    addBuiltinFunc("TypeName", Vb6Type::String);
    addBuiltinFunc("VarType", Vb6Type::Long);
    // 数组
    addBuiltinFunc("UBound", Vb6Type::Long);
    addBuiltinFunc("LBound", Vb6Type::Long);
    addBuiltinFunc("Array", Vb6Type::Variant);
    // 数学
    addBuiltinFunc("Sin", Vb6Type::Double);
    addBuiltinFunc("Cos", Vb6Type::Double);
    addBuiltinFunc("Tan", Vb6Type::Double);
    addBuiltinFunc("Atn", Vb6Type::Double);
    addBuiltinFunc("Exp", Vb6Type::Double);
    addBuiltinFunc("Log", Vb6Type::Double);
    // 文件
    addBuiltinFunc("FreeFile", Vb6Type::Long);
    addBuiltinFunc("LOF", Vb6Type::Long);
    addBuiltinFunc("EOF", Vb6Type::Boolean);
    addBuiltinFunc("Loc", Vb6Type::Long);
    addBuiltinFunc("FileLen", Vb6Type::Long);
    addBuiltinFunc("FileDateTime", Vb6Type::Date);
    addBuiltinFunc("Dir", Vb6Type::String);
    addBuiltinFunc("CurDir", Vb6Type::String);
    addBuiltinFunc("Shell", Vb6Type::Long);
    addBuiltinFunc("Environ", Vb6Type::String);
    addBuiltinFunc("Command", Vb6Type::String);
    // 日期时间
    addBuiltinFunc("Now", Vb6Type::Date);
    addBuiltinFunc("Date", Vb6Type::Date);
    addBuiltinFunc("Time", Vb6Type::Date);
    addBuiltinFunc("DateAdd", Vb6Type::Date);
    addBuiltinFunc("DateDiff", Vb6Type::Long);
    addBuiltinFunc("DatePart", Vb6Type::Long);
    addBuiltinFunc("DateSerial", Vb6Type::Double);
    addBuiltinFunc("DateValue", Vb6Type::Date);
    addBuiltinFunc("TimeSerial", Vb6Type::Date);
    addBuiltinFunc("TimeValue", Vb6Type::Date);
    addBuiltinFunc("Year", Vb6Type::Long);
    addBuiltinFunc("Month", Vb6Type::Long);
    addBuiltinFunc("Day", Vb6Type::Long);
    addBuiltinFunc("Hour", Vb6Type::Long);
    addBuiltinFunc("Minute", Vb6Type::Long);
    addBuiltinFunc("Second", Vb6Type::Long);
    addBuiltinFunc("Weekday", Vb6Type::Long);

    // 交互
    addBuiltinFunc("MsgBox", Vb6Type::Long);
    addBuiltinFunc("InputBox", Vb6Type::String);
    addBuiltinFunc("RGB", Vb6Type::Long);
    addBuiltinFunc("QBColor", Vb6Type::Long);
    // 杂项
    addBuiltinFunc("DoEvents", Vb6Type::Long);
    addBuiltinFunc("CreateObject", Vb6Type::Object);
    addBuiltinFunc("GetObject", Vb6Type::Object);
    addBuiltinFunc("LoadPicture", Vb6Type::Object);
    addBuiltinFunc("SavePicture", Vb6Type::Void);
    addBuiltinFunc("Load", Vb6Type::Void);
    addBuiltinFunc("Unload", Vb6Type::Void);
    addBuiltinFunc("SendKeys", Vb6Type::Void);
    addBuiltinFunc("AppActivate", Vb6Type::Void);
    addBuiltinFunc("IsMissing", Vb6Type::Boolean);
    addBuiltinFunc("IIf", Vb6Type::Variant);
    addBuiltinFunc("Beep", Vb6Type::Void);
// P14.3.5: CallByName(obj, procName$, callType, [args...])
addBuiltinFunc("CallByName", Vb6Type::Variant);
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
    
    // 非字面量表达式(如常量引用、运算表达式) -> 暂不支持, 返回空让cgen用类型零值
    return "";
}

} // namespace vb6c3
