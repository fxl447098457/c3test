// ast_printer_visitor_entry.inc - PrintVisitor 类体片段：构造与 print(Module&) 入口
// 由 src/ast/ast_printer.cpp 拆出（2026-09-17），在 `class PrintVisitor` 的类体内被 #include，非独立编译单元。
// 内容 = 原文件第 22~34 行，逐行未改。

    explicit PrintVisitor(std::ostream& os) : os_(os) {}

    void print(Module& module) {
        os_ << "Module: " << module.filename << "\n";
        if (!module.classTypeParams.empty()) {
            os_ << "  [generic class template, typeParams=";
            for (size_t i = 0; i < module.classTypeParams.size(); i++) {
                if (i) os_ << ",";
                os_ << module.classTypeParams[i];
            }
            os_ << "]\n";
        }
        indent_++;
        for (auto& opt : module.options) visitStmtHelper(*opt);
        for (auto& imp : module.implements) visitStmtHelper(*imp);
        for (auto& def : module.defTypes) visitStmtHelper(*def);
        for (auto& decl : module.declarations) visitDeclHelper(*decl);
        for (auto& attr : module.attributes) visitStmtHelper(*attr);
        indent_--;
    }
