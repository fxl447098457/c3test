// ast_printer_visitor_decl.inc - PrintVisitor 类体片段：声明类节点 visit
// 由 src/ast/ast_printer.cpp 拆出（2026-09-17），在 `class PrintVisitor` 的类体内被 #include，非独立编译单元。
// 内容 = 原文件第 35~127 行，逐行未改。

    // --- 声明 ---

    void visit(SubDecl& node) override {
        out() << "Sub " << node.name
              << " (access=" << accessStr(node.access) << ")";
        if (node.isStatic) os_ << " Static";
        os_ << "\n";
        indent_++;
        for (auto& p : node.params) visitDeclHelper(*p);
        for (auto& s : node.body) visitStmtHelper(*s);
        indent_--;
    }

    void visit(FunctionDecl& node) override {
        out() << "Function " << node.name
              << " (access=" << accessStr(node.access) << ")";
        if (node.isStatic) os_ << " Static";
        os_ << "\n";
        indent_++;
        for (auto& p : node.params) visitDeclHelper(*p);
        for (auto& s : node.body) visitStmtHelper(*s);
        indent_--;
    }

    void visit(PropertyDecl& node) override {
        out() << "Property " << procKindToString(node.propKind)
              << " " << node.name
              << " (access=" << accessStr(node.access) << ")\n";
        indent_++;
        for (auto& p : node.params) visitDeclHelper(*p);
        for (auto& s : node.body) visitStmtHelper(*s);
        indent_--;
    }

    void visit(TypeDecl& node) override {
        out() << "Type " << node.name
              << " (access=" << accessStr(node.access) << ")\n";
        indent_++;
        for (auto& m : node.members) visitDeclHelper(*m);
        indent_--;
    }

    void visit(TypeMember& node) override {
        out() << node.name << " As ...";
        os_ << "\n";
    }

    void visit(EnumDecl& node) override {
        out() << "Enum " << node.name
              << " (access=" << accessStr(node.access) << ")\n";
        indent_++;
        for (auto& m : node.members) visitDeclHelper(*m);
        indent_--;
    }

    void visit(EnumMember& node) override {
        out() << node.name;
        if (node.value) os_ << " = ...";
        os_ << "\n";
    }

    void visit(DeclareDecl& node) override {
        out() << "Declare " << procKindToString(node.procKind)
              << " " << node.name << " Lib \"" << node.libName << "\"";
        if (!node.aliasName.empty()) os_ << " Alias \"" << node.aliasName << "\"";
        os_ << "\n";
    }

    void visit(EventDecl& node) override {
        out() << "Event " << node.name << "\n";
    }

    void visit(DelegateDecl& node) override {
        out() << "Delegate " << procKindToString(node.procKind)
              << " " << node.name;
        if (node.callingConv == CallConv::CDecl) os_ << " CDecl";
        os_ << " (" << node.params.size() << " params)\n";
    }

    void visit(ConstDecl& node) override {
        out() << "Const " << node.name
              << " (access=" << accessStr(node.access) << ")\n";
    }

    void visit(VariableDecl& node) override {
        out() << "Dim " << node.name
              << " (access=" << accessStr(node.access) << ")";
        if (!node.dimensions.empty()) os_ << " [array]";
        os_ << "\n";
    }

    void visit(ParameterDecl& node) override {
        out() << "Param " << node.name;
        if (node.isByVal) os_ << " ByVal";
        else os_ << " ByRef";
        if (node.isOptional) os_ << " Optional";
        if (node.isParamArray) os_ << " ParamArray";
        os_ << "\n";
    }
