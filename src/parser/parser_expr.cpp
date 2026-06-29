// vb6c3 - Pratt 表达式解析器
// 参考: RustASP 45行核心实现, (l_bp, r_bp) 绑定力对设计
// VB6 14级优先级, 右结合 ^ 运算符

#include "parser/parser.hpp"
#include <algorithm>
#include <cctype>

namespace vb6c3 {

// ============================================================
// Pratt 核心: parseExpression(minBp)
//
// 原理:
//   1. 解析一个 null denotation (前缀/原子表达式)
//   2. 循环: 如果下一个是中缀运算符, 且其 l_bp >= minBp
//      - 消费运算符
//      - 递归解析右操作数 (r_bp 作为新的 minBp)
//      - 构造 BinaryExpr 节点
//   3. 返回表达式
//
// 关键: 右结合运算符 (如 ^) 的 l_bp > r_bp
//   a ^ b ^ c 解析为 a ^ (b ^ c)
//   因为第二次遇到 ^ 时, l_bp(21) >= minBp(20), 进入循环
//   递归时 minBp = r_bp(20), 所以 c 被归入内层
// ============================================================

ExprPtr Parser::parseExpression() {
    return parseExpression(0);  // 最低优先级, 接受所有运算符
}

ExprPtr Parser::parseExpression(int minBp) {
    // Step 1: 解析 null denotation (前缀/原子)
    auto left = parseNullDenotation();
    if (!left) {
        return left;
    }

    // Step 2: 循环处理中缀/后缀运算符
    while (true) {
        auto bp = getBindingPower(cur_.kind);
        if (bp.l_bp < minBp) {
            break;  // 运算符优先级不够, 退出循环
        }

        // 非运算符 token (如 NewLine, EndOfFile, 标识符等):
        // 绑定力为 {0,0}, 当 minBp=0 时 0 < 0 为 false 会误入循环.
        // 若既非中缀也非后缀起始, 应直接退出, 避免 parseLeftDenotation
        // 将 left move 走后返回 nullptr 导致有效表达式丢失.
        if (bp.l_bp == 0 && bp.r_bp == 0 &&
            cur_.kind != TokenKind::Dot &&
            cur_.kind != TokenKind::LeftParen &&
            cur_.kind != TokenKind::Exclamation) {
            break;
        }

        left = parseLeftDenotation(std::move(left), minBp);
        if (!left) {
            break;
        }
    }

    return left;
}

// ============================================================
// Null Denotation: 前缀/原子表达式
// ============================================================

ExprPtr Parser::parseNullDenotation() {
    switch (cur_.kind) {
        // --- 字面量 ---
        case TokenKind::IntegerLiteral:
        case TokenKind::LongLiteral:
        case TokenKind::FloatLiteral:
        case TokenKind::DecimalLiteral:
        case TokenKind::StringLiteral:
        case TokenKind::DateLiteral:
        case TokenKind::TrueKeyword:
        case TokenKind::FalseKeyword:
        case TokenKind::NothingKeyword:
        case TokenKind::EmptyKeyword:
        case TokenKind::NullKeyword:
            return parseLiteral();

        // --- 标识符 / 函数调用 ---
        case TokenKind::Identifier:
            return parseIdentifierOrCall();

        // --- 括号表达式 ---
        case TokenKind::LeftParen:
            return parseParenthesizedExpr();

        // --- 前缀运算符 ---
        case TokenKind::Minus: {
            auto loc = currentLoc();
            advance(); // consume '-'
            // 一元负: r_bp 用于控制绑定范围
            // -a + b → (-a) + b  (一元负比加号绑定更紧)
            auto operand = parseExpression(19);  // 与 * 同级 r_bp
            if (!operand) {
                diag_.error(DiagnosticID::ParseExpectedExpression, loc,
                    "expected expression after unary '-'");
                return nullptr;
            }
            return std::make_unique<UnaryExpr>(loc, UnaryOp::Negate, std::move(operand));
        }

        case TokenKind::Not: {
            auto loc = currentLoc();
            advance(); // consume 'Not'
            // Not 优先级低于 And (r_bp=5)
            auto operand = parseExpression(5);
            if (!operand) {
                diag_.error(DiagnosticID::ParseExpectedExpression, loc,
                    "expected expression after 'Not'");
                return nullptr;
            }
            return std::make_unique<UnaryExpr>(loc, UnaryOp::Not, std::move(operand));
        }

        // --- New 表达式 ---
        case TokenKind::New:
            return parseNewExpr();

        // --- TypeOf 表达式 ---
        case TokenKind::TypeOf:
            return parseTypeOfExpr();

        // --- AddressOf 表达式 ---
        case TokenKind::AddressOf:
            return parseAddressOfExpr();

        // --- Me 表达式 ---
        case TokenKind::MeKeyword:
            return parseMeExpr();

        // --- With 块中的 .Member ---
        case TokenKind::Dot:
            if (withDepth_ > 0) {
                return parseWithMemberExpr();
            }
            // 不在 With 块内的 . 是错误
            diag_.error(DiagnosticID::ParseUnexpectedToken, currentLoc(),
                "'.' outside of With block");
            advance();
            return nullptr;

        // --- 字典访问 obj!key ---
        case TokenKind::Exclamation: {
            // 这样写在表达式开头不太合理, 但容错
            auto loc = currentLoc();
            advance();
            auto key = expectName("expected identifier after '!'");
            return std::make_unique<DictionaryAccessExpr>(loc, nullptr, key.text);
        }

                // --- Input$() 函数 (P15.4) ---
        case TokenKind::Input: {
            // Input 后跟 '(' -> Input$ function; otherwise not an expression
            auto loc = currentLoc();
            advance(); // consume 'Input'
            // skip optional $ suffix
            if (cur_.kind == TokenKind::Dollar) advance();
            // must be followed by '('
            if (cur_.kind == TokenKind::LeftParen) {
                advance(); // consume '('
                auto call = std::make_unique<IndexOrCallExpr>(loc,
                    std::make_unique<IdentifierExpr>(loc, "Input$"));
                // parse argument list
                if (cur_.kind != TokenKind::RightParen) {
                    do {
                        auto arg = parseExpression();
                        call->positional.push_back(std::move(arg));
                    } while (match(TokenKind::Comma));
                }
                expect(TokenKind::RightParen, DiagnosticID::ParseExpectedToken, "expected ')'");
                return call;
            }
            diag_.error(DiagnosticID::ParseExpectedExpression, loc,
                "expected '(' after Input$ function");
            return nullptr;
        }


        default:
            // 软关键字在表达式位置 → 解析为标识符
            if (isSoftKeyword(cur_.kind)) {
                return parseIdentifierOrCall();
            }
            diag_.error(DiagnosticID::ParseExpectedExpression, currentLoc(),
                "expected expression, got " + std::string(Token::kindToString(cur_.kind)));
            return nullptr;
    }
}

// ============================================================
// Left Denotation: 中缀/后缀表达式
// ============================================================

ExprPtr Parser::parseLeftDenotation(ExprPtr left, int& minBp) {
    auto kind = cur_.kind;
    auto bp = getBindingPower(kind);

    if (bp.l_bp == 0 && bp.r_bp == 0) {
        // 不是中缀运算符, 检查后缀
        // 后缀: .member, (args), !dict
        if (kind == TokenKind::Dot || kind == TokenKind::LeftParen ||
            kind == TokenKind::Exclamation) {
            return parsePostfix(std::move(left));
        }
        // 不是中缀也不是后缀, 退出循环
        return nullptr;
    }

    // 中缀二元运算符
    auto loc = left->loc;
    auto op = tokenToBinaryOp(kind);
    advance(); // consume operator

    // 特殊处理: Is 运算符在 Case Is 中有不同语义
    // (由 parseCaseValue 单独处理, 此处按普通二元运算符)

    auto right = parseExpression(bp.r_bp);
    if (!right) {
        diag_.error(DiagnosticID::ParseExpectedExpression, currentLoc(),
            "expected expression after binary operator");
        // 尽量恢复: 返回左操作数
        return left;
    }

    return std::make_unique<BinaryExpr>(loc, op, std::move(left), std::move(right));
}

// ============================================================
// 原子表达式
// ============================================================

ExprPtr Parser::parseLiteral() {
    auto loc = currentLoc();
    auto tok = advance();

    switch (tok.kind) {
        case TokenKind::IntegerLiteral: {
            auto expr = std::make_unique<LiteralExpr>(loc, LiteralKind::Integer, tok.text);
            expr->intValue = static_cast<int32_t>(tok.intValue);
            return expr;
        }
        case TokenKind::LongLiteral: {
            auto expr = std::make_unique<LiteralExpr>(loc, LiteralKind::Long, tok.text);
            expr->longValue = tok.longValue;
            return expr;
        }
        case TokenKind::FloatLiteral: {
            auto expr = std::make_unique<LiteralExpr>(loc, LiteralKind::Double, tok.text);
            expr->doubleValue = tok.doubleValue;
            return expr;
        }
        case TokenKind::DecimalLiteral: {
            auto expr = std::make_unique<LiteralExpr>(loc, LiteralKind::Decimal, tok.text);
            expr->doubleValue = tok.doubleValue;
            return expr;
        }
        case TokenKind::StringLiteral:
            return std::make_unique<LiteralExpr>(loc, LiteralKind::String, tok.text);
        case TokenKind::DateLiteral:
            return std::make_unique<LiteralExpr>(loc, LiteralKind::Date, tok.text);
        case TokenKind::TrueKeyword: {
            auto expr = std::make_unique<LiteralExpr>(loc, LiteralKind::Boolean, tok.text);
            expr->boolValue = true;
            return expr;
        }
        case TokenKind::FalseKeyword: {
            auto expr = std::make_unique<LiteralExpr>(loc, LiteralKind::Boolean, tok.text);
            expr->boolValue = false;
            return expr;
        }
        case TokenKind::NothingKeyword:
            return std::make_unique<LiteralExpr>(loc, LiteralKind::Nothing, tok.text);
        case TokenKind::EmptyKeyword:
            return std::make_unique<LiteralExpr>(loc, LiteralKind::Empty, tok.text);
        case TokenKind::NullKeyword:
            return std::make_unique<LiteralExpr>(loc, LiteralKind::Null, tok.text);
        default:
            diag_.error(DiagnosticID::ParseUnexpectedToken, loc,
                "expected literal");
            return nullptr;
    }
}

ExprPtr Parser::parseIdentifierOrCall() {
    auto loc = currentLoc();
    auto nameTok = advance();
    auto expr = std::make_unique<IdentifierExpr>(loc, nameTok.text);

    // 后缀处理: 可能是函数调用 arr(i) 或 func(x,y)
    return parsePostfix(std::move(expr));
}

ExprPtr Parser::parseParenthesizedExpr() {
    auto loc = currentLoc();
    advance(); // consume '('

    // VB6: 空括号 () 可能是数组声明或无参调用, 但在表达式上下文
    // 这里处理的是 (expr) 分组
    auto expr = parseExpression();
    expect(TokenKind::RightParen, DiagnosticID::ParseExpectedToken,
           "expected ')'");
    return expr;
}

ExprPtr Parser::parseNewExpr() {
    auto loc = currentLoc();
    advance(); // consume 'New'
    auto classTok = expectName("expected class name after 'New'");
    return std::make_unique<NewExpr>(loc, classTok.text);
}

ExprPtr Parser::parseTypeOfExpr() {
    auto loc = currentLoc();
    advance(); // consume 'TypeOf'
    auto obj = parseExpression(23);  // TypeOf 比 Is 优先级低
    // 词法器将Is输出为IsKeyword，两者都接受
    if (cur_.kind != TokenKind::Is && cur_.kind != TokenKind::IsKeyword) {
        diag_.error(DiagnosticID::ParseExpectedToken, currentLoc(),
                    "expected 'Is' after 'TypeOf'");
    } else {
        advance();
    }
    auto typeTok = expectName("expected type name");
    return std::make_unique<TypeOfExpr>(loc, std::move(obj), typeTok.text);
}

ExprPtr Parser::parseAddressOfExpr() {
    auto loc = currentLoc();
    advance(); // consume 'AddressOf'
    auto funcTok = expectName("expected function name after 'AddressOf'");
    return std::make_unique<AddressOfExpr>(loc, funcTok.text);
}

ExprPtr Parser::parseMeExpr() {
    auto loc = currentLoc();
    advance(); // consume 'Me'

    auto expr = std::make_unique<MeExpr>(loc);
    return parsePostfix(std::move(expr));
}

ExprPtr Parser::parseWithMemberExpr() {
    auto loc = currentLoc();
    advance(); // consume '.'
    auto memberTok = expectName("expected member name after '.'");
    auto expr = std::make_unique<WithMemberExpr>(loc, memberTok.text);
    return parsePostfix(std::move(expr));
}

// ============================================================
// 后缀表达式: .member, (args), !dict
// ============================================================

ExprPtr Parser::parsePostfix(ExprPtr expr) {
    while (true) {
        switch (cur_.kind) {
            case TokenKind::Dot: {
                auto loc = currentLoc();
                // P17.1: In With context, Debug.Print .Member should parse .Member
                // as WithMemberExpr argument, not chain MemberAccessExpr
                if (withDepth_ > 0 && expr->kind == ASTNodeKind::MemberAccessExpr) {
                    auto& ma = static_cast<MemberAccessExpr&>(*expr);
                    if (ma.object && ma.object->kind == ASTNodeKind::IdentifierExpr) {
                        auto& obj = static_cast<IdentifierExpr&>(*ma.object);
                        std::string objL = toLower(obj.name);
                        std::string memL = toLower(ma.memberName);
                        if (objL == "debug" && (memL == "print" || memL == "assert")) {
                            return expr;  // stop postfix, let arg parser handle .Member
                        }
                    }
                }
                advance(); // consume '.'
                auto member = expectName("expected member name after '.'");
                expr = std::make_unique<MemberAccessExpr>(
                    loc, std::move(expr), member.text);
                break;
            }

            case TokenKind::LeftParen: {
                // VB6 不区分数组索引和函数调用 → IndexOrCallExpr
                auto loc = currentLoc();
                advance(); // consume '('

                auto call = std::make_unique<IndexOrCallExpr>(loc, std::move(expr));

                // 解析参数列表
                if (cur_.kind != TokenKind::RightParen) {
                    do {
                        skipNewLines();

                        // 命名参数?  name := value (允许软关键字作参数名)
                        if (canBeName(cur_.kind) &&
                            next_.kind == TokenKind::Assign) {
                            // 命名参数
                            auto nameTok = advance(); // name
                            advance(); // consume ':='
                            auto val = parseExpression();
                            call->named.push_back({nameTok.text, std::move(val)});
                        } else {
                            // 位置参数
                            auto arg = parseExpression();
                            call->positional.push_back(std::move(arg));
                        }

                        skipNewLines();
                    } while (match(TokenKind::Comma));
                }

                expect(TokenKind::RightParen, DiagnosticID::ParseExpectedToken,
                       "expected ')'");
                expr = std::move(call);
                break;
            }

            case TokenKind::Exclamation: {
                // 字典访问: expr!key
                auto loc = currentLoc();
                advance(); // consume '!'
                auto key = expectName("expected identifier after '!'");
                expr = std::make_unique<DictionaryAccessExpr>(
                    loc, std::move(expr), key.text);
                break;
            }

            default:
                return expr;  // 没有更多后缀
        }
    }
}

// ============================================================
// Case 值解析 (Select Case 专用)
// ============================================================

CaseClause::CaseValue Parser::parseCaseValue() {
    CaseClause::CaseValue cv;

    // Case Is > 0 形式
    if (cur_.kind == TokenKind::IsKeyword || cur_.kind == TokenKind::Is) {
        advance(); // consume 'Is'
        cv.isIsClause = true;

        // 比较运算符
        if (checkAny({TokenKind::LessThan, TokenKind::GreaterThan,
                      TokenKind::LessEqual, TokenKind::GreaterEqual,
                      TokenKind::Equals, TokenKind::NotEquals})) {
            // 将比较运算符和右操作数合并为一个表达式
            // 例如: Is > 0 → BinaryExpr(IdentifierExpr("Is"), Gt, LiteralExpr(0))
            auto isExpr = std::make_unique<IdentifierExpr>(currentLoc(), "Is");
            auto bp = getBindingPower(cur_.kind);
            cv.value = parseLeftDenotation(std::move(isExpr), bp.l_bp);
        } else {
            // Case Is (无比较符) → 标识符值
            cv.value = std::make_unique<IdentifierExpr>(currentLoc(), "Is");
        }
    } else {
        // 普通值或范围
        cv.value = parseExpression();

        // Case 1 To 10 范围形式
        if (match(TokenKind::To)) {
            cv.toValue = parseExpression();
        }
    }

    return cv;
}

} // namespace vb6c3
