#include "backend/cgen.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <functional>

namespace vb6c3 {

// --- cgen_loop.cpp: Do/While 循环语句生成 ---

void CCodeGen::visit(DoLoopStmt& node) {
    // P14.3.2: 嵌套循环栈
    std::string exitLabel = "vb6_loop_exit_" + std::to_string(labelCounter_++);
    loopStack_.push_back({ExitKind::Do, exitLabel});

    switch (node.loopKind) {
        case DoLoopKind::DoWhileLoop:
            if (node.condition) {
                emitExpr(*node.condition);
                if (isComMarker_) resolveComValue("Int");
                c_.emitLine("while (" + lastExpr_ + ") {");
            } else {
                c_.emitLine("while (1) {");
            }
            c_.indent();
            emitStmtList(node.body);
            c_.dedent();
            c_.emitLine("}");
            break;

        case DoLoopKind::DoUntilLoop:
            if (node.condition) {
                emitExpr(*node.condition);
                c_.emitLine("while (!(" + lastExpr_ + ")) {");
            } else {
                c_.emitLine("while (1) {");
            }
            c_.indent();
            emitStmtList(node.body);
            c_.dedent();
            c_.emitLine("}");
            break;

        case DoLoopKind::DoLoopWhile:
            c_.emitLine("do {");
            c_.indent();
            emitStmtList(node.body);
            c_.dedent();
            if (node.condition) {
                emitExpr(*node.condition);
                c_.emitLine("} while (" + lastExpr_ + ");");
            } else {
                c_.emitLine("} while (1);");
            }
            break;

        case DoLoopKind::DoLoopUntil:
            c_.emitLine("do {");
            c_.indent();
            emitStmtList(node.body);
            c_.dedent();
            if (node.condition) {
                emitExpr(*node.condition);
                c_.emitLine("} while (!(" + lastExpr_ + "));");
            } else {
                c_.emitLine("} while (1);");
            }
            break;

        case DoLoopKind::DoLoop:
            c_.emitLine("do {");
            c_.indent();
            emitStmtList(node.body);
            c_.dedent();
            c_.emitLine("} while (1);");
            break;
    }
    c_.emitLine(exitLabel + ":;  /* Exit Do target */");

    loopStack_.pop_back();
}

void CCodeGen::visit(WhileWendStmt& node) {
    // P14.3.2: 嵌套循环栈 (While...Wend 等同于 Do While...Loop)
    std::string exitLabel = "vb6_loop_exit_" + std::to_string(labelCounter_++);
    loopStack_.push_back({ExitKind::Do, exitLabel});

    emitExpr(*node.condition);
    c_.emitLine("while (" + lastExpr_ + ") {");
    c_.indent();
    emitStmtList(node.body);
    c_.dedent();
    c_.emitLine("}");
    c_.emitLine(exitLabel + ":;  /* Exit Do target */");

    loopStack_.pop_back();
}

} // namespace vb6c3
