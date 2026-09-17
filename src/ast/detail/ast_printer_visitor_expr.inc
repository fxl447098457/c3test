// ast_printer_visitor_expr.inc - PrintVisitor 类体片段：表达式与类型引用节点 visit
// 由 src/ast/ast_printer.cpp 拆出（2026-09-17），在 `class PrintVisitor` 的类体内被 #include，非独立编译单元。
// 内容 = 原文件第 350~442 行，逐行未改。

    // --- 表达式 ---

    void visit(BinaryExpr& node) override {
        out() << "Binary(" << binaryOpToString(node.op) << ")\n";
        indent_++;
        visitExprHelper(*node.left);
        visitExprHelper(*node.right);
        indent_--;
    }

    void visit(UnaryExpr& node) override {
        out() << "Unary(" << unaryOpToString(node.op) << ")\n";
        indent_++;
        visitExprHelper(*node.operand);
        indent_--;
    }

    void visit(LiteralExpr& node) override {
        out() << "Literal(" << literalKindToString(node.literalKind)
              << "): " << node.rawText << "\n";
    }

    void visit(IdentifierExpr& node) override {
        out() << "Id: " << node.name;
        if (node.bracketed) os_ << " [bracketed]";
        os_ << "\n";
    }

    void visit(MemberAccessExpr& node) override {
        out() << "MemberAccess(." << node.memberName << ")\n";
        indent_++;
        visitExprHelper(*node.object);
        indent_--;
    }

    void visit(DictionaryAccessExpr& node) override {
        out() << "DictAccess(!" << node.key << ")\n";
        indent_++;
        visitExprHelper(*node.object);
        indent_--;
    }

    void visit(IndexOrCallExpr& node) override {
        out() << "IndexOrCall";
        if (!node.named.empty()) os_ << " [has named args]";
        os_ << "\n";
        indent_++;
        visitExprHelper(*node.callee);
        for (auto& arg : node.positional) visitExprHelper(*arg);
        indent_--;
    }

    void visit(NewExpr& node) override {
        out() << "New " << node.className << "\n";
    }

    void visit(TypeOfExpr& node) override {
        out() << "TypeOf ... Is " << node.typeName << "\n";
    }

    void visit(AddressOfExpr& node) override {
        out() << "AddressOf " << node.funcName << "\n";
    }

    void visit(MeExpr&) override {
        out() << "Me\n";
    }

    void visit(WithMemberExpr& node) override {
        out() << "WithMember(." << node.memberName << ")\n";
    }

    // --- 类型引用 ---

    void visit(SimpleTypeRef& node) override {
        out() << "Type: " << node.name << "\n";
    }

    void visit(ArrayTypeRef& node) override {
        out() << "ArrayType (dims=" << node.dimensions.size() << ")\n";
    }

    void visit(FixedStringTypeRef& node) override {
        out() << "FixedString\n";
    }

    void visit(LocalDeclStmt& node) override {
        out() << "LocalDecl\n";
        indent_++;
        visitDeclHelper(*node.decl);
        indent_--;
    }
