// ast_printer_visitor_stmt.inc - PrintVisitor 类体片段：语句类节点 visit
// 由 src/ast/ast_printer.cpp 拆出（2026-09-17），在 `class PrintVisitor` 的类体内被 #include，非独立编译单元。
// 内容 = 原文件第 128~349 行，逐行未改。

    // --- 语句 ---

    void visit(Block& node) override {
        out() << "Block\n";
        indent_++;
        for (auto& s : node.stmts) visitStmtHelper(*s);
        indent_--;
    }

    void visit(AssignmentStmt& node) override {
        out() << "Assign: ";
        printExprBrief(*node.target);
        os_ << " = ";
        if (node.value) { printExprBrief(*node.value); }
        else { os_ << "<null>"; }
        os_ << "\n";
    }

    void visit(SetStmt& node) override {
        out() << "Set: ";
        printExprBrief(*node.target);
        os_ << " = ...\n";
    }

    void visit(LetStmt& node) override {
        out() << "Let: ";
        printExprBrief(*node.target);
        os_ << " = ...\n";
    }

    void visit(IfStmt& node) override {
        out() << "If";
        if (node.singleLine) os_ << " (single-line)";
        os_ << "\n";
        indent_++;
        for (auto& s : node.thenBody) visitStmtHelper(*s);
        indent_--;
        for (auto& elif : node.elseIfs) {
            visit(*elif);
        }
        if (!node.elseBody.empty()) {
            out() << "Else\n";
            indent_++;
            for (auto& s : node.elseBody) visitStmtHelper(*s);
            indent_--;
        }
    }

    void visit(ElseIfClause& node) override {
        out() << "ElseIf\n";
        indent_++;
        for (auto& s : node.body) visitStmtHelper(*s);
        indent_--;
    }

    void visit(ForStmt& node) override {
        out() << "For " << node.varName << " = ... To ...\n";
        indent_++;
        for (auto& s : node.body) visitStmtHelper(*s);
        indent_--;
    }

    void visit(ForEachStmt& node) override {
        out() << "For Each " << node.varName << " In ...\n";
        indent_++;
        for (auto& s : node.body) visitStmtHelper(*s);
        indent_--;
    }

    void visit(DoLoopStmt& node) override {
        out() << "DoLoop (kind=" << static_cast<int>(node.loopKind) << ")\n";
        indent_++;
        for (auto& s : node.body) visitStmtHelper(*s);
        indent_--;
    }

    void visit(WhileWendStmt& node) override {
        out() << "While...Wend\n";
        indent_++;
        for (auto& s : node.body) visitStmtHelper(*s);
        indent_--;
    }

    void visit(SelectCaseStmt& node) override {
        out() << "Select Case\n";
        indent_++;
        for (auto& c : node.cases) visit(*c);
        if (!node.elseCase.empty()) {
            out() << "Case Else\n";
            indent_++;
            for (auto& s : node.elseCase) visitStmtHelper(*s);
            indent_--;
        }
        indent_--;
    }

    void visit(CaseClause& node) override {
        out() << "Case\n";
        indent_++;
        for (auto& s : node.body) visitStmtHelper(*s);
        indent_--;
    }

    void visit(WithStmt& node) override {
        out() << "With\n";
        indent_++;
        for (auto& s : node.body) visitStmtHelper(*s);
        indent_--;
    }

    void visit(GoToStmt& node) override {
        out() << "GoTo " << node.labelName << "\n";
    }

    void visit(GoSubStmt& node) override {
        out() << "GoSub " << node.labelName << "\n";
    }

    void visit(ReturnStmt&) override {
        out() << "Return\n";
    }

    void visit(OnErrorStmt& node) override {
        out() << "On Error";
        switch (node.errorKind) {
        case OnErrorKind::GoToLabel:  os_ << " GoTo " << node.labelName; break;
        case OnErrorKind::ResumeNext: os_ << " Resume Next"; break;
        case OnErrorKind::GoToZero:   os_ << " GoTo 0"; break;
        }
        os_ << "\n";
    }

    void visit(OnGoToStmt& node) override {
        out() << "On ... GoTo\n";
    }

    void visit(OnGoSubStmt& node) override {
        out() << "On ... GoSub\n";
    }

    void visit(MidStmt& node) override {  // P18-A
        out() << "Mid$ Statement\n";
    }

    void visit(ExitStmt& node) override {
        out() << "Exit " << exitKindToString(node.exitKind) << "\n";
    }

    void visit(StopStmt&) override { out() << "Stop\n"; }
    void visit(AsmStmt& node) override {
        out() << "Asm (" << node.lines.size() << " lines"
              << (node.naked ? ", Naked" : "") << ")\n";
    }
    void visit(EndStmt&) override  { out() << "End\n"; }

    void visit(CallStmt& node) override {
        out() << "Call: ";
        printExprBrief(*node.callee);
        os_ << "\n";
    }

    void visit(ReDimStmt& node) override {
        out() << "ReDim " << (node.preserve ? "Preserve " : "") << node.varName;
        // Fix 100: 复杂目标 (arr(i).Field) 附加标记, 完整左值见 targetExpr 子树
        if (node.targetExpr) out() << "(target-expr)";
        out() << "\n";
    }

    void visit(EraseStmt& node) override {
        out() << "Erase\n";
    }

    void visit(LabelStmt& node) override {
        out() << node.labelName << ":\n";
    }

    void visit(RaiseEventStmt& node) override {
        out() << "RaiseEvent " << node.eventName << "\n";
    }

    // 文件I/O (简要输出)
    void visit(OpenStmt&) override    { out() << "Open\n"; }
    void visit(CloseStmt&) override   { out() << "Close\n"; }
    void visit(GetStmt&) override     { out() << "Get\n"; }
    void visit(PutStmt&) override     { out() << "Put\n"; }
    void visit(InputStmt&) override   { out() << "Input\n"; }
    void visit(PrintStmt&) override   { out() << "Print\n"; }
    void visit(WriteStmt&) override   { out() << "Write\n"; }
    void visit(LineInputStmt&) override { out() << "Line Input\n"; }
    void visit(WidthStmt&) override   { out() << "Width\n"; }
    void visit(SeekStmt&) override    { out() << "Seek\n"; }
    void visit(LockStmt&) override    { out() << "Lock\n"; }
    void visit(UnlockStmt&) override  { out() << "Unlock\n"; }
    void visit(ResetStmt&) override   { out() << "Reset\n"; }
    void visit(NameStmt&) override    { out() << "Name\n"; }
    void visit(FileCopyStmt&) override { out() << "FileCopy\n"; }
    void visit(KillStmt&) override    { out() << "Kill\n"; }
    void visit(MkDirStmt&) override   { out() << "MkDir\n"; }
    void visit(RmDirStmt&) override   { out() << "RmDir\n"; }
    void visit(ChDirStmt&) override   { out() << "ChDir\n"; }
    void visit(ChDriveStmt&) override { out() << "ChDrive\n"; }
    void visit(BeepStmt&) override    { out() << "Beep\n"; }
    void visit(DoEventsStmt&) override{ out() << "DoEvents\n"; }

    void visit(AttributeStmt& node) override {
        out() << "Attribute " << node.attrName << "\n";
    }

    void visit(OptionStmt& node) override {
        out() << "Option ";
        switch (node.optionKind) {
        case OptionKind::Explicit:      os_ << "Explicit"; break;
        case OptionKind::CompareText:   os_ << "Compare Text"; break;
        case OptionKind::CompareBinary: os_ << "Compare Binary"; break;
        case OptionKind::BaseZero:      os_ << "Base 0"; break;
        case OptionKind::BaseOne:       os_ << "Base 1"; break;
        case OptionKind::PrivateModule: os_ << "Private Module"; break;
        }
        os_ << "\n";
    }

    void visit(ImplementsStmt& node) override {
        out() << "Implements " << node.interfaceName << "\n";
    }

    void visit(DefTypeStmt& node) override {
        out() << "DefType\n";
    }
