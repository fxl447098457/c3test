// vb6c3 - 语句解析器
// VB6 块语句 + 单行语句

#include "parser/parser.hpp"

namespace vb6c3 {

// --- parser_stmt_assign.cpp: 赋值与声明语句（Set / Let / Call / Dim / ReDim / Const / Static / Erase / RaiseEvent + 标号与调用消歧） ---


// ============================================================
// Set / Let / Call
// ============================================================

std::unique_ptr<SetStmt> Parser::parseSetStmt() {
    auto loc = currentLoc();
    advance(); // consume 'Set'
    // '=' 是赋值号, 不是比较运算符; 用 minBp > '='(l_bp=8) 避免表达式吃掉 '='
    auto target = parseExpression(9);
    expect(TokenKind::Equals, DiagnosticID::ParseExpectedToken,
           "expected '=' in Set statement");
    auto value = parseExpression();
    return std::make_unique<SetStmt>(loc, std::move(target), std::move(value));
}

std::unique_ptr<LetStmt> Parser::parseLetStmt() {
    auto loc = currentLoc();
    advance(); // consume 'Let'
    // 同 Set, '=' 是赋值号
    auto target = parseExpression(9);
    expect(TokenKind::Equals, DiagnosticID::ParseExpectedToken,
           "expected '=' in Let statement");
    auto value = parseExpression();
    return std::make_unique<LetStmt>(loc, std::move(target), std::move(value));
}

std::unique_ptr<CallStmt> Parser::parseCallStmt() {
    auto loc = currentLoc();
    advance(); // consume 'Call'
    auto callee = parseExpression();
    return std::make_unique<CallStmt>(loc, std::move(callee));
}

// ============================================================
// Dim / ReDim / Const / Static (过程体内)
// ============================================================

StmtPtr Parser::parseDimStmt() {
    auto loc = currentLoc();
    advance(); // consume 'Dim'
    auto varDecl = parseVariableDecl(AccessLevel::Private, false);
    // P20: Dim a As Long, b As String — comma-separated multi-variable
    if (cur_.kind != TokenKind::Comma) {
        return std::make_unique<LocalDeclStmt>(loc, std::move(varDecl));
    }
    // Multiple variables: wrap in Block
    StmtList stmts;
    stmts.push_back(std::make_unique<LocalDeclStmt>(loc, std::move(varDecl)));
    while (match(TokenKind::Comma)) {
        // Parse additional variable name As Type
        auto nameTok = expectName("expected variable name");
        // Fix 028: 剥离 VB6 类型后缀 ($%&!#@), 与 parseVariableDecl 保持一致
        auto suffixInfo = stripTypeSuffix(nameTok.text);
        const std::string& varName = suffixInfo.name;
        std::vector<VariableDecl::Dimension> dimensions;
        bool isDynamicArray = false;
        if (match(TokenKind::LeftParen)) {
            if (cur_.kind != TokenKind::RightParen) {
                do {
                    VariableDecl::Dimension dim;
                    auto first = parseExpression();
                    if (match(TokenKind::To)) { dim.lower = std::move(first); dim.upper = parseExpression(); }
                    else { dim.upper = std::move(first); }
                    dimensions.push_back(std::move(dim));
                } while (match(TokenKind::Comma));
            } else { isDynamicArray = true; }
            expect(TokenKind::RightParen, DiagnosticID::ParseExpectedToken, "expected ')'");
        }
        bool isNew = false;
        TypeRefPtr asType;
        if (match(TokenKind::As)) {
            if (match(TokenKind::New)) isNew = true;
            asType = parseTypeRef();
        }
        ExprPtr initializer;
        if (match(TokenKind::Equals)) initializer = parseExpression();
        stmts.push_back(std::make_unique<LocalDeclStmt>(loc,
            std::make_unique<VariableDecl>(loc, AccessLevel::Private, varName,
                false, false, isNew, std::move(asType), std::move(initializer),
                std::move(dimensions), isDynamicArray)));
    }
    return std::make_unique<Block>(loc, std::move(stmts));
}

std::unique_ptr<ReDimStmt> Parser::parseReDimStmt() {
    auto loc = currentLoc();
    bool preserve = false;
    advance(); // consume 'ReDim'
    if (match(TokenKind::Preserve)) {
        preserve = true;
    }

    // 解析变量名, 支持点访问: uOutput.Buffer 和 With块: .Member
    std::string varName;
    if (match(TokenKind::Dot)) {
        varName = ".";
    }
    auto varTok = expectName("expected variable name in ReDim");
    varName += varTok.text;
    while (match(TokenKind::Dot)) {
        if (canBeName(cur_.kind)) {
            varName += "." + advance().text;
        } else if (!cur_.text.empty() && cur_.kind != TokenKind::EndOfFile &&
                   cur_.kind != TokenKind::NewLine && cur_.kind != TokenKind::Colon &&
                   cur_.kind != TokenKind::LeftParen && cur_.kind != TokenKind::RightParen &&
                   cur_.kind != TokenKind::Comma) {
            varName += "." + advance().text;
        } else {
            break;
        }
    }
    expect(TokenKind::LeftParen, DiagnosticID::ParseExpectedToken,
           "expected '(' after ReDim variable");

    std::vector<ReDimStmt::Dimension> dims;
    do {
        ReDimStmt::Dimension dim;
        dim.lower = nullptr;
        dim.upper = parseExpression();
        if (match(TokenKind::To)) {
            dim.lower = std::move(dim.upper);
            dim.upper = parseExpression();
        }
        dims.push_back(std::move(dim));
    } while (match(TokenKind::Comma));

    expect(TokenKind::RightParen, DiagnosticID::ParseExpectedToken,
           "expected ')' after ReDim dimensions");

    TypeRefPtr asType;
    if (match(TokenKind::As)) {
        asType = parseTypeRef();
    }

    return std::make_unique<ReDimStmt>(loc, preserve, varName,
        std::move(dims), std::move(asType));
}

StmtPtr Parser::parseConstStmtInBody() {
    auto loc = currentLoc();
    // 不需要 advance() — parseConstDeclList -> parseConstDecl 会消费 'Const'
    auto decl = parseConstDeclList(AccessLevel::Private);
    return std::make_unique<LocalDeclStmt>(loc, std::move(decl));
}

StmtPtr Parser::parseStaticStmtInBody() {
    auto loc = currentLoc();
    advance(); // consume 'Static'
    // Static x As Long  or  Static Sub ...
    if (cur_.kind == TokenKind::Sub) {
        auto subDecl = parseSubDecl(AccessLevel::Private, true);
        return std::make_unique<LocalDeclStmt>(loc, std::move(subDecl));
    }
    if (cur_.kind == TokenKind::Function) {
        auto funcDecl = parseFunctionDecl(AccessLevel::Private, true);
        return std::make_unique<LocalDeclStmt>(loc, std::move(funcDecl));
    }
    auto varDecl = parseVariableDeclList(AccessLevel::Private, true);
    return std::make_unique<LocalDeclStmt>(loc, std::move(varDecl));
}

StmtPtr Parser::parseAccessDeclInBody() {
    // Public/Private x As Long  (过程体内的声明)
    auto loc = currentLoc();
    AccessLevel access = (cur_.kind == TokenKind::Public)
        ? AccessLevel::Public : AccessLevel::Private;
    advance();
    auto varDecl = parseVariableDeclList(access, false);
    return std::make_unique<LocalDeclStmt>(loc, std::move(varDecl));
}

// ============================================================
// Erase / RaiseEvent
// ============================================================

std::unique_ptr<EraseStmt> Parser::parseEraseStmt() {
    auto loc = currentLoc();
    advance(); // consume 'Erase'
    std::vector<std::string> names;

    // 辅助: 解析一个 Erase 目标, 支持 .Member (With块) 和 obj.Member
    auto parseEraseTarget = [this]() -> std::string {
        std::string name;
        if (match(TokenKind::Dot)) {
            name = ".";
        }
        if (canBeName(cur_.kind)) {
            name += advance().text;
        } else if (!cur_.text.empty() && cur_.kind != TokenKind::EndOfFile &&
                   cur_.kind != TokenKind::NewLine && cur_.kind != TokenKind::Colon &&
                   cur_.kind != TokenKind::Comma) {
            name += advance().text;
        }
        // 支持 obj.Member.Member 链
        while (match(TokenKind::Dot)) {
            if (canBeName(cur_.kind)) {
                name += "." + advance().text;
            } else if (!cur_.text.empty() && cur_.kind != TokenKind::EndOfFile &&
                       cur_.kind != TokenKind::NewLine && cur_.kind != TokenKind::Colon &&
                       cur_.kind != TokenKind::Comma) {
                name += "." + advance().text;
            } else {
                break;
            }
        }
        return name;
    };

    names.push_back(parseEraseTarget());
    while (match(TokenKind::Comma)) {
        names.push_back(parseEraseTarget());
    }
    return std::make_unique<EraseStmt>(loc, std::move(names));
}

std::unique_ptr<RaiseEventStmt> Parser::parseRaiseEventStmt() {
    auto loc = currentLoc();
    advance(); // consume 'RaiseEvent'
    auto nameTok = expectName("expected event name");
    std::vector<ExprPtr> args;
    if (match(TokenKind::LeftParen)) {
        if (cur_.kind != TokenKind::RightParen) {
            do {
                args.push_back(parseExpression());
            } while (match(TokenKind::Comma));
        }
        expect(TokenKind::RightParen, DiagnosticID::ParseExpectedToken,
               "expected ')'");
    }
    return std::make_unique<RaiseEventStmt>(loc, nameTok.text, std::move(args));
}

// ============================================================
// 标签 / 赋值 / 调用 (两可)
// ============================================================

StmtPtr Parser::parseLabelOrAssignmentOrCall() {
    auto loc = currentLoc();

    // 检查是否是标签: Name 后面紧跟冒号
    // 在语句起始位置, identifier: 只能是标签（VB6 规则）
    // 但在单行 If 内, colon 是语句分隔符 (If x Then a: b: c)
    if (inSingleLineIf_ == 0 && canBeName(cur_.kind) && next_.kind == TokenKind::Colon) {
        auto nameTok = advance();  // consume label name
        advance();                 // consume ':'
        return std::make_unique<LabelStmt>(loc, nameTok.text);
    }

    // 解析左值/调用目标表达式
    // 在语句级, 顶层的 = 是赋值而非比较运算符。
    // 使用 minBp=9 (> = 的 l_bp=8) 阻止 = 被消费为比较运算符,
    // 同时允许 +,-,*,/,& 等运算符在目标内出现 (如 arr(i+1))。
    auto expr = parseExpression(9);

    // Fix 092r: Debug.Assert 的条件表达式可含顶层 '=' 比较:
    //   Debug.Assert (lSig And &HFF&) = (&H201 And &HFF&)
    // 上面 parseExpression(9) 已吃掉 `Debug.Assert (<cond>)`, 若这里再按赋值处理就会
    // 生成 `vb6_DebugAssert(<cond>) = <rhs>` → C2186 ("=" 左侧是 void, ToolsTlsThunks 4733).
    // 故先把 '=' 右侧并回断言条件, 再由调用路径生成 vb6_DebugAssert(<cond>)。
    if (cur_.kind == TokenKind::Equals && expr->kind == ASTNodeKind::IndexOrCallExpr) {
        auto& dba092r = static_cast<IndexOrCallExpr&>(*expr);
        bool isDebugAssert092r = false;
        if (dba092r.named.empty() && dba092r.positional.size() == 1 && dba092r.callee
            && dba092r.callee->kind == ASTNodeKind::MemberAccessExpr) {
            auto& dbaMa092r = static_cast<MemberAccessExpr&>(*dba092r.callee);
            if (dbaMa092r.object && dbaMa092r.object->kind == ASTNodeKind::IdentifierExpr) {
                auto& dbaObj092r = static_cast<IdentifierExpr&>(*dbaMa092r.object);
                isDebugAssert092r = toLower(dbaObj092r.name) == "debug"
                    && toLower(dbaMa092r.memberName) == "assert";
            }
        }
        if (isDebugAssert092r) {
            advance();  // consume '='
            auto rhs092r = parseExpression();
            auto cond092r = std::make_unique<BinaryExpr>(loc, BinaryOp::Eq,
                std::move(dba092r.positional[0]), std::move(rhs092r));
            dba092r.positional.clear();
            dba092r.positional.push_back(std::move(cond092r));
        }
    }

    // 检查是否是赋值
    if (match(TokenKind::Equals)) {
        auto value = parseExpression();  // 右值: = 是比较, 完整解析
        if (!value) {
            diag_.error(DiagnosticID::ParseExpectedExpression, loc,
                "赋值右值为空 (cur=" + std::string(Token::kindToString(cur_.kind)) + ")");
        }
        return std::make_unique<AssignmentStmt>(loc, std::move(expr), std::move(value));
    }

    // P15.6: 检测 Debug.Print/Debug.Assert (后续的-应为一元负号而非中缀减法)
    auto isDebugPrint = false;
    if (expr->kind == ASTNodeKind::MemberAccessExpr) {
        auto& ma = static_cast<MemberAccessExpr&>(*expr);
        if (ma.object->kind == ASTNodeKind::IdentifierExpr) {
            auto& obj = static_cast<IdentifierExpr&>(*ma.object);
            std::string objL = toLower(obj.name);
            std::string memL = toLower(ma.memberName);
            if (objL == "debug" && (memL == "print" || memL == "assert")) {
                isDebugPrint = true;
            }
        }
    }

    // VB6 无括号调用: Sub arg1, arg2 / Debug.Print "text"
    // 如果表达式后还有同一行的 token (非 NewLine/Colon/EndOfFile),
    // 且不是中缀运算符 (但前缀运算符如 - 可开始新参数), 则视为无括号调用的参数列表
    if (cur_.kind != TokenKind::NewLine && cur_.kind != TokenKind::Colon &&
        cur_.kind != TokenKind::EndOfFile && 
        (!isInfixOperator(cur_.kind) || isPrefixOperator(cur_.kind) || isDebugPrint)) {
        // 将表达式包装为 IndexOrCallExpr, 追加参数
        auto call = std::make_unique<IndexOrCallExpr>(loc, std::move(expr));

        // 解析参数列表
        // VB6 中逗号和分号都分隔参数 (分号是 Print 的位置修饰符)
        // MsgBox arg1, arg2, arg3        — 逗号分隔
        // Debug.Print "text"; i         — 分号分隔 (紧跟下一个参数)
        // Debug.Print "text";           — 末尾分号 (抑制换行, 无后续参数)

        // 辅助: 判断当前 token 是否可以开始一个表达式 (但不是语句结束符/中缀运算符)
        // 注意: 逗号后的 - 可以是一元负号 (前缀运算符), 需要允许
        auto canStartArg = [this]() -> bool {
            return cur_.kind != TokenKind::NewLine &&
                   cur_.kind != TokenKind::Colon &&
                   cur_.kind != TokenKind::EndOfFile &&
                   (!isInfixOperator(cur_.kind) || isPrefixOperator(cur_.kind)) &&
                   cur_.kind != TokenKind::Comma &&
                   cur_.kind != TokenKind::Semicolon;
        };

        // 第一个参数
        // Fix 073: 无括号调用路径也需记录 ByVal 覆盖到 byvalOverrides,
        // 与 parser_expr.cpp 带括号调用路径一致.
        size_t stmtArgIndex = 0;
        if (cur_.kind == TokenKind::ByVal || cur_.kind == TokenKind::ByRef) {
            if (cur_.kind == TokenKind::ByVal) {
                call->byvalOverrides.insert(stmtArgIndex);
            }
            advance(); // 消费 ByVal/ByRef
        }
        if (canStartArg()) {
            // 命名参数?  name := value
            if (canBeName(cur_.kind) && next_.kind == TokenKind::Assign) {
                auto nameTok = advance(); // name
                advance(); // consume ':='
                auto val = parseExpression();
                call->named.push_back({nameTok.text, std::move(val)});
            } else {
                call->positional.push_back(parseExpression());
            }
        }
        stmtArgIndex++;

        // 后续参数: 逗号或分号后继续
        while (true) {
            // 跳过分号 (; 在 Print 语句中是位置修饰符, 后面可能还有参数)
            bool sawSemi = false;
            while (cur_.kind == TokenKind::Semicolon) {
                advance();
                sawSemi = true;
            }
            // 逗号分隔 -> 继续解析下一个参数
            if (match(TokenKind::Comma)) {
                // ByVal/ByRef 前缀
                if (cur_.kind == TokenKind::ByVal || cur_.kind == TokenKind::ByRef) {
                    if (cur_.kind == TokenKind::ByVal) {
                        call->byvalOverrides.insert(stmtArgIndex);
                    }
                    advance();
                }
                if (canStartArg()) {
                    // 命名参数?  name := value
                    if (canBeName(cur_.kind) && next_.kind == TokenKind::Assign) {
                        auto nameTok = advance();
                        advance(); // ':='
                        auto val = parseExpression();
                        call->named.push_back({nameTok.text, std::move(val)});
                    } else {
                        call->positional.push_back(parseExpression());
                    }
                }
                stmtArgIndex++;
                continue;
            }
            // 分号后跟着表达式 -> 作为下一个参数
            if (sawSemi && canStartArg()) {
                call->positional.push_back(parseExpression());
                continue;
            }
            // 没有更多参数
            break;
        }

        return std::make_unique<CallStmt>(loc, std::move(call));
    }

    // 否则就是调用语句 (可能带括号也可能不带)
    return std::make_unique<CallStmt>(loc, std::move(expr));
}

} // namespace vb6c3
