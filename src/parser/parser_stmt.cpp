// vb6c3 - 语句解析器
// VB6 块语句 + 单行语句

#include "parser/parser.hpp"

namespace vb6c3 {

// ============================================================
// 语句分发
// ============================================================

StmtPtr Parser::parseStatement() {
    switch (cur_.kind) {
        // --- 块语句 ---
        case TokenKind::If:       return parseIfStmt();
        case TokenKind::For:      return parseForOrForEach();
        case TokenKind::Do:       return parseDoLoopStmt();
        case TokenKind::While:    return parseWhileWendStmt();
        case TokenKind::Select:   return parseSelectCaseStmt();
        case TokenKind::With:     return parseWithStmt();

        // --- 跳转语句 ---
        case TokenKind::GoTo:     return parseGoToStmt();
        case TokenKind::GoSub:    return parseGoSubStmt();
        case TokenKind::Return:   return parseReturnStmt();
        case TokenKind::Exit:     return parseExitStmt();
        case TokenKind::Stop:     return parseStopStmt();
        case TokenKind::End: {
            // End 本身 (终止程序) vs End If/Sub/Function/... (块终止)
            if (isEndBlock()) {
                // 块终止符, 不应该在这里解析, 返回 nullptr 让上层处理
                return nullptr;
            }
            return parseEndStmt();
        }

        // --- On Error / On GoTo ---

        // --- P18-A: Mid$ statement (assignment) ---
        case TokenKind::Mid:
            if (next_.kind == TokenKind::LeftParen || next_.kind == TokenKind::Dollar) {
                return parseMidStmt();
            }
            return parseLabelOrAssignmentOrCall();

        case TokenKind::On:       return parseOnStmt();

        // --- P14.1.2: Resume --
        case TokenKind::Resume:   return parseResumeStmt();

        // --- P14.1.3: Error --
        case TokenKind::Error: {
            if (next_.kind == TokenKind::Equals || next_.kind == TokenKind::LeftParen) {
                return parseLabelOrAssignmentOrCall();
            }
            return parseErrorStmt();
        }

        // --- 赋值/调用 ---
        case TokenKind::Set:      return parseSetStmt();
        case TokenKind::Let:      return parseLetStmt();
        case TokenKind::Call:     return parseCallStmt();

        // --- 声明 (可出现在过程体内) ---
        case TokenKind::Dim:      return parseDimStmt();
        case TokenKind::ReDim:    return parseReDimStmt();
        case TokenKind::Const:    return parseConstStmtInBody();
        case TokenKind::Static:   return parseStaticStmtInBody();
        case TokenKind::Public:
        case TokenKind::Private:  return parseAccessDeclInBody();

        // --- 文件 I/O ---
        // P12.6: 双用关键字消歧 — 文件I/O语句 vs 变量名赋值/调用
        // 如果后跟 = 或 ( 或 . , 视为标识符(赋值/调用), 否则作为I/O语句
        case TokenKind::Get: {
            if (next_.kind == TokenKind::Equals ||
                next_.kind == TokenKind::LeftParen ||
                next_.kind == TokenKind::Dot) {
                return parseLabelOrAssignmentOrCall();
            }
            return parseGetStmt();
        }
        case TokenKind::Put: {
            if (next_.kind == TokenKind::Equals ||
                next_.kind == TokenKind::LeftParen ||
                next_.kind == TokenKind::Dot) {
                return parseLabelOrAssignmentOrCall();
            }
            return parsePutStmt();
        }
        case TokenKind::Input: {
            if (next_.kind == TokenKind::Equals ||
                next_.kind == TokenKind::LeftParen ||
                next_.kind == TokenKind::Dot) {
                return parseLabelOrAssignmentOrCall();
            }
            return parseInputStmt();
        }
        case TokenKind::Print: {
            if (next_.kind == TokenKind::Equals) {
                return parseLabelOrAssignmentOrCall();
            }
            return parsePrintStmt();
        }
        case TokenKind::Write: {
            if (next_.kind == TokenKind::Equals) {
                return parseLabelOrAssignmentOrCall();
            }
            return parseWriteStmt();
        }
        case TokenKind::Open: {
            if (next_.kind == TokenKind::Equals ||
                next_.kind == TokenKind::LeftParen ||
                next_.kind == TokenKind::Dot) {
                return parseLabelOrAssignmentOrCall();
            }
            return parseOpenStmt();
        }
        case TokenKind::Close: {
            if (next_.kind == TokenKind::Equals ||
                next_.kind == TokenKind::LeftParen ||
                next_.kind == TokenKind::Dot) {
                return parseLabelOrAssignmentOrCall();
            }
            return parseCloseStmt();
        }
        case TokenKind::Line: {
            if (next_.kind == TokenKind::Equals ||
                next_.kind == TokenKind::LeftParen ||
                next_.kind == TokenKind::Dot) {
                return parseLabelOrAssignmentOrCall();
            }
            return parseLineInputStmt();
        }
        case TokenKind::Width: {
            if (next_.kind == TokenKind::Equals ||
                next_.kind == TokenKind::LeftParen ||
                next_.kind == TokenKind::Dot) {
                return parseLabelOrAssignmentOrCall();
            }
            return parseWidthStmt();
        }
        case TokenKind::Seek: {
            if (next_.kind == TokenKind::Equals ||
                next_.kind == TokenKind::LeftParen ||
                next_.kind == TokenKind::Dot) {
                return parseLabelOrAssignmentOrCall();
            }
            return parseSeekStmt();
        }
        case TokenKind::Lock: {
            if (next_.kind == TokenKind::Equals ||
                next_.kind == TokenKind::LeftParen ||
                next_.kind == TokenKind::Dot) {
                return parseLabelOrAssignmentOrCall();
            }
            return parseLockStmt();
        }
        case TokenKind::Unlock: {
            if (next_.kind == TokenKind::Equals ||
                next_.kind == TokenKind::LeftParen ||
                next_.kind == TokenKind::Dot) {
                return parseLabelOrAssignmentOrCall();
            }
            return parseUnlockStmt();
        }
        case TokenKind::Reset: {
            auto loc = currentLoc();
            advance(); // consume 'Reset'
            return std::make_unique<ResetStmt>(loc);
        }
        case TokenKind::FileCopy: return parseFileCopyStmt();
        case TokenKind::Kill:     return parseKillStmt();
        case TokenKind::MkDir:    return parseMkDirStmt();
        case TokenKind::RmDir:    return parseRmDirStmt();
        case TokenKind::ChDir:    return parseChDirStmt();
        case TokenKind::ChDrive:  return parseChDriveStmt();

        // --- 杂项 ---
        case TokenKind::Beep:
        case TokenKind::DoEvents: return parseBeepOrDoEvents();
        case TokenKind::RaiseEvent: return parseRaiseEventStmt();
        case TokenKind::Attribute: return parseAttributeInBody();

        // --- 标识符开头的语句 ---
        // 可能是: 赋值 x = 1, 调用 proc args, 标签 LabelName:, 或 Erase
        case TokenKind::Identifier: {
            // 特殊标识符: Erase (VB6关键字但词法器将其视为标识符)
            if (toLower(cur_.text) == "erase") {
                return parseEraseStmt();
            }
            return parseLabelOrAssignmentOrCall();
        }
        case TokenKind::MeKeyword:
        case TokenKind::Dot:
        case TokenKind::Exclamation:
            return parseLabelOrAssignmentOrCall();

        // --- 软关键字作为标识符 ---
        // VB6 允许 Name/String/Step 等软关键字用作变量名。
        // 当软关键字出现在语句开头, 需要上下文判断:
        // - 如果 next_ 是 =, (, ., NewLine, Colon 等, 视为标识符 (赋值/调用)
        // - 否则尝试作为语句关键字解析
        case TokenKind::Name: {
            // Name X As Y → Name 语句 (文件重命名)
            // name = "World" → 赋值
            if (next_.kind == TokenKind::Equals ||
                next_.kind == TokenKind::LeftParen ||
                next_.kind == TokenKind::Dot ||
                next_.kind == TokenKind::NewLine ||
                next_.kind == TokenKind::Colon) {
                return parseLabelOrAssignmentOrCall();
            }
            return parseNameStmt();
        }

        case TokenKind::MsgBox: {
            // MsgBox 是 VB6 语句 (也是函数, 语句形式不需要括号)
            // result = MsgBox(...) 由赋值处理
            // MsgBox prompt, buttons, title → 语句形式
            if (next_.kind == TokenKind::Equals ||
                next_.kind == TokenKind::LeftParen) {
                // 赋值右边或函数调用形式 → 作为表达式处理
                return parseLabelOrAssignmentOrCall();
            }
            // 语句形式: MsgBox prompt, buttons, ...
            advance(); // consume MsgBox
            auto loc = currentLoc();
            // 解析参数列表 (逗号分隔, 无括号)
            // 将其构造为 CallStmt
            std::vector<ExprPtr> args;
            if (cur_.kind != TokenKind::NewLine && cur_.kind != TokenKind::Colon &&
                cur_.kind != TokenKind::EndOfFile) {
                args.push_back(parseExpression());
                while (match(TokenKind::Comma)) {
                    // VB6 allows empty params: MsgBox "hi", , "title" (buttons omitted)
                    // Use a LiteralExpr with kind EmptyPlaceholder to represent skipped params
                    if (cur_.kind == TokenKind::Comma || cur_.kind == TokenKind::NewLine ||
                        cur_.kind == TokenKind::Colon || cur_.kind == TokenKind::EndOfFile) {
                        auto _ph = std::make_unique<LiteralExpr>(currentLoc(), LiteralKind::Long, "0");
                        _ph->longValue = 0;  // Union与intValue共享内存, 必须显式设置longValue
                        args.push_back(std::move(_ph));
                    } else {
                        args.push_back(parseExpression());
                    }
                }
            }
            auto callExpr = std::make_unique<IndexOrCallExpr>(loc,
                std::make_unique<IdentifierExpr>(loc, "MsgBox"));
            callExpr->positional = std::move(args);
            return std::make_unique<CallStmt>(loc, std::move(callExpr));
        }

        // P22: LSet/RSet statement form (LSet strVar = strExpr / RSet strVar = strExpr)
        case TokenKind::LSet:
        case TokenKind::RSet: {
            auto loc = currentLoc();
            bool isLSet = (cur_.kind == TokenKind::LSet);
            advance();  // consume LSet/RSet
            if (!canBeName(cur_.kind)) {
                diag_.error(DiagnosticID::ParseExpectedToken, loc,
                    isLSet ? "LSet statement requires variable name" : "RSet statement requires variable name");
                return nullptr;
            }
            auto targetName = advance();
            if (!match(TokenKind::Equals)) {
                diag_.error(DiagnosticID::ParseExpectedToken, loc,
                    isLSet ? "LSet statement requires =" : "RSet statement requires =");
                return nullptr;
            }
            auto value = parseExpression();
            auto assignStmt = std::make_unique<AssignmentStmt>(loc,
                std::make_unique<IdentifierExpr>(loc, targetName.text), std::move(value));
            if (isLSet) assignStmt->isLSet = true;
            else assignStmt->isRSet = true;
            return assignStmt;
        }

        default:
            // 其他软关键字在语句位置 → 视为标识符 (赋值/调用)
            if (isSoftKeyword(cur_.kind)) {
                return parseLabelOrAssignmentOrCall();
            }
            diag_.error(DiagnosticID::ParseUnexpectedToken, currentLoc(),
                "unexpected token in statement: " + cur_.text);
            advance();
            return nullptr;
    }
}

// ============================================================
// For / For Each 分发
// ============================================================

StmtPtr Parser::parseForOrForEach() {
    // For Each item In collection  vs  For i = 1 To 10
    if (next_.kind == TokenKind::Each) {
        return parseForEachStmt();
    }
    return parseForStmt();
}

// ============================================================
// 块解析
// ============================================================

StmtList Parser::parseBlock(TokenKind endKind1, TokenKind endKind2) {
    StmtList stmts;
    skipNewLines();

    while (cur_.kind != TokenKind::EndOfFile &&
           cur_.kind != endKind1 &&
           cur_.kind != endKind2) {
        auto stmt = parseStatement();
        if (stmt) {
            stmts.push_back(std::move(stmt));
        }
        expectEndOfStatement();
    }

    return stmts;
}

// --- parseBlockUntil replacement content ---
StmtList Parser::parseBlockUntil(std::initializer_list<TokenKind> endKinds) {
    StmtList stmts;
    skipNewLines();

    while (cur_.kind != TokenKind::EndOfFile) {
        bool isEndOfBlock = false;
        for (auto k : endKinds) {
            if (cur_.kind == k) {
                // Special handling for End token:
                // If End is followed by Sub/Function/Property/etc., it's a block terminator (End Sub, etc.)
                // If End is followed by newline or non-block keyword, it's a standalone End statement
                if (k == TokenKind::End && !isEndBlock()) {
                    // Bare End statement (terminate program), not a block terminator - continue parsing
                    break;
                }
                isEndOfBlock = true;
                break;
            }
        }
        if (isEndOfBlock) return stmts;

        auto stmt = parseStatement();
        if (stmt) {
            stmts.push_back(std::move(stmt));
        }
        expectEndOfStatement();
    }

    return stmts;
}


// ============================================================
// If 语句
// ============================================================

std::unique_ptr<IfStmt> Parser::parseIfStmt() {
    auto loc = currentLoc();
    advance(); // consume 'If'

    auto condition = parseExpression();
    expect(TokenKind::Then, DiagnosticID::ParseInvalidIf,
           "expected 'Then' after If condition");

    // 单行 If?  检查 Then 后面是否紧接语句 (无换行)
    // 跳过 Then 后的可选冒号: If x Then: stmt
    if (cur_.kind == TokenKind::Colon) {
        advance();
    }
    if (cur_.kind != TokenKind::NewLine && cur_.kind != TokenKind::EndOfFile) {
        // 单行 If...Then...[Else...]
        StmtList thenBody;
        inSingleLineIf_++;
        auto stmt = parseStatement();
        if (stmt) thenBody.push_back(std::move(stmt));

        // 解析 Then 后续的冒号分隔语句: If x Then stmt1: stmt2: stmt3
        while (cur_.kind == TokenKind::Colon) {
            advance(); // consume ':'
            if (cur_.kind == TokenKind::Else) break;
            auto moreStmt = parseStatement();
            if (moreStmt) thenBody.push_back(std::move(moreStmt));
        }

        StmtList elseBody;
        if (match(TokenKind::Else)) {
            auto elseStmt = parseStatement();
            if (elseStmt) elseBody.push_back(std::move(elseStmt));
            // Else 也可以有冒号分隔的多语句
            while (cur_.kind == TokenKind::Colon) {
                advance(); // consume ':'
                auto moreStmt = parseStatement();
                if (moreStmt) elseBody.push_back(std::move(moreStmt));
            }
        }
        inSingleLineIf_--;

        return std::make_unique<IfStmt>(loc, std::move(condition),
            std::move(thenBody),
            std::vector<std::unique_ptr<ElseIfClause>>{},
            std::move(elseBody), true);
    }

    // 多行 If...Then
    skipNewLines();
    // 使用 parseBlockUntil 包含 End 作为停止条件
    // (不能只用 parseBlock(ElseIf, Else), 因为没有 Else 的 If 块
    //  遇到 End If 时无法终止, 导致无限循环)
    StmtList thenBody = parseBlockUntil({TokenKind::ElseIf, TokenKind::Else,
                                         TokenKind::End});

    std::vector<std::unique_ptr<ElseIfClause>> elseIfs;
    while (cur_.kind == TokenKind::ElseIf) {
        auto elseifLoc = currentLoc();
        advance(); // consume 'ElseIf'
        auto elseifCond = parseExpression();
        expect(TokenKind::Then, DiagnosticID::ParseInvalidIf,
               "expected 'Then' after ElseIf condition");
        skipNewLines();
        auto elseifBody = parseBlockUntil({TokenKind::ElseIf, TokenKind::Else,
                                           TokenKind::End});
        elseIfs.push_back(std::make_unique<ElseIfClause>(elseifLoc,
            std::move(elseifCond), std::move(elseifBody)));
    }

    StmtList elseBody;
    if (match(TokenKind::Else)) {
        skipNewLines();
        elseBody = parseBlockUntil({TokenKind::End});
    }

    // End If
    expect(TokenKind::End, DiagnosticID::ParseMismatchedBlock,
           "expected 'End If'");
    expect(TokenKind::If, DiagnosticID::ParseMismatchedBlock,
           "expected 'End If'");

    return std::make_unique<IfStmt>(loc, std::move(condition),
        std::move(thenBody), std::move(elseIfs), std::move(elseBody), false);
}

// ============================================================
// For 语句
// ============================================================

std::unique_ptr<ForStmt> Parser::parseForStmt() {
    auto loc = currentLoc();
    advance(); // consume 'For'

    auto varTok = expectName("expected variable name after 'For'");
    expect(TokenKind::Equals, DiagnosticID::ParseInvalidFor,
           "expected '=' in For statement");

    auto start = parseExpression();
    expect(TokenKind::To, DiagnosticID::ParseInvalidFor,
           "expected 'To' in For statement");
    auto end = parseExpression();

    ExprPtr step;
    if (match(TokenKind::Step)) {
        step = parseExpression();
    }

    skipNewLines();
    auto body = parseBlockUntil({TokenKind::Next});

    // Next [var] — 必须消费 Next, 可选的循环变量
    expect(TokenKind::Next, DiagnosticID::ParseMismatchedBlock,
           "expected 'Next' to close For loop");
    if (canBeName(cur_.kind) && cur_.kind != TokenKind::NewLine &&
        cur_.kind != TokenKind::Colon && cur_.kind != TokenKind::EndOfFile) {
        advance();
    }

    return std::make_unique<ForStmt>(loc, varTok.text,
        std::move(start), std::move(end), std::move(step), std::move(body));
}

std::unique_ptr<ForEachStmt> Parser::parseForEachStmt() {
    auto loc = currentLoc();
    advance(); // consume 'For'
    advance(); // consume 'Each'

    auto varTok = expectName("expected variable name");
    expect(TokenKind::In, DiagnosticID::ParseInvalidFor,
           "expected 'In' in For Each statement");
    auto collection = parseExpression();

    skipNewLines();
    auto body = parseBlockUntil({TokenKind::Next});

    expect(TokenKind::Next, DiagnosticID::ParseMismatchedBlock,
           "expected 'Next'");
    if (canBeName(cur_.kind) && cur_.kind != TokenKind::NewLine &&
        cur_.kind != TokenKind::Colon && cur_.kind != TokenKind::EndOfFile) {
        advance();
    }

    return std::make_unique<ForEachStmt>(loc, varTok.text,
        std::move(collection), std::move(body));
}

// ============================================================
// Do Loop 语句
// ============================================================

std::unique_ptr<DoLoopStmt> Parser::parseDoLoopStmt() {
    auto loc = currentLoc();
    advance(); // consume 'Do'

    DoLoopKind loopKind;
    ExprPtr condition;

    if (match(TokenKind::While)) {
        loopKind = DoLoopKind::DoWhileLoop;
        condition = parseExpression();
        skipNewLines();
    } else if (match(TokenKind::Until)) {
        loopKind = DoLoopKind::DoUntilLoop;
        condition = parseExpression();
        skipNewLines();
    } else {
        loopKind = DoLoopKind::DoLoop;  // 无条件 Do...Loop
        skipNewLines();
    }

    auto body = parseBlockUntil({TokenKind::Loop});

    expect(TokenKind::Loop, DiagnosticID::ParseMismatchedBlock,
           "expected 'Loop'");

    // Do...Loop While|Until
    if (loopKind == DoLoopKind::DoLoop) {
        if (match(TokenKind::While)) {
            loopKind = DoLoopKind::DoLoopWhile;
            condition = parseExpression();
        } else if (match(TokenKind::Until)) {
            loopKind = DoLoopKind::DoLoopUntil;
            condition = parseExpression();
        }
    }

    return std::make_unique<DoLoopStmt>(loc, loopKind, std::move(condition), std::move(body));
}

// ============================================================
// While...Wend
// ============================================================

std::unique_ptr<WhileWendStmt> Parser::parseWhileWendStmt() {
    auto loc = currentLoc();
    advance(); // consume 'While'
    auto condition = parseExpression();
    skipNewLines();
    auto body = parseBlockUntil({TokenKind::Wend});
    expect(TokenKind::Wend, DiagnosticID::ParseMismatchedBlock,
           "expected 'Wend'");
    return std::make_unique<WhileWendStmt>(loc, std::move(condition), std::move(body));
}

// ============================================================
// Select Case
// ============================================================

std::unique_ptr<SelectCaseStmt> Parser::parseSelectCaseStmt() {
    auto loc = currentLoc();
    advance(); // consume 'Select'
    expect(TokenKind::Case, DiagnosticID::ParseInvalidSelect,
           "expected 'Case' after 'Select'");
    auto testExpr = parseExpression();
    skipNewLines();

    std::vector<std::unique_ptr<CaseClause>> cases;
    StmtList elseCase;

    while (cur_.kind == TokenKind::Case && next_.kind != TokenKind::Else) {
        auto caseLoc = currentLoc();
        advance(); // consume 'Case'

        // 解析 Case 值列表
        std::vector<CaseClause::CaseValue> values;
        do {
            skipNewLines();
            values.push_back(parseCaseValue());
        } while (match(TokenKind::Comma));

        skipNewLines();
        // VB6 允许 Case N: statement (冒号分隔)
        match(TokenKind::Colon);
        auto body = parseBlockUntil({TokenKind::Case, TokenKind::End});
        cases.push_back(std::make_unique<CaseClause>(caseLoc,
            std::move(values), std::move(body)));
    }

    // Case Else
    if (cur_.kind == TokenKind::Case && next_.kind == TokenKind::Else) {
        advance(); // consume 'Case'
        advance(); // consume 'Else'
        skipNewLines();
        // VB6 允许 Case Else: statement (冒号分隔)
        match(TokenKind::Colon);
        elseCase = parseBlockUntil({TokenKind::End});
    }

    // End Select
    expect(TokenKind::End, DiagnosticID::ParseMismatchedBlock,
           "expected 'End Select'");
    expect(TokenKind::Select, DiagnosticID::ParseMismatchedBlock,
           "expected 'End Select'");

    return std::make_unique<SelectCaseStmt>(loc, std::move(testExpr),
        std::move(cases), std::move(elseCase));
}

// ============================================================
// With 语句
// ============================================================

std::unique_ptr<WithStmt> Parser::parseWithStmt() {
    auto loc = currentLoc();
    advance(); // consume 'With'
    auto object = parseExpression();
    skipNewLines();

    withDepth_++;
    auto body = parseBlockUntil({TokenKind::End});
    withDepth_--;

    // End With
    expect(TokenKind::End, DiagnosticID::ParseMismatchedBlock,
           "expected 'End With'");
    expect(TokenKind::With, DiagnosticID::ParseMismatchedBlock,
           "expected 'End With'");

    return std::make_unique<WithStmt>(loc, std::move(object), std::move(body));
}

// ============================================================
// On 语句 (On Error / On GoTo / On GoSub)
// ============================================================

StmtPtr Parser::parseOnStmt() {
    // On Error GoTo | On Error Resume Next | On x GoTo | On x GoSub
    if (next_.kind == TokenKind::Error) {
        return parseOnErrorStmt();
    }
    if (next_.kind == TokenKind::GoTo) {
        return parseOnGoToStmt();
    }
    if (next_.kind == TokenKind::GoSub) {
        return parseOnGoSubStmt();
    }

    diag_.error(DiagnosticID::ParseUnexpectedToken, currentLoc(),
        "expected 'Error GoTo', 'Error Resume', 'GoTo', or 'GoSub' after 'On'");
    advance();
    return nullptr;
}

std::unique_ptr<OnErrorStmt> Parser::parseOnErrorStmt() {
    auto loc = currentLoc();
    advance(); // consume 'On'
    advance(); // consume 'Error'

    if (match(TokenKind::Resume)) {
        expect(TokenKind::Next, DiagnosticID::ParseExpectedToken,
               "expected 'Next' after 'On Error Resume'");
        return std::make_unique<OnErrorStmt>(loc, OnErrorKind::ResumeNext);
    }

    expect(TokenKind::GoTo, DiagnosticID::ParseExpectedToken,
           "expected 'GoTo' after 'On Error'");

    // On Error GoTo 0 (关闭错误处理)
    if (cur_.kind == TokenKind::IntegerLiteral && cur_.intValue == 0) {
        advance();
        return std::make_unique<OnErrorStmt>(loc, OnErrorKind::GoToZero);
    }

    auto labelTok = expectName("expected label after 'On Error GoTo'");
    return std::make_unique<OnErrorStmt>(loc, OnErrorKind::GoToLabel, labelTok.text);
}

// P14.1.2: Resume语句解析
std::unique_ptr<ResumeStmt> Parser::parseResumeStmt() {
    auto loc = currentLoc();
    advance();
    if (match(TokenKind::Next)) {
        return std::make_unique<ResumeStmt>(loc, ResumeKind::ResumeNext);
    }
    if (cur_.kind == TokenKind::Identifier) {
        auto labelTok = expectName("expected label after 'Resume'");
        return std::make_unique<ResumeStmt>(loc, ResumeKind::ResumeLabel, labelTok.text);
    }
    return std::make_unique<ResumeStmt>(loc, ResumeKind::ResumeHere);
}

// P14.1.3: Error语句解析
std::unique_ptr<ErrorStmt> Parser::parseErrorStmt() {
    auto loc = currentLoc();
    advance();
    auto errNum = parseExpression();
    return std::make_unique<ErrorStmt>(loc, std::move(errNum));
}

// P18-A: Mid$ statement (assignment) — Mid$(var, start[, length]) = expr
// P18-A: Mid$ statement (assignment) — Mid$(var, start[, length]) = expr
// When Mid$(var, start, len) is NOT followed by =, treat as function call expression statement
StmtPtr Parser::parseMidStmt() {
    auto loc = currentLoc();
    advance(); // consume 'Mid'
    // optional $ suffix
    if (cur_.kind == TokenKind::Dollar) advance();
    expect(TokenKind::LeftParen, DiagnosticID::ParseExpectedToken,
           "expected '(' after Mid$");
    // Parse target variable
    auto target = parseExpression();
    expect(TokenKind::Comma, DiagnosticID::ParseExpectedToken,
           "expected ',' in Mid$ statement");
    // Parse start position
    auto startPos = parseExpression();
    // Optional length argument
    ExprPtr lengthExpr;
    int32_t hasLength = 0;
    if (match(TokenKind::Comma)) {
        lengthExpr = parseExpression();
        hasLength = 1;
    }
    expect(TokenKind::RightParen, DiagnosticID::ParseExpectedToken,
           "expected ')' in Mid$ statement");
    // Check for = (Mid$ statement assignment)
    if (match(TokenKind::Equals)) {
        // Parse replacement value
        auto value = parseExpression();
        if (!hasLength) {
            // Create dummy length expr (0 means "rest of string")
            auto dummyLen = std::make_unique<LiteralExpr>(loc, LiteralKind::Integer, "0");
            dummyLen->intValue = 0;
            lengthExpr = std::move(dummyLen);
        }
        return std::make_unique<MidStmt>(loc, std::move(target), std::move(startPos),
                                          std::move(lengthExpr), std::move(value), hasLength);
    }
    // Not Mid$ statement assignment — treat as function call expression statement
    // Construct Mid$(target, start, len) as IndexOrCallExpr
    auto call = std::make_unique<IndexOrCallExpr>(loc, std::move(target));
    call->positional.push_back(std::move(startPos));
    if (hasLength && lengthExpr) call->positional.push_back(std::move(lengthExpr));
    return std::make_unique<CallStmt>(loc, std::move(call));
}

std::unique_ptr<OnGoToStmt> Parser::parseOnGoToStmt() {
    auto loc = currentLoc();
    advance(); // consume 'On'
    auto index = parseExpression();
    expect(TokenKind::GoTo, DiagnosticID::ParseExpectedToken,
           "expected 'GoTo' in On...GoTo");
    std::vector<std::string> labels;
    labels.push_back(expectName("expected label").text);
    while (match(TokenKind::Comma)) {
        labels.push_back(expectName("expected label").text);
    }
    return std::make_unique<OnGoToStmt>(loc, std::move(index), std::move(labels));
}

std::unique_ptr<OnGoSubStmt> Parser::parseOnGoSubStmt() {
    auto loc = currentLoc();
    advance(); // consume 'On'
    auto index = parseExpression();
    expect(TokenKind::GoSub, DiagnosticID::ParseExpectedToken,
           "expected 'GoSub' in On...GoSub");
    std::vector<std::string> labels;
    labels.push_back(expectName("expected label").text);
    while (match(TokenKind::Comma)) {
        labels.push_back(expectName("expected label").text);
    }
    return std::make_unique<OnGoSubStmt>(loc, std::move(index), std::move(labels));
}

// ============================================================
// Exit / GoTo / GoSub / Return
// ============================================================

std::unique_ptr<ExitStmt> Parser::parseExitStmt() {
    auto loc = currentLoc();
    advance(); // consume 'Exit'

    if (match(TokenKind::Sub))      return std::make_unique<ExitStmt>(loc, ExitKind::Sub);
    if (match(TokenKind::Function)) return std::make_unique<ExitStmt>(loc, ExitKind::Function);
    if (match(TokenKind::Property)) return std::make_unique<ExitStmt>(loc, ExitKind::Property);
    if (match(TokenKind::Do))       return std::make_unique<ExitStmt>(loc, ExitKind::Do);
    if (match(TokenKind::For))      return std::make_unique<ExitStmt>(loc, ExitKind::For);

    diag_.error(DiagnosticID::ParseExpectedToken, currentLoc(),
        "expected 'Sub', 'Function', 'Property', 'Do', or 'For' after 'Exit'");
    return std::make_unique<ExitStmt>(loc, ExitKind::Sub);
}

std::unique_ptr<GoToStmt> Parser::parseGoToStmt() {
    auto loc = currentLoc();
    advance(); // consume 'GoTo'
    auto label = expectName("expected label after 'GoTo'");
    return std::make_unique<GoToStmt>(loc, label.text);
}

std::unique_ptr<GoSubStmt> Parser::parseGoSubStmt() {
    auto loc = currentLoc();
    advance(); // consume 'GoSub'
    auto label = expectName("expected label after 'GoSub'");
    return std::make_unique<GoSubStmt>(loc, label.text);
}

std::unique_ptr<ReturnStmt> Parser::parseReturnStmt() {
    auto loc = currentLoc();
    advance(); // consume 'Return'
    return std::make_unique<ReturnStmt>(loc);
}

std::unique_ptr<StopStmt> Parser::parseStopStmt() {
    auto loc = currentLoc();
    advance(); // consume 'Stop'
    return std::make_unique<StopStmt>(loc);
}

std::unique_ptr<EndStmt> Parser::parseEndStmt() {
    auto loc = currentLoc();
    advance(); // consume 'End'
    return std::make_unique<EndStmt>(loc);
}

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
            std::make_unique<VariableDecl>(loc, AccessLevel::Private, nameTok.text,
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
    // 且不是中缀运算符, 则视为无括号调用的参数列表
    if (cur_.kind != TokenKind::NewLine && cur_.kind != TokenKind::Colon &&
        cur_.kind != TokenKind::EndOfFile && (!isInfixOperator(cur_.kind) || isDebugPrint)) {
        // 将表达式包装为 IndexOrCallExpr, 追加参数
        auto call = std::make_unique<IndexOrCallExpr>(loc, std::move(expr));

        // 解析参数列表
        // VB6 中逗号和分号都分隔参数 (分号是 Print 的位置修饰符)
        // MsgBox arg1, arg2, arg3        — 逗号分隔
        // Debug.Print "text"; i         — 分号分隔 (紧跟下一个参数)
        // Debug.Print "text";           — 末尾分号 (抑制换行, 无后续参数)

        // 辅助: 判断当前 token 是否可以开始一个表达式 (但不是语句结束符/中缀运算符)
        auto canStartArg = [this]() -> bool {
            return cur_.kind != TokenKind::NewLine &&
                   cur_.kind != TokenKind::Colon &&
                   cur_.kind != TokenKind::EndOfFile &&
                   !isInfixOperator(cur_.kind) &&
                   cur_.kind != TokenKind::Comma &&
                   cur_.kind != TokenKind::Semicolon;
        };

        // 第一个参数
        if (cur_.kind == TokenKind::ByVal || cur_.kind == TokenKind::ByRef) {
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

// ============================================================
// 文件 I/O 语句
// ============================================================

std::unique_ptr<OpenStmt> Parser::parseOpenStmt() {
    auto loc = currentLoc();
    advance(); // consume 'Open'
    auto pathName = parseExpression();

    OpenMode mode = OpenMode::Random;
    if (match(TokenKind::For)) {
        if (match(TokenKind::Input))          mode = OpenMode::Input;
        else if (match(TokenKind::Output))    mode = OpenMode::Output;
        else if (match(TokenKind::Append))     mode = OpenMode::Append;
        else if (match(TokenKind::Binary))     mode = OpenMode::Binary;
        else if (match(TokenKind::Random))     mode = OpenMode::Random;
    }

    OpenAccess access = OpenAccess::Default;
    if (match(TokenKind::Access)) {
        if (match(TokenKind::Read)) {
            if (match(TokenKind::Write)) access = OpenAccess::ReadWrite;
            else access = OpenAccess::Read;
        } else if (match(TokenKind::Write)) {
            access = OpenAccess::Write;
        } else if (match(TokenKind::ReadWrite)) {
            access = OpenAccess::ReadWrite;
        }
    }

    LockType lock = LockType::Default;
    if (match(TokenKind::Shared)) {
        lock = LockType::Shared;
    } else if (cur_.kind == TokenKind::Lock) {
        advance();
        if (match(TokenKind::Read)) {
            if (match(TokenKind::Write)) lock = LockType::LockReadWrite;
            else lock = LockType::LockRead;
        } else if (match(TokenKind::Write)) {
            lock = LockType::LockWrite;
        }
    }

    expect(TokenKind::As, DiagnosticID::ParseExpectedToken,
           "expected 'As' in Open statement");
    if (cur_.kind == TokenKind::Hash) advance();  // 可选的 #
    auto fileNumber = parseExpression();

    ExprPtr recordLength;
    // Len=reclength (可选)
    if (cur_.kind == TokenKind::Identifier && toLower(cur_.text) == "len") {
        advance(); // consume 'Len'
        if (match(TokenKind::Equals)) {
            recordLength = parseExpression();
        }
    }

    return std::make_unique<OpenStmt>(loc, std::move(pathName), mode, access,
        lock, std::move(fileNumber), std::move(recordLength));
}

std::unique_ptr<CloseStmt> Parser::parseCloseStmt() {
    auto loc = currentLoc();
    advance(); // consume 'Close'
    std::vector<ExprPtr> fileNumbers;
    if (cur_.kind != TokenKind::NewLine && cur_.kind != TokenKind::EndOfFile) {
        do {
            if (cur_.kind == TokenKind::Hash) advance();
            fileNumbers.push_back(parseExpression());
        } while (match(TokenKind::Comma));
    }
    return std::make_unique<CloseStmt>(loc, std::move(fileNumbers));
}

std::unique_ptr<GetStmt> Parser::parseGetStmt() {
    auto loc = currentLoc();
    advance(); // consume 'Get'
    if (cur_.kind == TokenKind::Hash) advance();
    auto fileNumber = parseExpression();
    ExprPtr recordNumber;
    if (match(TokenKind::Comma)) {
        // VB6 允许省略记录号: Get #1, , data
        if (cur_.kind != TokenKind::Comma) {
            recordNumber = parseExpression();
        }
    }
    expect(TokenKind::Comma, DiagnosticID::ParseExpectedToken,
           "expected ',' in Get statement");
    auto varName = parseExpression();
    return std::make_unique<GetStmt>(loc, std::move(fileNumber),
        std::move(recordNumber), std::move(varName));
}

std::unique_ptr<PutStmt> Parser::parsePutStmt() {
    auto loc = currentLoc();
    advance(); // consume 'Put'
    if (cur_.kind == TokenKind::Hash) advance();
    auto fileNumber = parseExpression();
    ExprPtr recordNumber;
    if (match(TokenKind::Comma)) {
        // VB6 允许省略记录号: Put #1, , data
        if (cur_.kind != TokenKind::Comma) {
            recordNumber = parseExpression();
        }
    }
    expect(TokenKind::Comma, DiagnosticID::ParseExpectedToken,
           "expected ',' in Put statement");
    auto varName = parseExpression();
    return std::make_unique<PutStmt>(loc, std::move(fileNumber),
        std::move(recordNumber), std::move(varName));
}

std::unique_ptr<InputStmt> Parser::parseInputStmt() {
    auto loc = currentLoc();
    advance(); // consume 'Input'
    if (cur_.kind == TokenKind::Hash) advance();
    auto fileNumber = parseExpression();
    expect(TokenKind::Comma, DiagnosticID::ParseExpectedToken,
           "expected ',' in Input statement");
    std::vector<ExprPtr> varList;
    varList.push_back(parseExpression());
    while (match(TokenKind::Comma)) {
        varList.push_back(parseExpression());
    }
    return std::make_unique<InputStmt>(loc, std::move(fileNumber), std::move(varList));
}

std::unique_ptr<PrintStmt> Parser::parsePrintStmt() {
    auto loc = currentLoc();
    advance(); // consume 'Print'
    
    // M22-Issue6: Distinguish "Print expr" (form surface) from "Print #n, expr" (file I/O)
    // If '#' is present, this is file I/O mode
    // If no '#', the first expression is the first output item (form print mode)
    bool hasHash = (cur_.kind == TokenKind::Hash);
    if (hasHash) advance();
    
    auto firstExpr = parseExpression();
    std::vector<ExprPtr> outputList;
    ExprPtr fileNumber;
    bool isFormPrint = false;
    
    if (hasHash) {
        // File I/O mode: "Print #n, expr1; expr2; ..."
        fileNumber = std::move(firstExpr);
        if (match(TokenKind::Comma) || match(TokenKind::Semicolon)) {
            while (cur_.kind != TokenKind::NewLine && cur_.kind != TokenKind::EndOfFile) {
                outputList.push_back(parseExpression());
                if (!match(TokenKind::Comma) && !match(TokenKind::Semicolon)) break;
            }
        }
    } else {
        // Form print mode: "Print expr1; expr2; ..."
        // The first expression IS the first output item
        isFormPrint = true;
        outputList.push_back(std::move(firstExpr));
        while (match(TokenKind::Semicolon) || match(TokenKind::Comma)) {
            if (cur_.kind == TokenKind::NewLine || cur_.kind == TokenKind::EndOfFile) break;
            outputList.push_back(parseExpression());
        }
    }
    
    auto stmt = std::make_unique<PrintStmt>(loc, std::move(fileNumber), std::move(outputList));
    stmt->isFormPrint = isFormPrint;
    return stmt;
}

std::unique_ptr<WriteStmt> Parser::parseWriteStmt() {
    auto loc = currentLoc();
    advance(); // consume 'Write'
    if (cur_.kind == TokenKind::Hash) advance();
    auto fileNumber = parseExpression();
    std::vector<ExprPtr> outputList;
    if (match(TokenKind::Comma) || match(TokenKind::Semicolon)) {
        while (cur_.kind != TokenKind::NewLine && cur_.kind != TokenKind::EndOfFile) {
            outputList.push_back(parseExpression());
            if (!match(TokenKind::Comma) && !match(TokenKind::Semicolon)) break;
        }
    }
    return std::make_unique<WriteStmt>(loc, std::move(fileNumber), std::move(outputList));
}

std::unique_ptr<LineInputStmt> Parser::parseLineInputStmt() {
    auto loc = currentLoc();
    advance(); // consume 'Line'
    expect(TokenKind::Input, DiagnosticID::ParseExpectedToken,
           "expected 'Input' after 'Line'");
    if (cur_.kind == TokenKind::Hash) advance();
    auto fileNumber = parseExpression();
    expect(TokenKind::Comma, DiagnosticID::ParseExpectedToken,
           "expected ',' in Line Input statement");
    auto varName = parseExpression();
    return std::make_unique<LineInputStmt>(loc, std::move(fileNumber), std::move(varName));
}

std::unique_ptr<WidthStmt> Parser::parseWidthStmt() {
    auto loc = currentLoc();
    advance(); // consume 'Width'
    if (cur_.kind == TokenKind::Hash) advance();
    auto fileNumber = parseExpression();
    expect(TokenKind::Comma, DiagnosticID::ParseExpectedToken,
           "expected ',' in Width statement");
    auto width = parseExpression();
    return std::make_unique<WidthStmt>(loc, std::move(fileNumber), std::move(width));
}

std::unique_ptr<SeekStmt> Parser::parseSeekStmt() {
    auto loc = currentLoc();
    advance(); // consume 'Seek'
    if (cur_.kind == TokenKind::Hash) advance();
    auto fileNumber = parseExpression();
    expect(TokenKind::Comma, DiagnosticID::ParseExpectedToken,
           "expected ',' in Seek statement");
    auto position = parseExpression();
    return std::make_unique<SeekStmt>(loc, std::move(fileNumber), std::move(position));
}

std::unique_ptr<LockStmt> Parser::parseLockStmt() {
    auto loc = currentLoc();
    advance(); // consume 'Lock'
    if (cur_.kind == TokenKind::Hash) advance();
    auto fileNumber = parseExpression();
    ExprPtr start, end;
    if (match(TokenKind::Comma)) {
        start = parseExpression();
        if (match(TokenKind::To)) {
            end = parseExpression();
        }
    }
    return std::make_unique<LockStmt>(loc, std::move(fileNumber), std::move(start), std::move(end));
}

std::unique_ptr<UnlockStmt> Parser::parseUnlockStmt() {
    auto loc = currentLoc();
    advance(); // consume 'Unlock'
    if (cur_.kind == TokenKind::Hash) advance();
    auto fileNumber = parseExpression();
    ExprPtr start, end;
    if (match(TokenKind::Comma)) {
        start = parseExpression();
        if (match(TokenKind::To)) {
            end = parseExpression();
        }
    }
    return std::make_unique<UnlockStmt>(loc, std::move(fileNumber), std::move(start), std::move(end));
}

std::unique_ptr<NameStmt> Parser::parseNameStmt() {
    auto loc = currentLoc();
    advance(); // consume 'Name'
    auto oldPath = parseExpression();
    expect(TokenKind::As, DiagnosticID::ParseExpectedToken,
           "expected 'As' in Name statement");
    auto newPath = parseExpression();
    return std::make_unique<NameStmt>(loc, std::move(oldPath), std::move(newPath));
}

std::unique_ptr<FileCopyStmt> Parser::parseFileCopyStmt() {
    auto loc = currentLoc();
    advance(); // consume 'FileCopy'
    auto source = parseExpression();
    expect(TokenKind::Comma, DiagnosticID::ParseExpectedToken,
           "expected ',' in FileCopy statement");
    auto dest = parseExpression();
    return std::make_unique<FileCopyStmt>(loc, std::move(source), std::move(dest));
}

std::unique_ptr<KillStmt> Parser::parseKillStmt() {
    auto loc = currentLoc();
    advance(); // consume 'Kill'
    auto path = parseExpression();
    return std::make_unique<KillStmt>(loc, std::move(path));
}

std::unique_ptr<MkDirStmt> Parser::parseMkDirStmt() {
    auto loc = currentLoc();
    advance(); // consume 'MkDir'
    auto path = parseExpression();
    return std::make_unique<MkDirStmt>(loc, std::move(path));
}

std::unique_ptr<RmDirStmt> Parser::parseRmDirStmt() {
    auto loc = currentLoc();
    advance(); // consume 'RmDir'
    auto path = parseExpression();
    return std::make_unique<RmDirStmt>(loc, std::move(path));
}

std::unique_ptr<ChDirStmt> Parser::parseChDirStmt() {
    auto loc = currentLoc();
    advance(); // consume 'ChDir'
    auto path = parseExpression();
    return std::make_unique<ChDirStmt>(loc, std::move(path));
}

std::unique_ptr<ChDriveStmt> Parser::parseChDriveStmt() {
    auto loc = currentLoc();
    advance(); // consume 'ChDrive'
    auto drive = parseExpression();
    return std::make_unique<ChDriveStmt>(loc, std::move(drive));
}

// ============================================================
// Beep / DoEvents
// ============================================================

StmtPtr Parser::parseBeepOrDoEvents() {
    auto loc = currentLoc();
    if (cur_.kind == TokenKind::Beep) {
        advance();
        return std::make_unique<BeepStmt>(loc);
    }
    advance(); // consume 'DoEvents'
    return std::make_unique<DoEventsStmt>(loc);
}

// ============================================================
// Attribute (过程体内)
// ============================================================

std::unique_ptr<AttributeStmt> Parser::parseAttributeInBody() {
    return parseAttribute();
}

} // namespace vb6c3
