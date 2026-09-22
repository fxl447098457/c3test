// vb6c3 - 声明解析器 — Declare / Event / Delegate / Const / Variable 声明 + 参数与类型引用
// 由 src/parser/parser_decl.cpp 拆出（2026-09-17），纯搬移、零行为改动。

#include "parser/parser.hpp"

namespace vb6c3 {


// ============================================================
// Declare 声明 (外部函数声明)
// ============================================================

std::unique_ptr<DeclareDecl> Parser::parseDeclareDecl(AccessLevel access) {
    auto loc = currentLoc();
    advance(); // consume 'Declare'

    bool isPtrSafe = false;
    if (match(TokenKind::Identifier) && toLower(cur_.text) == "ptrsafe") {
        isPtrSafe = true;
        advance(); // consume PtrSafe (实际是标识符, 词法器可能不会识别为关键字)
    }

    ProcKind procKind;
    if (match(TokenKind::Sub)) {
        procKind = ProcKind::Sub;
    } else if (match(TokenKind::Function)) {
        procKind = ProcKind::Function;
    } else {
        diag_.error(DiagnosticID::ParseExpectedToken, currentLoc(),
            "expected 'Sub' or 'Function' after 'Declare'");
        procKind = ProcKind::Sub;
    }

    auto nameTok = expectName("expected Declare name");

    expect(TokenKind::Lib, DiagnosticID::ParseExpectedToken,
           "expected 'Lib' in Declare statement");
    auto libTok = expect(TokenKind::StringLiteral, "expected library name string");

    std::string aliasName;
    if (match(TokenKind::Alias)) {
        auto aliasTok = expect(TokenKind::StringLiteral, "expected alias name string");
        aliasName = aliasTok.text;
    }

    // 调用约定: 默认 StdCall, 可选 CDecl
    CallConv callingConv = CallConv::StdCall;
    if (match(TokenKind::CDecl)) {
        callingConv = CallConv::CDecl;
    }

    auto params = parseParameterList();

    TypeRefPtr returnType;
    if (procKind == ProcKind::Function && match(TokenKind::As)) {
        returnType = parseTypeRef();
    }

    return std::make_unique<DeclareDecl>(loc, access, procKind,
        nameTok.text, libTok.text, aliasName, callingConv, isPtrSafe,
        std::move(params), std::move(returnType));
}

// ============================================================
// Event 声明
// ============================================================

std::unique_ptr<EventDecl> Parser::parseEventDecl(AccessLevel access) {
    auto loc = currentLoc();
    advance(); // consume 'Event'
    auto nameTok = expectName("expected Event name");
    auto params = parseParameterList();
    expectEndOfStatement();
    return std::make_unique<EventDecl>(loc, access, nameTok.text, std::move(params));
}

// ============================================================
// Delegate 声明 (tB 扩展: 具名函数指针类型)
// ============================================================

std::unique_ptr<DelegateDecl> Parser::parseDelegateDecl(AccessLevel access) {
    auto loc = currentLoc();
    advance(); // consume 'Delegate'

    ProcKind procKind;
    if (match(TokenKind::Sub)) {
        procKind = ProcKind::Sub;
    } else if (match(TokenKind::Function)) {
        procKind = ProcKind::Function;
    } else {
        diag_.error(DiagnosticID::ParseExpectedToken, currentLoc(),
            "expected 'Sub' or 'Function' after 'Delegate'");
        procKind = ProcKind::Sub;
    }

    auto nameTok = expectName("expected Delegate name");

    // 调用约定: 默认 StdCall, 可选 CDecl (位置同 Declare: 参数表之前)
    CallConv callingConv = CallConv::StdCall;
    if (match(TokenKind::CDecl)) {
        callingConv = CallConv::CDecl;
    }

    auto params = parseParameterList();

    TypeRefPtr returnType;
    if (procKind == ProcKind::Function) {
        if (match(TokenKind::As)) {
            returnType = parseTypeRef();
        } else {
            diag_.error(DiagnosticID::ParseExpectedToken, currentLoc(),
                "expected 'As type' in Delegate Function declaration");
        }
    }
    expectEndOfStatement();

    return std::make_unique<DelegateDecl>(loc, access, procKind,
        nameTok.text, callingConv, std::move(params), std::move(returnType));
}

// ============================================================
// Const 声明
// ============================================================

std::unique_ptr<ConstDecl> Parser::parseConstDecl(AccessLevel access) {
    auto loc = currentLoc();
    advance(); // consume 'Const'

    auto nameTok = expectName("expected Const name");

    // Fix 028: 剥离 VB6 类型后缀 ($%&!#@)
    auto suffixInfo = stripTypeSuffix(nameTok.text);
    const std::string& constName = suffixInfo.name;

    TypeRefPtr asType;
    if (match(TokenKind::As)) {
        asType = parseTypeRef();
    }

    expect(TokenKind::Equals, DiagnosticID::ParseExpectedToken,
           "expected '=' in Const declaration");

    auto value = parseExpression();

    return std::make_unique<ConstDecl>(loc, access, constName,
        std::move(asType), std::move(value));
}

// Const 列表: Const A = 1, B = 2, C As Long = 3
DeclPtr Parser::parseConstDeclList(AccessLevel access) {
    auto first = parseConstDecl(access);
    if (cur_.kind != TokenKind::Comma) {
        return first;
    }
    DeclList decls;
    decls.push_back(std::move(first));
    while (match(TokenKind::Comma)) {
        auto loc = currentLoc();
        auto nameTok = expectName("expected Const name");
        // Fix 028: 剥离 VB6 类型后缀 ($%&!#@)
        auto suffixInfo = stripTypeSuffix(nameTok.text);
        const std::string& constName = suffixInfo.name;
        TypeRefPtr asType;
        if (match(TokenKind::As)) {
            asType = parseTypeRef();
        }
        expect(TokenKind::Equals, DiagnosticID::ParseExpectedToken,
               "expected '=' in Const declaration");
        auto value = parseExpression();
        decls.push_back(std::make_unique<ConstDecl>(loc, access, constName,
            std::move(asType), std::move(value)));
    }
    return std::make_unique<MultiDecl>(decls[0]->loc, std::move(decls));
}

// ============================================================
// Variable 声明
// ============================================================

std::unique_ptr<VariableDecl> Parser::parseVariableDecl(AccessLevel access, bool isStatic) {
    auto loc = currentLoc();
    // 注意: Dim/Public/Private/Static 已由调用方消费
    // 但如果从 parseDeclaration 直接调用, Dim 尚未消费
    if (cur_.kind == TokenKind::Dim || cur_.kind == TokenKind::Public ||
        cur_.kind == TokenKind::Private || cur_.kind == TokenKind::Static ||
        cur_.kind == TokenKind::Global || cur_.kind == TokenKind::Friend) {
        advance();
    }

    bool isWithEvents = false;
    if (match(TokenKind::WithEvents)) {
        isWithEvents = true;
    }

    auto nameTok = expectName("expected variable name");

    // Fix 028: 剥离 VB6 类型后缀 ($%&!#@), 防止 cIdent 把 $ 改成 _ 导致
    // 声明名(BSTR/VARIANT Address_) 与使用名(Address) 不一致 → C2065。
    // 注意: 不注入后缀对应的类型, 保留默认 Variant 行为(避免引入 C2440)。
    auto suffixInfo = stripTypeSuffix(nameTok.text);
    const std::string& varName = suffixInfo.name;

    // 数组维度: dim a(1 To 10, 1 To 20) As Long
    // 或动态数组: dim a() As Long
    std::vector<VariableDecl::Dimension> dimensions;
    bool isDynamicArray = false;
    if (match(TokenKind::LeftParen)) {
        // 空括号 = 动态数组 (Dim a() As Long)
        if (cur_.kind != TokenKind::RightParen) {
            do {
                VariableDecl::Dimension dim;
                auto first = parseExpression();
                if (match(TokenKind::To)) {
                    dim.lower = std::move(first);
                    dim.upper = parseExpression();
                } else {
                    dim.upper = std::move(first);
                }
                dimensions.push_back(std::move(dim));
            } while (match(TokenKind::Comma));
        } else {
            // 空括号 () = 动态数组
            isDynamicArray = true;
        }
        expect(TokenKind::RightParen, DiagnosticID::ParseExpectedToken,
               "expected ')'");
    }

    bool isNew = false;
    TypeRefPtr asType;
    if (match(TokenKind::As)) {
        if (match(TokenKind::New)) {
            isNew = true;
        }
        asType = parseTypeRef();
    }

    ExprPtr initializer;
    if (match(TokenKind::Equals)) {
        initializer = parseExpression();
    }

    return std::make_unique<VariableDecl>(loc, access, varName,
        isWithEvents, isStatic, isNew, std::move(asType), std::move(initializer),
        std::move(dimensions), isDynamicArray);
}

// Variable 列表: Dim a, b As Long, c As String
// parseVariableDecl 会条件性地消费 Dim/Private/Public 关键字,
// 对逗号后的后续变量, 当前 token 是变量名而非关键字, 所以可以直接复用
DeclPtr Parser::parseVariableDeclList(AccessLevel access, bool isStatic) {
    auto first = parseVariableDecl(access, isStatic);
    if (cur_.kind != TokenKind::Comma) {
        return first;
    }
    DeclList decls;
    decls.push_back(std::move(first));
    while (match(TokenKind::Comma)) {
        decls.push_back(parseVariableDecl(access, isStatic));
    }
    return std::make_unique<MultiDecl>(decls[0]->loc, std::move(decls));
}

// ============================================================
// 参数列表
// ============================================================

std::vector<std::unique_ptr<ParameterDecl>> Parser::parseParameterList() {
    std::vector<std::unique_ptr<ParameterDecl>> params;

    if (!match(TokenKind::LeftParen)) {
        return params;  // 无参数列表
    }

    skipNewLines();
    if (cur_.kind != TokenKind::RightParen) {
        do {
            skipNewLines();
            params.push_back(parseParameter());
            skipNewLines();
        } while (match(TokenKind::Comma));
    }

    expect(TokenKind::RightParen, DiagnosticID::ParseExpectedToken,
           "expected ')'");
    return params;
}

std::unique_ptr<ParameterDecl> Parser::parseParameter() {
    auto loc = currentLoc();

    bool isOptional = false;
    bool isByVal = false;
    bool isParamArray = false;

    if (match(TokenKind::Optional)) {
        isOptional = true;
    }
    if (match(TokenKind::ByVal)) {
        isByVal = true;
    } else if (match(TokenKind::ByRef)) {
        isByVal = false;  // 显式 ByRef (默认)
    }
    if (match(TokenKind::ParamArray)) {
        isParamArray = true;
    }

    auto nameTok = expectName("expected parameter name");

    // Fix 028: 剥离 VB6 类型后缀 ($%&!#@), 见 parseVariableDecl 同样说明。
    auto suffixInfo = stripTypeSuffix(nameTok.text);
    const std::string& paramName = suffixInfo.name;

    // 数组参数: name() As Type
    bool isArrayParam = false;
    if (match(TokenKind::LeftParen)) {
        expect(TokenKind::RightParen, DiagnosticID::ParseExpectedToken,
               "expected ')'");
        isArrayParam = true;
    }

    TypeRefPtr asType;
    if (match(TokenKind::As)) {
        if (cur_.kind == TokenKind::Any) {
            // Declare 中的 Any 类型
            advance();
            asType = std::make_unique<SimpleTypeRef>(currentLoc(), "Any");
        } else {
            asType = parseTypeRef();
        }
    }

    ExprPtr defaultValue;
    if (isOptional && match(TokenKind::Equals)) {
        defaultValue = parseExpression();
    }

    // 数组参数: name() As Type 需要将类型包装为 ArrayTypeRef
    if (isArrayParam && asType && asType->kind == ASTNodeKind::SimpleTypeRef) {
        auto& simple = static_cast<SimpleTypeRef&>(*asType);
        auto elemType = std::make_unique<SimpleTypeRef>(simple.loc, simple.name);
        asType = std::make_unique<ArrayTypeRef>(simple.loc, std::move(elemType), std::vector<ArrayTypeRef::Dimension>{});
    }

    return std::make_unique<ParameterDecl>(loc, paramName, isOptional,
        isByVal, isParamArray, std::move(asType), std::move(defaultValue));
}

// ============================================================
// 类型引用
// ============================================================

TypeRefPtr Parser::parseTypeRef() {
    auto loc = currentLoc();

    // 简单类型名: Long, String, MyClass 等
    if (cur_.kind == TokenKind::Identifier || cur_.kind == TokenKind::Boolean ||
        cur_.kind == TokenKind::Byte || cur_.kind == TokenKind::Integer ||
        cur_.kind == TokenKind::Long || cur_.kind == TokenKind::LongLong ||
        cur_.kind == TokenKind::LongPtr || cur_.kind == TokenKind::Single ||
        cur_.kind == TokenKind::Double || cur_.kind == TokenKind::Currency ||
        cur_.kind == TokenKind::Decimal || cur_.kind == TokenKind::Date ||
        cur_.kind == TokenKind::Object || cur_.kind == TokenKind::String ||
        cur_.kind == TokenKind::Variant) {
        auto tok = advance();
        auto typeRef = std::make_unique<SimpleTypeRef>(loc, tok.text);

        // 限定类型名: Scripting.Dictionary, MSComctlLib.ImageList 等
        if (cur_.kind == TokenKind::Dot) {
            std::string qualified = tok.text;
            while (cur_.kind == TokenKind::Dot) {
                advance(); // consume '.'
            // VB6 允许关键字作为限定类型名组件: ADODB.Error, ADODB.Command 等
            // 与 parsePostfix() Dot 分支一致, 使用 canBeName + 文本检查
            if (canBeName(cur_.kind)) {
                qualified += "." + advance().text;
            } else if (!cur_.text.empty() && cur_.kind != TokenKind::EndOfFile &&
                       cur_.kind != TokenKind::NewLine && cur_.kind != TokenKind::Colon &&
                       cur_.kind != TokenKind::LeftParen && cur_.kind != TokenKind::RightParen &&
                       cur_.kind != TokenKind::Comma) {
                // 硬关键字也可作为限定类型名组件 (如 Error, Command, Type 等)
                qualified += "." + advance().text;
            } else {
                break;
            }
            }
            typeRef = std::make_unique<SimpleTypeRef>(loc, qualified);
        }

        // 数组类型: Long(), String(10)
        if (match(TokenKind::LeftParen)) {
            std::vector<ArrayTypeRef::Dimension> dims;
            if (cur_.kind != TokenKind::RightParen) {
                do {
                    ArrayTypeRef::Dimension dim;
                    auto first = parseExpression();
                    if (match(TokenKind::To)) {
                        dim.lower = std::move(first);
                        dim.upper = parseExpression();
                    } else {
                        dim.upper = std::move(first);
                    }
                    dims.push_back(std::move(dim));
                } while (match(TokenKind::Comma));
            }
            expect(TokenKind::RightParen, DiagnosticID::ParseExpectedToken,
                   "expected ')'");
            return std::make_unique<ArrayTypeRef>(loc, std::move(typeRef), std::move(dims));
        }

        // 定长字符串: String * N
        if (tok.kind == TokenKind::String && match(TokenKind::Star)) {
            auto len = parseExpression();
            return std::make_unique<FixedStringTypeRef>(loc, std::move(len));
        }

        return typeRef;
    }

    // 括号标识符类型: [My Type]
    if (cur_.kind == TokenKind::LeftBracket) {
        advance();
        auto nameTok = expectName("expected type name");
        expect(TokenKind::RightBracket, DiagnosticID::ParseExpectedToken,
               "expected ']'");
        return std::make_unique<SimpleTypeRef>(loc, nameTok.text);
    }

    diag_.error(DiagnosticID::ParseExpectedToken, currentLoc(),
        "expected type name");
    return std::make_unique<SimpleTypeRef>(loc, "Variant");
}

} // namespace vb6c3
