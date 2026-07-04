// vb6c3 - 声明解析器
// Sub / Function / Property / Type / Enum / Declare / Event / Const / Variable

#include "parser/parser.hpp"

namespace vb6c3 {

// ============================================================
// 声明分发
// ============================================================

DeclPtr Parser::parseDeclaration() {
    switch (cur_.kind) {
        case TokenKind::Sub:      return parseSubDecl(AccessLevel::Default, false);
        case TokenKind::Function: return parseFunctionDecl(AccessLevel::Default, false);
        case TokenKind::Property: return parsePropertyDecl(AccessLevel::Default);
        case TokenKind::Type:     return parseTypeDecl(AccessLevel::Default);
        case TokenKind::Enum:     return parseEnumDecl(AccessLevel::Default);
        case TokenKind::Declare:  return parseDeclareDecl(AccessLevel::Default);
        case TokenKind::Event:    return parseEventDecl(AccessLevel::Default);
        case TokenKind::Const:    return parseConstDecl(AccessLevel::Default);
        case TokenKind::Dim:      return parseVariableDecl(AccessLevel::Default, false);
        case TokenKind::Static:   return parseVariableDecl(AccessLevel::Private, true);

        case TokenKind::Public:
        case TokenKind::Private:
        case TokenKind::Friend:
        case TokenKind::Global: {
            AccessLevel access;
            if (cur_.kind == TokenKind::Public || cur_.kind == TokenKind::Global) {
                access = AccessLevel::Public;
            } else if (cur_.kind == TokenKind::Friend) {
                access = AccessLevel::Friend;
            } else {
                access = AccessLevel::Private;
            }
            advance(); // consume access modifier

            // Public Sub/Function/Property/Type/Enum/Declare/Event/Const/Dim
            switch (cur_.kind) {
                case TokenKind::Sub:      return parseSubDecl(access, false);
                case TokenKind::Function: return parseFunctionDecl(access, false);
                case TokenKind::Property: return parsePropertyDecl(access);
                case TokenKind::Type:     return parseTypeDecl(access);
                case TokenKind::Enum:     return parseEnumDecl(access);
                case TokenKind::Declare:  return parseDeclareDecl(access);
                case TokenKind::Event:    return parseEventDecl(access);
                case TokenKind::Const:    return parseConstDecl(access);
                default:                  return parseVariableDecl(access, false);
            }
        }

        default:
            diag_.error(DiagnosticID::ParseUnexpectedToken, currentLoc(),
                "expected declaration");
            advance();
            return nullptr;
    }
}

// ============================================================
// Sub 声明
// ============================================================

std::unique_ptr<SubDecl> Parser::parseSubDecl(AccessLevel access, bool isStatic) {
    auto loc = currentLoc();
    advance(); // consume 'Sub'

    auto nameTok = expectName("expected Sub name");
    auto params = parseParameterList();
    expectEndOfStatement();

    auto body = parseBlockUntil({TokenKind::End});

    expect(TokenKind::End, DiagnosticID::ParseMismatchedBlock,
           "expected 'End Sub'");
    expect(TokenKind::Sub, DiagnosticID::ParseMismatchedBlock,
           "expected 'End Sub'");

    return std::make_unique<SubDecl>(loc, access, nameTok.text,
        std::move(params), std::move(body), isStatic);
}

// ============================================================
// Function 声明
// ============================================================

std::unique_ptr<FunctionDecl> Parser::parseFunctionDecl(AccessLevel access, bool isStatic) {
    auto loc = currentLoc();
    advance(); // consume 'Function'

    auto nameTok = expectName("expected Function name");
    auto params = parseParameterList();

    TypeRefPtr returnType;
    if (match(TokenKind::As)) {
        returnType = parseTypeRef();
    }
    expectEndOfStatement();

    auto body = parseBlockUntil({TokenKind::End});

    expect(TokenKind::End, DiagnosticID::ParseMismatchedBlock,
           "expected 'End Function'");
    expect(TokenKind::Function, DiagnosticID::ParseMismatchedBlock,
           "expected 'End Function'");

    return std::make_unique<FunctionDecl>(loc, access, nameTok.text,
        std::move(params), std::move(returnType), std::move(body), isStatic);
}

// ============================================================
// Property 声明
// ============================================================

std::unique_ptr<PropertyDecl> Parser::parsePropertyDecl(AccessLevel access) {
    auto loc = currentLoc();
    advance(); // consume 'Property'

    ProcKind propKind;
    if (match(TokenKind::Get)) {
        propKind = ProcKind::PropertyGet;
    } else if (match(TokenKind::Let)) {
        propKind = ProcKind::PropertyLet;
    } else if (match(TokenKind::Set)) {
        propKind = ProcKind::PropertySet;
    } else {
        diag_.error(DiagnosticID::ParseExpectedToken, currentLoc(),
            "expected 'Get', 'Let', or 'Set' after 'Property'");
        propKind = ProcKind::PropertyGet;
    }

    auto nameTok = expectName("expected Property name");
    auto params = parseParameterList();

    TypeRefPtr returnType;
    if (match(TokenKind::As)) {
        returnType = parseTypeRef();
    }
    expectEndOfStatement();

    auto body = parseBlockUntil({TokenKind::End});

    expect(TokenKind::End, DiagnosticID::ParseMismatchedBlock,
           "expected 'End Property'");
    expect(TokenKind::Property, DiagnosticID::ParseMismatchedBlock,
           "expected 'End Property'");

    return std::make_unique<PropertyDecl>(loc, access, propKind,
        nameTok.text, std::move(params), std::move(returnType), std::move(body));
}

// ============================================================
// Type 声明 (用户自定义类型/UDT)
// ============================================================

std::unique_ptr<TypeDecl> Parser::parseTypeDecl(AccessLevel access) {
    auto loc = currentLoc();
    advance(); // consume 'Type'
    auto nameTok = expectName("expected Type name");
    expectEndOfStatement();

    std::vector<std::unique_ptr<TypeMember>> members;
    while (cur_.kind != TokenKind::End && cur_.kind != TokenKind::EndOfFile) {
        skipNewLines();
        if (cur_.kind == TokenKind::End) break;

        auto memberLoc = currentLoc();
        auto memberName = expectName("expected member name");

        // 可能有数组维度: memberName(10) As Type
        ExprPtr arraySize;
        if (match(TokenKind::LeftParen)) {
            arraySize = parseExpression();
            expect(TokenKind::RightParen, DiagnosticID::ParseExpectedToken,
                   "expected ')'");
        }

        TypeRefPtr type;
        if (match(TokenKind::As)) {
            type = parseTypeRef();
        }
        expectEndOfStatement();

        members.push_back(std::make_unique<TypeMember>(memberLoc,
            memberName.text, std::move(type), std::move(arraySize)));
    }

    expect(TokenKind::End, DiagnosticID::ParseMismatchedBlock,
           "expected 'End Type'");
    expect(TokenKind::Type, DiagnosticID::ParseMismatchedBlock,
           "expected 'End Type'");

    return std::make_unique<TypeDecl>(loc, access, nameTok.text, std::move(members));
}

// ============================================================
// Enum 声明
// ============================================================

std::unique_ptr<EnumDecl> Parser::parseEnumDecl(AccessLevel access) {
    auto loc = currentLoc();
    advance(); // consume 'Enum'
    auto nameTok = expectName("expected Enum name");
    expectEndOfStatement();

    std::vector<std::unique_ptr<EnumMember>> members;
    while (cur_.kind != TokenKind::End && cur_.kind != TokenKind::EndOfFile) {
        skipNewLines();
        if (cur_.kind == TokenKind::End) break;

        auto memberLoc = currentLoc();
        auto memberName = expectName("expected enum member name");

        ExprPtr value;
        if (match(TokenKind::Equals)) {
            value = parseExpression();
        }
        expectEndOfStatement();

        members.push_back(std::make_unique<EnumMember>(memberLoc,
            memberName.text, std::move(value)));
    }

    expect(TokenKind::End, DiagnosticID::ParseMismatchedBlock,
           "expected 'End Enum'");
    expect(TokenKind::Enum, DiagnosticID::ParseMismatchedBlock,
           "expected 'End Enum'");

    return std::make_unique<EnumDecl>(loc, access, nameTok.text, std::move(members));
}

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
    return std::make_unique<EventDecl>(loc, access, nameTok.text, std::move(params));
}

// ============================================================
// Const 声明
// ============================================================

std::unique_ptr<ConstDecl> Parser::parseConstDecl(AccessLevel access) {
    auto loc = currentLoc();
    advance(); // consume 'Const'

    auto nameTok = expectName("expected Const name");

    TypeRefPtr asType;
    if (match(TokenKind::As)) {
        asType = parseTypeRef();
    }

    expect(TokenKind::Equals, DiagnosticID::ParseExpectedToken,
           "expected '=' in Const declaration");

    auto value = parseExpression();

    return std::make_unique<ConstDecl>(loc, access, nameTok.text,
        std::move(asType), std::move(value));
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

    return std::make_unique<VariableDecl>(loc, access, nameTok.text,
        isWithEvents, isStatic, isNew, std::move(asType), std::move(initializer),
        std::move(dimensions), isDynamicArray);
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

    // 数组参数: name() As Type
    if (match(TokenKind::LeftParen)) {
        expect(TokenKind::RightParen, DiagnosticID::ParseExpectedToken,
               "expected ')'");
        // 数组参数标记在类型上
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

    return std::make_unique<ParameterDecl>(loc, nameTok.text, isOptional,
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
                if (cur_.kind == TokenKind::Identifier || cur_.kind == TokenKind::Boolean ||
                    cur_.kind == TokenKind::Byte || cur_.kind == TokenKind::Integer ||
                    cur_.kind == TokenKind::Long || cur_.kind == TokenKind::LongLong ||
                    cur_.kind == TokenKind::LongPtr || cur_.kind == TokenKind::Single ||
                    cur_.kind == TokenKind::Double || cur_.kind == TokenKind::Currency ||
                    cur_.kind == TokenKind::Decimal || cur_.kind == TokenKind::Date ||
                    cur_.kind == TokenKind::Object || cur_.kind == TokenKind::String ||
                    cur_.kind == TokenKind::Variant) {
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
