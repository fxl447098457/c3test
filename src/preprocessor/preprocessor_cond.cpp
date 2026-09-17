#include "preprocessor/preprocessor.hpp"
#include <cctype>
#include <algorithm>

namespace vb6c3 {

// --- preprocessor_cond.cpp: 条件编译指令处理（#Const / #If / #ElseIf / #Else / #End If） ---


// ============================================================
// #Const name = expression
// ============================================================

void Preprocessor::handleHashConst() {
    auto loc = currentLoc();
    fetchRaw(); // 消费 #Const

    // 在非活跃分支中, #Const 不生效, 跳过整行
    if (!isActive()) {
        skipToEndOfLine();
        return;
    }

    // 读取常量名
    if (rawCur_.kind != TokenKind::Identifier) {
        diag_.error(DiagnosticID::ParseExpectedToken, loc,
            "#Const 后面需要标识符");
        skipToEndOfLine();
        return;
    }
    std::string name = rawCur_.text;
    std::string nameLower = name;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
    fetchRaw(); // 消费标识符

    // 期望 =
    if (rawCur_.kind != TokenKind::Equals) {
        diag_.error(DiagnosticID::ParseExpectedToken, loc,
            "#Const 后面需要 '='");
        skipToEndOfLine();
        return;
    }
    fetchRaw(); // 消费 =

    // 求值表达式
    CondCompileValue value = evalCondition();

    // 存储 (命令行定义优先, 不可覆盖)
    if (constants_.find(nameLower) == constants_.end()) {
        constants_[nameLower] = value;
    }
}

// ============================================================
// #If condition Then
// ============================================================

void Preprocessor::handleHashIf() {
    fetchRaw(); // 消费 #If

    bool outerActive = isActive();

    if (!outerActive) {
        // 外层非活跃, 本块整体非活跃
        // 仍需压栈以追踪嵌套
        condStack_.push_back({false, false, false, false});
        skipToEndOfLine(); // 跳过 Then
        return;
    }

    // 求值条件
    CondCompileValue condVal = evalCondition();

    // 期望 Then
    if (rawCur_.kind == TokenKind::Then) {
        fetchRaw(); // 消费 Then
    } else {
        SourceLocation loc{"", rawCur_.line, rawCur_.column};
        diag_.error(DiagnosticID::ParseExpectedToken, loc,
            "#If 条件后需要 Then");
    }

    bool condBool = condVal.toBool();
    condStack_.push_back({true, condBool, condBool, false});
}

// ============================================================
// #ElseIf condition Then
// ============================================================

void Preprocessor::handleHashElseIf() {
    fetchRaw(); // 消费 #ElseIf

    if (condStack_.empty()) {
        SourceLocation loc{"", rawCur_.line, rawCur_.column};
        diag_.error(DiagnosticID::ParseMismatchedBlock, loc,
            "#ElseIf 没有匹配的 #If");
        skipToEndOfLine();
        return;
    }

    auto& block = condStack_.back();

    if (block.hasElse) {
        SourceLocation loc{"", rawCur_.line, rawCur_.column};
        diag_.error(DiagnosticID::ParseMismatchedBlock, loc,
            "#ElseIf 出现在 #Else 之后");
        skipToEndOfLine();
        return;
    }

    if (!block.outerActive) {
        // 外层非活跃, 直接跳过
        skipToEndOfLine();
        return;
    }

    if (block.anyBranchTaken) {
        // 已有分支被选中, 本分支不活跃
        block.currentActive = false;
        skipToEndOfLine();
        return;
    }

    // 求值条件
    CondCompileValue condVal = evalCondition();

    // 期望 Then
    if (rawCur_.kind == TokenKind::Then) {
        fetchRaw(); // 消费 Then
    }

    bool condBool = condVal.toBool();
    block.currentActive = condBool;
    if (condBool) {
        block.anyBranchTaken = true;
    }
}

// ============================================================
// #Else
// ============================================================

void Preprocessor::handleHashElse() {
    fetchRaw(); // 消费 #Else

    if (condStack_.empty()) {
        SourceLocation loc{"", rawCur_.line, rawCur_.column};
        diag_.error(DiagnosticID::ParseMismatchedBlock, loc,
            "#Else 没有匹配的 #If");
        return;
    }

    auto& block = condStack_.back();
    block.hasElse = true;

    if (!block.outerActive) {
        return;
    }

    // 如果前面已有分支被选中, #Else 不活跃
    block.currentActive = !block.anyBranchTaken;
    if (block.currentActive) {
        block.anyBranchTaken = true;
    }
}

// ============================================================
// #End If
// ============================================================

void Preprocessor::handleHashEndIf() {
    fetchRaw(); // 消费 #End

    // 期望 If
    if (rawCur_.kind == TokenKind::If) {
        fetchRaw(); // 消费 If
    } else {
        SourceLocation loc{"", rawCur_.line, rawCur_.column};
        diag_.error(DiagnosticID::ParseMismatchedBlock, loc,
            "#End 后面需要 If");
        return;
    }

    if (condStack_.empty()) {
        SourceLocation loc{"", rawCur_.line, rawCur_.column};
        diag_.error(DiagnosticID::ParseMismatchedBlock, loc,
            "#End If 没有匹配的 #If");
        return;
    }

    condStack_.pop_back();
}

// ============================================================
// 跳过到行尾
// ============================================================

void Preprocessor::skipToEndOfLine() {
    // 跳过当前行剩余token直到NewLine或EOF
    // 注意: 需要正确处理嵌套的条件编译
    while (rawCur_.kind != TokenKind::NewLine &&
           rawCur_.kind != TokenKind::EndOfFile) {
        fetchRaw();
    }
    // 也跳过NewLine本身
    if (rawCur_.kind == TokenKind::NewLine) {
        fetchRaw();
    }
}

} // namespace vb6c3
