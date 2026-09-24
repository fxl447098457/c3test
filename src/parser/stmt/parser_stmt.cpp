// vb6c3 - 语句解析器
// VB6 块语句 + 单行语句

#include "parser/parser.hpp"

namespace vb6c3 {

// --- parser_stmt.cpp: 语句分发 + 块辅助（parseStatement / parseBlock / parseBlockUntil） ---


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
        // ai/vb-asm-extension-spec: Asm ... End Asm 内联汇编块
        case TokenKind::Asm:      return parseAsmStmt();

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

        // VB6 行号标签: `100: ...` 或 `100 ...`。语句起始处只可能是行号 (赋值/调用的
        // 左值必须是名字), 交给通用标签/赋值解析器识别成 LabelStmt。
        case TokenKind::IntegerLiteral:
            return parseLabelOrAssignmentOrCall();

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

        // P22: LSet/RSet statement form (LSet strVar = strExpr / RSet objVar = objExpr)
        // Fix 082: LHS 可以是数组元素或成员 (VB6 允许 LSet arr(i) = arr(i+1) /
        // LSet obj.Sub = obj2), 故复用通用左值解析而不是只接受裸标识符.
        // (Common.bas:417 `LSet MsgBoxHelpData(i) = MsgBoxHelpData(i + 1)`)
        case TokenKind::LSet:
        case TokenKind::RSet: {
            auto loc = currentLoc();
            bool isLSet = (cur_.kind == TokenKind::LSet);
            advance();  // consume LSet/RSet
            auto stmt = parseLabelOrAssignmentOrCall();
            auto* assign = dynamic_cast<AssignmentStmt*>(stmt.get());
            if (!assign) {
                diag_.error(DiagnosticID::ParseExpectedToken, loc,
                    (isLSet ? "LSet statement requires '<target> = <expr>'"
                            : "RSet statement requires '<target> = <expr>'"));
                return nullptr;
            }
            if (isLSet) assign->isLSet = true;
            else assign->isRSet = true;
            return stmt;
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
// ai/vb-asm-extension-spec: Asm ... End Asm 原始块捕获
//   块内行按**原始源码**直取 (buffer_->getLine), 不做 token 重组 ——
//   保证 `dword ptr [x]` / `.label:` / `'` 注释 等原样交付给 MASM。
//   token 流只用来找边界: 行首处出现 End + Asm。
// ============================================================
StmtPtr Parser::parseAsmStmt() {
    auto loc = currentLoc();
    Token asmTok = advance();   // consume 'Asm'

    // v1: 仅支持块形式 —— Asm 后必须换行 (`Asm <指令>` 单行形式不支持)
    if (!check(TokenKind::NewLine) && !check(TokenKind::EndOfFile)) {
        diag_.error(DiagnosticID::ParseAsmBlockMalformed, loc,
                    "Asm 块必须独占一行 (v1 不支持 `Asm <指令>` 单行形式)");
    }

    auto stmt = std::make_unique<AsmStmt>(loc);
    const uint32_t startLine = asmTok.line;
    uint32_t endLine = 0;

    // 逐 token 前进, 直到「行首」出现 End + Asm (atLineStart 由消费 NewLine 置位)
    bool atLineStart = true;
    while (!check(TokenKind::EndOfFile)) {
        Token t = peek();
        if (t.kind == TokenKind::NewLine) { advance(); atLineStart = true; continue; }
        if (atLineStart && t.kind == TokenKind::End) {
            advance();                       // consume End
            if (check(TokenKind::Asm)) {
                advance();                   // consume Asm
                endLine = t.line;
                break;
            }
            atLineStart = false;             // End 是块内内容, 继续
            continue;
        }
        atLineStart = false;
        advance();
    }

    if (endLine == 0) {
        diag_.error(DiagnosticID::ParseAsmBlockMalformed, loc, "Asm 块缺少 End Asm");
        return stmt;
    }

    // 原始行切片: (startLine, endLine) 开区间
    //   getLine() 返回的行含行尾换行 (含 CRLF 的 \r), 必须剥掉 —— 否则发射端
    //   再加一个 \n 会凭空多出空行 (实测 ml64 对空行无害, 但输出不可读)。
    if (buffer_) {
        for (uint32_t ln = startLine + 1; ln < endLine; ln++) {
            std::string line(buffer_->getLine(ln));
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
            stmt->lines.push_back(line);
        }
    }
    // 注意: 不要把 `End Asm` 行尾的 NewLine 吃掉 —— 语句边界由外层
    // parseBlock 的 expectEndOfStatement() 统一消费 (同其它语句)。
    return stmt;
}

} // namespace vb6c3
