// vb6c3 - Pratt 表达式解析器 — 后缀表达式 + Case 值解析
// 由 src/parser/parser_expr.cpp 拆出（2026-09-17），纯搬移、零行为改动。

#include "parser/parser.hpp"
#include <algorithm>
#include <cctype>

namespace vb6c3 {


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
        // Fix 043c: In With context, ".Member1 .Member2" (space before 2nd dot)
        // is a sub call: .Member1(.Member2), NOT member access .Member1.Member2.
        // Detect space by comparing prevTok_ end column with cur_ start column.
        if (withDepth_ > 0 && expr->kind == ASTNodeKind::WithMemberExpr &&
            prevTok_.line == cur_.line &&
            prevTok_.column + prevTok_.length < cur_.column) {
            return expr;  // space detected — let caller parse .Member2 as argument
        }
        // Fix 077: In With context, "SubName .Field" (space before dot) is a bare
        // sub call where .Field is a With-block member access used as argument,
        // NOT a member access SubName.Field.  E.g.:
        //   pvAppendBitsToBuffer .Mode, 4, baQrCode, lBitLen
        // Without this, parser creates MemberAccessExpr(pvAppendBitsToBuffer, Mode)
        // instead of IdentifierExpr(pvAppendBitsToBuffer) + WithMemberExpr(.Mode).
        if (withDepth_ > 0 && expr->kind == ASTNodeKind::IdentifierExpr &&
            prevTok_.line == cur_.line &&
            prevTok_.column + prevTok_.length < cur_.column) {
            return expr;  // space detected — .XXX is a With-member argument, not member access
        }
        // Fix 099: 同 Fix 077, 扩展到无括号方法调用形态 "obj.Method .Field" —
        // obj.Method 后空格跟 .Field 时, .Field 是该调用的 With 块实参
        // (VB6 sub 风格调用: 实参不带括号, 以空格分隔), 而非成员链
        // obj.Method.Rs. 否则 "Users.Decode .Rs" 被解析成
        // MemberAccessExpr(MemberAccessExpr(U,Decode),Rs) → cgen 生成
        // ComCall(ComGetObjectProp(U,L"Decode"), L"Rs", ...) — .Rs 变成
        // 对 Decode 结果的链式 COM 调用且实参丢失 → C2198+C2039
        // (Demo.bas Db2: Users.Decode .Rs, VB6 实际语义 = Users.Decode(.Rs)).
        if (withDepth_ > 0 && expr->kind == ASTNodeKind::MemberAccessExpr &&
            prevTok_.line == cur_.line &&
            prevTok_.column + prevTok_.length < cur_.column) {
            return expr;  // space detected — .Field is a With-member argument to the obj.Method call
        }
        advance(); // consume '.'
        // VB6 允许关键字作为成员名: obj.Type, obj.Loop, etc.
        // expectName 只接受 Identifier 和软关键字, 这里扩展为接受所有带文本的 token
        std::string memberName;
        if (canBeName(cur_.kind)) {
            memberName = advance().text;
        } else if (!cur_.text.empty() && cur_.kind != TokenKind::EndOfFile &&
                   cur_.kind != TokenKind::NewLine && cur_.kind != TokenKind::Colon &&
                   cur_.kind != TokenKind::LeftParen && cur_.kind != TokenKind::RightParen &&
                   cur_.kind != TokenKind::Comma) {
            // 硬关键字也可作为成员名 (如 Type, Loop, Next 等)
            memberName = advance().text;
        } else {
            diag_.error(DiagnosticID::ParseExpectedToken, currentLoc(),
                std::string("expected member name after '.' (got ") +
                Token::kindToString(cur_.kind) + ")");
            memberName = "?";
        }
        expr = std::make_unique<MemberAccessExpr>(
            loc, std::move(expr), memberName);
        break;
    }

            case TokenKind::LeftParen: {
                // VB6 不区分数组索引和函数调用 → IndexOrCallExpr
                auto loc = currentLoc();
                advance(); // consume '('

                auto call = std::make_unique<IndexOrCallExpr>(loc, std::move(expr));

                // 解析参数列表
                if (cur_.kind != TokenKind::RightParen) {
                    size_t argIndex = 0;
                    do {
                        skipNewLines();

                        // VB6 允许在调用时覆盖传递方式: MyFunc(ByVal arg)
                        bool hasByValOverride = false;
                        if (cur_.kind == TokenKind::ByVal) {
                            advance(); // consume ByVal
                            hasByValOverride = true;
                        } else if (cur_.kind == TokenKind::ByRef) {
                            advance(); // consume ByRef
                        }

                        // 命名参数?  name := value (允许软关键字作参数名)
                        if (canBeName(cur_.kind) &&
                            next_.kind == TokenKind::Assign) {
                            // 命名参数
                            auto nameTok = advance(); // name
                            advance(); // consume ':='
                            auto val = parseExpression();
                            call->named.push_back({nameTok.text, std::move(val)});
                        } else if (cur_.kind == TokenKind::Comma || cur_.kind == TokenKind::RightParen) {
                            // M22: 空参数占位 - VB6允许 MsgBox("hi", , "title")
                            auto _ph = std::make_unique<LiteralExpr>(currentLoc(), LiteralKind::Long, "0");
                            _ph->longValue = 0;  // Union与intValue共享内存, 必须显式设置longValue
                            call->positional.push_back(std::move(_ph));
                        } else {
                            // 位置参数
                            auto arg = parseExpression();
                            call->positional.push_back(std::move(arg));
                        }

                        // Fix 072: 记录 ByVal 覆盖的参数索引
                        if (hasByValOverride) {
                            call->byvalOverrides.insert(argIndex);
                        }

                        argIndex++;
                        skipNewLines();
                    } while (match(TokenKind::Comma));
                }

                expect(TokenKind::RightParen, DiagnosticID::ParseExpectedToken,
                       "expected ')'");

                // Fix 102: VB6 图形方法坐标语法
                //   obj.Line (x1, y1)-(x2, y2)[, color][, BF | B | F]
                // `(x1, y1)` 已按普通实参表解析完毕; 紧随的 `-(x2, y2)` 必须在此吸收,
                // 否则外层 parseExpression 会把它当作中缀减法, 而右操作数 `(x2, y2)`
                // 的括号内含逗号 → "expected ')'", 整行解析崩坏并连锁破坏其后的
                // If/End If 配对 (Charts 2020 ppProgressCircular.pag 297/299/474).
                // 吸收后统一为 IndexOrCallExpr(callee=obj.Line,
                // 实参 = x1, y1, x2, y2[, color][, fillMode]), 由后端按控件类型发射。
                if (cur_.kind == TokenKind::Minus && next_.kind == TokenKind::LeftParen) {
                    bool isLineCall = false;
                    if (call->callee && call->callee->kind == ASTNodeKind::MemberAccessExpr) {
                        auto& maLine = static_cast<MemberAccessExpr&>(*call->callee);
                        isLineCall = toLower(maLine.memberName) == "line";
                    }
                    if (isLineCall) {
                        advance();  // 消费 '-'
                        advance();  // 消费 '('
                        auto x2 = parseExpression();
                        expect(TokenKind::Comma, DiagnosticID::ParseExpectedToken,
                               "expected ',' in Line (x1,y1)-(x2,y2)");
                        auto y2 = parseExpression();
                        expect(TokenKind::RightParen, DiagnosticID::ParseExpectedToken,
                               "expected ')' in Line (x1,y1)-(x2,y2)");
                        call->positional.push_back(std::move(x2));
                        call->positional.push_back(std::move(y2));
                        // 可选后续参数: ", color" / ", color, BF|B|F"
                        while (match(TokenKind::Comma)) {
                            if (cur_.kind == TokenKind::NewLine ||
                                cur_.kind == TokenKind::Colon ||
                                cur_.kind == TokenKind::EndOfFile) {
                                break;
                            }
                            call->positional.push_back(parseExpression());
                        }
                    }
                }

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
