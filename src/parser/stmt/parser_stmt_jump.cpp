// vb6c3 - 语句解析器
// VB6 块语句 + 单行语句

#include "parser/parser.hpp"
#include <algorithm>
#include <cctype>

namespace vb6c3 {

// --- parser_stmt_jump.cpp: 错误处理与跳转（On / OnError / Resume / Error / Mid / Exit / GoTo / GoSub / Return / Stop / End） ---


// ============================================================
// On 语句 (On Error / On GoTo / On GoSub)
// ============================================================

StmtPtr Parser::parseOnStmt() {
    // On Error GoTo | On Error Resume Next | On x GoTo | On x GoSub
    std::string modifier = next_.text;
    std::transform(modifier.begin(), modifier.end(), modifier.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (next_.kind == TokenKind::Error ||
        (next_.kind == TokenKind::Identifier && modifier == "local")) {
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
    // VB6 accepts Local as an optional modifier; handlers are procedure-local.
    if (cur_.kind == TokenKind::Identifier) advance(); // Local (checked by caller)
    expect(TokenKind::Error, DiagnosticID::ParseExpectedToken,
           "expected 'Error' after 'On [Local]'");

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

} // namespace vb6c3
