#include "ast/ast_printer.hpp"
#include "ast/ast.hpp"
#include "ast/ast_visitor.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>

namespace vb6c3 {

// ============================================================
// AST 打印器实现
// 内部使用 ASTVisitor 递归打印
// ============================================================

ASTPrinter::ASTPrinter(std::ostream& os) : os_(os) {}

namespace {

// 内部打印 Visitor
class PrintVisitor : public ASTVisitor {
public:
    explicit PrintVisitor(std::ostream& os) : os_(os) {}

    void print(Module& module) {
        os_ << "Module: " << module.filename << "\n";
        indent_++;
        for (auto& opt : module.options) visitStmtHelper(*opt);
        for (auto& imp : module.implements) visitStmtHelper(*imp);
        for (auto& def : module.defTypes) visitStmtHelper(*def);
        for (auto& decl : module.declarations) visitDeclHelper(*decl);
        for (auto& attr : module.attributes) visitStmtHelper(*attr);
        indent_--;
    }

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
    void visit(EndStmt&) override  { out() << "End\n"; }

    void visit(CallStmt& node) override {
        out() << "Call: ";
        printExprBrief(*node.callee);
        os_ << "\n";
    }

    void visit(ReDimStmt& node) override {
        out() << "ReDim " << (node.preserve ? "Preserve " : "") << node.varName << "\n";
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
        case ASTNodeKind::ConstDecl:     visit(static_cast<ConstDecl&>(d)); break;
        case ASTNodeKind::VariableDecl:  visit(static_cast<VariableDecl&>(d)); break;
        case ASTNodeKind::ParameterDecl: visit(static_cast<ParameterDecl&>(d)); break;
        default: out() << "UnknownDecl\n"; break;
        }
    }
};

} // anonymous namespace

// ASTPrinter::print 桥接到 PrintVisitor
void ASTPrinter::print(Module& module) {
    PrintVisitor visitor(os_);
    visitor.print(module);
}

} // namespace vb6c3
