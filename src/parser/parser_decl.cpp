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
        case TokenKind::Delegate: return parseDelegateDecl(AccessLevel::Default);
        case TokenKind::Const:    return parseConstDeclList(AccessLevel::Default);
        case TokenKind::Dim:      return parseVariableDeclList(AccessLevel::Default, false);
        case TokenKind::Static:   return parseVariableDeclList(AccessLevel::Private, true);

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

            // Public Sub/Function/property/Type/Enum/Declare/Event/Const/Dim
            switch (cur_.kind) {
                case TokenKind::Sub:      return parseSubDecl(access, false);
                case TokenKind::Function: return parseFunctionDecl(access, false);
                case TokenKind::Property: return parsePropertyDecl(access);
                case TokenKind::Type:     return parseTypeDecl(access);
                case TokenKind::Enum:     return parseEnumDecl(access);
                case TokenKind::Declare:  return parseDeclareDecl(access);
                case TokenKind::Event:    return parseEventDecl(access);
                case TokenKind::Delegate: return parseDelegateDecl(access);
                case TokenKind::Const:    return parseConstDeclList(access);
                default:                  return parseVariableDeclList(access, false);
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
// 成员级 Implements 尾子句 (tB 扩展, ai/022 D5, 批次 B02b)
// ============================================================
//
//   Private Sub OpenFile(s As String) Implements IStorage.Open, IStream.Read
//
// VB6 里"过程签名后跟 Implements"必然在 expectEndOfStatement() 处报 VB2003, 因此本
// 子句属"错误→可解析"的安全新增 (同 B01 属性行手法). 解出的 iface/member 名不在此处
// 校验语义, 由 checkNewStyleInterface 按登记表小写键解析.
//
// 点号拼接口径与模块级 parseImplements (Fix 083) 一致: `A.B.C` 的接口名取 `A.B`
// (工程/库限定), 成员名取末段.
void Parser::parseTrailingImplementsClauses(std::vector<ImplementsClause>& out) {
    if (cur_.kind != TokenKind::Implements) return;
    advance(); // 'Implements'

    // 畸形输入只报一条诊断: 留在行尾 NewLine 上让调用方的 expectEndOfStatement 正常通过.
    auto skipRestOfLine = [&]() {
        while (cur_.kind != TokenKind::NewLine && cur_.kind != TokenKind::EndOfFile) {
            advance();
        }
    };

    for (;;) {
        if (!canBeName(cur_.kind)) {
            diag_.error(DiagnosticID::ParseExpectedToken, currentLoc(),
                "expected interface member name after 'Implements'");
            skipRestOfLine();
            return;
        }
        ImplementsClause c;
        c.loc = currentLoc();
        std::vector<std::string> parts;
        parts.push_back(advance().text);
        while (cur_.kind == TokenKind::Dot && canBeName(peek2().kind)) {
            advance(); // '.'
            parts.push_back(advance().text);
        }
        if (parts.size() < 2) {
            diag_.error(DiagnosticID::ParseInvalidInterfaceMember, c.loc,
                "Member-level Implements needs a qualified name: Implements <Interface>.<Member> (got " +
                parts.front() + ")");
        } else {
            for (size_t i = 0; i + 1 < parts.size(); i++) {
                if (i) c.ifaceName += ".";
                c.ifaceName += parts[i];
            }
            c.memberName = parts.back();
            out.push_back(std::move(c));   // 畸形子句不入列表: 语义层不再二次报错
        }
        if (cur_.kind != TokenKind::Comma) break;
        advance(); // ','
    }
}

// ============================================================
// Sub 声明
// ============================================================

std::unique_ptr<SubDecl> Parser::parseSubDecl(AccessLevel access, bool isStatic) {
    auto loc = currentLoc();
    advance(); // consume 'Sub'

    auto nameTok = expectName("expected Sub name");
    auto typeParams = parseTypeParams();   // 泛型 (tB): Sub Foo(Of T)
    auto params = parseParameterList();
    std::vector<ImplementsClause> clauses;
    parseTrailingImplementsClauses(clauses);   // B02b
    expectEndOfStatement();

    auto body = parseBlockUntil({TokenKind::End});

    expect(TokenKind::End, DiagnosticID::ParseMismatchedBlock,
           "expected 'End Sub'");
    expect(TokenKind::Sub, DiagnosticID::ParseMismatchedBlock,
           "expected 'End Sub'");

    auto d = std::make_unique<SubDecl>(loc, access, nameTok.text,
        std::move(params), std::move(body), isStatic);
    d->typeParams = std::move(typeParams);
    d->implementsClauses = std::move(clauses);   // B02b
    curTypeParams_.clear();  // G3 护栏窗口只覆盖本模板 params+body
    return d;
}

// ============================================================
// Function 声明
// ============================================================

std::unique_ptr<FunctionDecl> Parser::parseFunctionDecl(AccessLevel access, bool isStatic) {
    auto loc = currentLoc();
    advance(); // consume 'Function'

    auto nameTok = expectName("expected Function name");
    auto typeParams = parseTypeParams();   // 泛型 (tB): Function Foo(Of T)
    auto params = parseParameterList();

    TypeRefPtr returnType;
    if (match(TokenKind::As)) {
        returnType = parseTypeRef();
    }
    std::vector<ImplementsClause> clauses;
    parseTrailingImplementsClauses(clauses);   // B02b: 子句写在 `As Type` 之后
    expectEndOfStatement();

    auto body = parseBlockUntil({TokenKind::End});

    expect(TokenKind::End, DiagnosticID::ParseMismatchedBlock,
           "expected 'End Function'");
    expect(TokenKind::Function, DiagnosticID::ParseMismatchedBlock,
           "expected 'End Function'");

    auto d = std::make_unique<FunctionDecl>(loc, access, nameTok.text,
        std::move(params), std::move(returnType), std::move(body), isStatic);
    d->typeParams = std::move(typeParams);
    d->implementsClauses = std::move(clauses);   // B02b
    curTypeParams_.clear();  // G3 护栏窗口只覆盖本模板 params+As+body
    return d;
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
    auto typeParams = parseTypeParams();   // 泛型 (tB): Property Get Foo(Of T)
    auto params = parseParameterList();

    TypeRefPtr returnType;
    if (match(TokenKind::As)) {
        returnType = parseTypeRef();
    }
    std::vector<ImplementsClause> clauses;
    parseTrailingImplementsClauses(clauses);   // B02b: 子句写在 `As Type` 之后
    expectEndOfStatement();

    auto body = parseBlockUntil({TokenKind::End});

    expect(TokenKind::End, DiagnosticID::ParseMismatchedBlock,
           "expected 'End Property'");
    expect(TokenKind::Property, DiagnosticID::ParseMismatchedBlock,
           "expected 'End Property'");

    auto d = std::make_unique<PropertyDecl>(loc, access, propKind,
        nameTok.text, std::move(params), std::move(returnType), std::move(body));
    d->typeParams = std::move(typeParams);
    d->implementsClauses = std::move(clauses);   // B02b
    curTypeParams_.clear();  // G3 护栏窗口只覆盖本模板 params+As+body
    return d;
}

// ============================================================
// Type 声明 (用户自定义类型/UDT)
// ============================================================

std::unique_ptr<TypeDecl> Parser::parseTypeDecl(AccessLevel access) {
    auto loc = currentLoc();
    advance(); // consume 'Type'
    auto nameTok = expectName("expected Type name");
    auto typeParams = parseTypeParams();   // 泛型 (tB): Type Foo(Of T)
    expectEndOfStatement();

    std::vector<std::unique_ptr<TypeMember>> members;
    while (cur_.kind != TokenKind::End && cur_.kind != TokenKind::EndOfFile) {
        skipNewLines();
        if (cur_.kind == TokenKind::End) break;

        auto memberLoc = currentLoc();
        // 允许硬关键字作为 Type 成员名 (如 Next, Type 等)
        std::string memberNameStr;
        if (canBeName(cur_.kind)) {
            memberNameStr = advance().text;
        } else if (!cur_.text.empty() && cur_.kind != TokenKind::EndOfFile &&
                   cur_.kind != TokenKind::NewLine && cur_.kind != TokenKind::Colon &&
                   cur_.kind != TokenKind::LeftParen && cur_.kind != TokenKind::RightParen &&
                   cur_.kind != TokenKind::Comma && cur_.kind != TokenKind::End) {
            memberNameStr = advance().text;
        } else {
            diag_.error(DiagnosticID::ParseExpectedToken, currentLoc(),
                std::string("expected member name (got ") +
                Token::kindToString(cur_.kind) + ")");
            memberNameStr = "?";
        }

        // 可能有数组维度:
        //   memberName(10) As Type    — 固定大小
        //   memberName() As Type      — 动态数组
        //   memberName(1 To 8) As Type — 下界 To 上界
        //   memberName(1, 2) As Type  — 多维
        ExprPtr arraySize;
        bool isArrayDynamic = false;  // Fix 037: 标记动态数组 `()` 语法
        if (match(TokenKind::LeftParen)) {
            if (cur_.kind != TokenKind::RightParen) {
                // 解析第一个维度
                auto first = parseExpression();
                if (match(TokenKind::To)) {
                    // 1 To 8: 只保留上界 (语法检查阶段)
                    auto upper = parseExpression();
                    arraySize = std::move(upper);
                } else {
                    arraySize = std::move(first);
                }
                // 消费后续维度 (多维数组)
                while (match(TokenKind::Comma)) {
                    parseExpression(); // 解析并丢弃后续维度
                    if (match(TokenKind::To)) {
                        parseExpression();
                    }
                }
            } else {
                // 空括号 () = 动态数组, arraySize 保持 nullptr
                isArrayDynamic = true;
            }
            expect(TokenKind::RightParen, DiagnosticID::ParseExpectedToken,
                   "expected ')'");
        }

        TypeRefPtr type;
        if (match(TokenKind::As)) {
            type = parseTypeRef();
        }
        expectEndOfStatement();

        auto memberNode = std::make_unique<TypeMember>(memberLoc,
            memberNameStr, std::move(type), std::move(arraySize));
        memberNode->isArrayDynamic = isArrayDynamic;
        members.push_back(std::move(memberNode));
    }

    expect(TokenKind::End, DiagnosticID::ParseMismatchedBlock,
           "expected 'End Type'");
    expect(TokenKind::Type, DiagnosticID::ParseMismatchedBlock,
           "expected 'End Type'");

    auto d = std::make_unique<TypeDecl>(loc, access, nameTok.text, std::move(members));
    d->typeParams = std::move(typeParams);
    curTypeParams_.clear();  // G3 护栏窗口只覆盖本模板成员类型
    return d;
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

} // namespace vb6c3
