// ast_printer_visitor_support.inc - PrintVisitor 类体片段：private 成员与派发 helper
// 由 src/ast/ast_printer.cpp 拆出（2026-09-17），在 `class PrintVisitor` 的类体内被 #include，非独立编译单元。
// 内容 = 原文件第 443~570 行，逐行未改。

private:
    std::ostream& os_;
    int indent_ = 0;

    std::ostream& out() {
        os_ << std::string(indent_ * 2, ' ');
        return os_;
    }

    static const char* accessStr(AccessLevel a) {
        switch (a) {
        case AccessLevel::Public:  return "Public";
        case AccessLevel::Private: return "Private";
        case AccessLevel::Friend:  return "Friend";
        default: return "?";
        }
    }

    void printExprBrief(Expr& expr) {
        switch (expr.kind) {
        case ASTNodeKind::IdentifierExpr:
            os_ << static_cast<IdentifierExpr&>(expr).name;
            break;
        case ASTNodeKind::MemberAccessExpr:
            printExprBrief(*static_cast<MemberAccessExpr&>(expr).object);
            os_ << "." << static_cast<MemberAccessExpr&>(expr).memberName;
            break;
        default:
            os_ << "...";
            break;
        }
    }

    void visitExprHelper(Expr& e) {
        switch (e.kind) {
        case ASTNodeKind::BinaryExpr:        visit(static_cast<BinaryExpr&>(e)); break;
        case ASTNodeKind::UnaryExpr:         visit(static_cast<UnaryExpr&>(e)); break;
        case ASTNodeKind::LiteralExpr:       visit(static_cast<LiteralExpr&>(e)); break;
        case ASTNodeKind::IdentifierExpr:    visit(static_cast<IdentifierExpr&>(e)); break;
        case ASTNodeKind::MemberAccessExpr:  visit(static_cast<MemberAccessExpr&>(e)); break;
        case ASTNodeKind::DictionaryAccessExpr: visit(static_cast<DictionaryAccessExpr&>(e)); break;
        case ASTNodeKind::IndexOrCallExpr:   visit(static_cast<IndexOrCallExpr&>(e)); break;
        case ASTNodeKind::NewExpr:           visit(static_cast<NewExpr&>(e)); break;
        case ASTNodeKind::TypeOfExpr:        visit(static_cast<TypeOfExpr&>(e)); break;
        case ASTNodeKind::AddressOfExpr:     visit(static_cast<AddressOfExpr&>(e)); break;
        case ASTNodeKind::MeExpr:            visit(static_cast<MeExpr&>(e)); break;
        case ASTNodeKind::WithMemberExpr:    visit(static_cast<WithMemberExpr&>(e)); break;
        default: out() << "UnknownExpr\n"; break;
        }
    }

    void visitStmtHelper(Stmt& s) {
        switch (s.kind) {
        case ASTNodeKind::Block:           visit(static_cast<Block&>(s)); break;
        case ASTNodeKind::AssignmentStmt:  visit(static_cast<AssignmentStmt&>(s)); break;
        case ASTNodeKind::SetStmt:         visit(static_cast<SetStmt&>(s)); break;
        case ASTNodeKind::LetStmt:         visit(static_cast<LetStmt&>(s)); break;
        case ASTNodeKind::IfStmt:          visit(static_cast<IfStmt&>(s)); break;
        case ASTNodeKind::ForStmt:         visit(static_cast<ForStmt&>(s)); break;
        case ASTNodeKind::ForEachStmt:     visit(static_cast<ForEachStmt&>(s)); break;
        case ASTNodeKind::DoLoopStmt:      visit(static_cast<DoLoopStmt&>(s)); break;
        case ASTNodeKind::WhileWendStmt:   visit(static_cast<WhileWendStmt&>(s)); break;
        case ASTNodeKind::SelectCaseStmt:  visit(static_cast<SelectCaseStmt&>(s)); break;
        case ASTNodeKind::WithStmt:        visit(static_cast<WithStmt&>(s)); break;
        case ASTNodeKind::GoToStmt:        visit(static_cast<GoToStmt&>(s)); break;
        case ASTNodeKind::GoSubStmt:       visit(static_cast<GoSubStmt&>(s)); break;
        case ASTNodeKind::ReturnStmt:      visit(static_cast<ReturnStmt&>(s)); break;
        case ASTNodeKind::OnErrorStmt:     visit(static_cast<OnErrorStmt&>(s)); break;
        case ASTNodeKind::ExitStmt:        visit(static_cast<ExitStmt&>(s)); break;
        case ASTNodeKind::StopStmt:        visit(static_cast<StopStmt&>(s)); break;
        case ASTNodeKind::EndStmt:         visit(static_cast<EndStmt&>(s)); break;
        case ASTNodeKind::CallStmt:        visit(static_cast<CallStmt&>(s)); break;
        case ASTNodeKind::ReDimStmt:       visit(static_cast<ReDimStmt&>(s)); break;
        case ASTNodeKind::EraseStmt:       visit(static_cast<EraseStmt&>(s)); break;
        case ASTNodeKind::LabelStmt:       visit(static_cast<LabelStmt&>(s)); break;
        case ASTNodeKind::RaiseEventStmt:  visit(static_cast<RaiseEventStmt&>(s)); break;
        case ASTNodeKind::OptionStmt:      visit(static_cast<OptionStmt&>(s)); break;
        case ASTNodeKind::ImplementsStmt:  visit(static_cast<ImplementsStmt&>(s)); break;
        case ASTNodeKind::DefTypeStmt:     visit(static_cast<DefTypeStmt&>(s)); break;
        case ASTNodeKind::AttributeStmt:   visit(static_cast<AttributeStmt&>(s)); break;
        case ASTNodeKind::BeepStmt:        visit(static_cast<BeepStmt&>(s)); break;
        case ASTNodeKind::DoEventsStmt:    visit(static_cast<DoEventsStmt&>(s)); break;
        case ASTNodeKind::LocalDeclStmt:   visit(static_cast<LocalDeclStmt&>(s)); break;
        case ASTNodeKind::OnGoToStmt:      visit(static_cast<OnGoToStmt&>(s)); break;
        case ASTNodeKind::OnGoSubStmt:     visit(static_cast<OnGoSubStmt&>(s)); break;
        case ASTNodeKind::MidStmt:        visit(static_cast<MidStmt&>(s)); break;  // P18-A
        case ASTNodeKind::NameStmt:        visit(static_cast<NameStmt&>(s)); break;
        // 文件I/O
        case ASTNodeKind::OpenStmt:        visit(static_cast<OpenStmt&>(s)); break;
        case ASTNodeKind::CloseStmt:       visit(static_cast<CloseStmt&>(s)); break;
        case ASTNodeKind::GetStmt:         visit(static_cast<GetStmt&>(s)); break;
        case ASTNodeKind::PutStmt:         visit(static_cast<PutStmt&>(s)); break;
        case ASTNodeKind::InputStmt:       visit(static_cast<InputStmt&>(s)); break;
        case ASTNodeKind::PrintStmt:       visit(static_cast<PrintStmt&>(s)); break;
        case ASTNodeKind::WriteStmt:       visit(static_cast<WriteStmt&>(s)); break;
        case ASTNodeKind::LineInputStmt:   visit(static_cast<LineInputStmt&>(s)); break;
        case ASTNodeKind::WidthStmt:       visit(static_cast<WidthStmt&>(s)); break;
        case ASTNodeKind::SeekStmt:        visit(static_cast<SeekStmt&>(s)); break;
        case ASTNodeKind::LockStmt:        visit(static_cast<LockStmt&>(s)); break;
        case ASTNodeKind::UnlockStmt:      visit(static_cast<UnlockStmt&>(s)); break;
        case ASTNodeKind::ResetStmt:        visit(static_cast<ResetStmt&>(s)); break;
        case ASTNodeKind::FileCopyStmt:    visit(static_cast<FileCopyStmt&>(s)); break;
        case ASTNodeKind::KillStmt:        visit(static_cast<KillStmt&>(s)); break;
        case ASTNodeKind::MkDirStmt:       visit(static_cast<MkDirStmt&>(s)); break;
        case ASTNodeKind::RmDirStmt:       visit(static_cast<RmDirStmt&>(s)); break;
        case ASTNodeKind::ChDirStmt:       visit(static_cast<ChDirStmt&>(s)); break;
        case ASTNodeKind::ChDriveStmt:     visit(static_cast<ChDriveStmt&>(s)); break;
        default: out() << "UnknownStmt\n"; break;
        }
    }

    void visitDeclHelper(Decl& d) {
        switch (d.kind) {
        case ASTNodeKind::SubDecl:       visit(static_cast<SubDecl&>(d)); break;
        case ASTNodeKind::FunctionDecl:  visit(static_cast<FunctionDecl&>(d)); break;
        case ASTNodeKind::PropertyDecl:  visit(static_cast<PropertyDecl&>(d)); break;
        case ASTNodeKind::TypeDecl:      visit(static_cast<TypeDecl&>(d)); break;
        case ASTNodeKind::TypeMember:    visit(static_cast<TypeMember&>(d)); break;
        case ASTNodeKind::EnumDecl:      visit(static_cast<EnumDecl&>(d)); break;
        case ASTNodeKind::EnumMember:    visit(static_cast<EnumMember&>(d)); break;
        case ASTNodeKind::DeclareDecl:   visit(static_cast<DeclareDecl&>(d)); break;
        case ASTNodeKind::EventDecl:     visit(static_cast<EventDecl&>(d)); break;
        case ASTNodeKind::DelegateDecl:  visit(static_cast<DelegateDecl&>(d)); break;
        case ASTNodeKind::InterfaceDecl: visit(static_cast<InterfaceDecl&>(d)); break;
        case ASTNodeKind::ConstDecl:     visit(static_cast<ConstDecl&>(d)); break;
        case ASTNodeKind::VariableDecl:  visit(static_cast<VariableDecl&>(d)); break;
        case ASTNodeKind::ParameterDecl: visit(static_cast<ParameterDecl&>(d)); break;
        default: out() << "UnknownDecl\n"; break;
        }
    }