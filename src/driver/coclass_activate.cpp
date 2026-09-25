// vb6c3 - CoClass 组内名字激活的实现 (ai/026 五-3/4/5, ai/022 D54, 批次 B11/C05)
// 口径与设计依据见 coclass_activate.hpp 头注。

#include "driver/coclass_activate.hpp"

#include "ast/ast.hpp"
#include "ast/ast_visitor.hpp"
#include "common/diagnostics.hpp"
#include "semantics/interface_sig.hpp"   // ifaceLower

#include <iostream>
#include <unordered_map>
#include <unordered_set>

namespace vb6c3 {
namespace {

struct Ctx {
    const std::vector<CoClassActivation>* acts = nullptr;
    Diagnostics* diag = nullptr;
    std::unordered_map<std::string, size_t> byName;     // lower(块名) -> act 下标
    std::unordered_map<std::string, size_t> byProgId;   // lower(ProgID) -> act 下标
    std::unordered_set<std::string> implLess;           // lower(块名): 没有实现类的块
    std::vector<size_t> refs;                           // 按 act 下标: 改掉的类型位点
    std::vector<size_t> swaps;                          // 按 act 下标: 改掉的 CreateObject
};

std::string unquote(const std::string& raw) {
    if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"') {
        return raw.substr(1, raw.size() - 2);
    }
    return raw;
}

// 一个写在类型位置上的名字。命中别名就地改名; 命中"没有 [Implementation] 的块"当场判死。
void rewriteName(Ctx& c, std::string& name, const SourceLocation& loc) {
    const std::string key = ifaceLower(name);
    auto hit = c.byName.find(key);
    if (hit != c.byName.end()) {
        const CoClassActivation& a = (*c.acts)[hit->second];
        if (ifaceLower(a.implName) != key) {   // 块名与实现类同名时不必改 (也不该改)
            name = a.implName;
            c.refs[hit->second]++;
        }
        return;
    }
    if (c.implLess.count(key) && c.diag) {
        c.diag->error(DiagnosticID::SemCoClassTypeUnbound, loc,
            "'" + name + "' is a CoClass block that declares no [Implementation] class, so it "
            "cannot be used as a type (bind the variable to the implementation class, or give "
            "the block [Implementation(\"ClassName\")])");
        c.implLess.erase(key);   // 同一个块只报一次，免得刷屏
    }
}

// TypeRef 是个 ASTNode 基类指针: 简单名直接改, 数组名往元素类型里钻一层。
void rewriteTypeRef(Ctx& c, ASTNode* ref) {
    if (!ref) return;
    if (ref->kind == ASTNodeKind::SimpleTypeRef) {
        auto* s = static_cast<SimpleTypeRef*>(ref);
        rewriteName(c, s->name, s->loc);
        return;
    }
    if (ref->kind == ASTNodeKind::ArrayTypeRef) {
        rewriteTypeRef(c, static_cast<ArrayTypeRef*>(ref)->elementType.get());
    }
}

// 表达式里的类型名只有两处是字符串形态 (New 的类名 / TypeOf 的目标名)。
class ExprRewriter : public ASTVisitor {
public:
    explicit ExprRewriter(Ctx& c) : c_(c) {}
    void visit(NewExpr& node) override { rewriteName(c_, node.className, node.loc); }
    void visit(TypeOfExpr& node) override { rewriteName(c_, node.typeName, node.loc); }

private:
    Ctx& c_;
};

void rewriteExpr(Ctx& c, ExprRewriter& v, Expr* e) {
    if (e) traverseExpr(*e, v);
}

// `Set x = CreateObject("本工程的 ProgID")` → `New <实现类>`（026 五-5: 编译期改写，
// 不做运行期查表）。只认"整个右值就是这一枚调用"的惯用形 —— 嵌在更大的表达式里
// （`Foo(CreateObject("p.c"))`）今天照旧走注册表，边界与理由记 D54-④。
bool swapCreateObject(Ctx& c, ExprPtr& slot) {
    if (!slot || slot->kind != ASTNodeKind::IndexOrCallExpr) return false;
    auto* call = static_cast<IndexOrCallExpr*>(slot.get());
    if (!call->callee || call->callee->kind != ASTNodeKind::IdentifierExpr) return false;
    if (ifaceLower(static_cast<IdentifierExpr*>(call->callee.get())->name) != "createobject") {
        return false;
    }
    if (call->positional.size() != 1 || !call->named.empty()) return false;
    Expr* arg = call->positional[0].get();
    if (!arg || arg->kind != ASTNodeKind::LiteralExpr) return false;
    auto* lit = static_cast<LiteralExpr*>(arg);
    if (lit->literalKind != LiteralKind::String) return false;
    auto hit = c.byProgId.find(ifaceLower(unquote(lit->rawText)));
    if (hit == c.byProgId.end()) return false;
    const CoClassActivation& a = (*c.acts)[hit->second];
    slot = std::make_unique<NewExpr>(slot->loc, a.implName);
    c.swaps[hit->second]++;
    return true;
}

void rewriteStmt(Ctx& c, ExprRewriter& v, Stmt& s);

void rewriteStmtList(Ctx& c, ExprRewriter& v, StmtList& list) {
    for (auto& s : list) if (s) rewriteStmt(c, v, *s);
}

void rewriteParamList(Ctx& c, ExprRewriter& v,
                      std::vector<std::unique_ptr<ParameterDecl>>& params) {
    for (auto& p : params) {
        if (!p) continue;
        rewriteTypeRef(c, p->asType.get());
        rewriteExpr(c, v, p->defaultValue.get());
    }
}

void rewriteDecl(Ctx& c, ExprRewriter& v, Decl& d) {
    switch (d.kind) {
    case ASTNodeKind::VariableDecl: {
        auto& x = static_cast<VariableDecl&>(d);
        rewriteTypeRef(c, x.asType.get());              // 含 `Dim x As New <块名>`
        for (auto& dim : x.dimensions) {
            rewriteExpr(c, v, dim.lower.get());
            rewriteExpr(c, v, dim.upper.get());
        }
        rewriteExpr(c, v, x.initializer.get());
        break;
    }
    case ASTNodeKind::ConstDecl: {
        auto& x = static_cast<ConstDecl&>(d);
        rewriteTypeRef(c, x.asType.get());
        rewriteExpr(c, v, x.value.get());
        break;
    }
    case ASTNodeKind::SubDecl: {
        auto& x = static_cast<SubDecl&>(d);
        rewriteParamList(c, v, x.params);
        rewriteStmtList(c, v, x.body);
        break;
    }
    case ASTNodeKind::FunctionDecl: {
        auto& x = static_cast<FunctionDecl&>(d);
        rewriteTypeRef(c, x.returnType.get());
        rewriteParamList(c, v, x.params);
        rewriteStmtList(c, v, x.body);
        break;
    }
    case ASTNodeKind::PropertyDecl: {
        auto& x = static_cast<PropertyDecl&>(d);
        rewriteTypeRef(c, x.returnType.get());
        rewriteParamList(c, v, x.params);
        rewriteStmtList(c, v, x.body);
        break;
    }
    case ASTNodeKind::TypeDecl: {
        auto& x = static_cast<TypeDecl&>(d);
        for (auto& m : x.members) {
            if (!m) continue;
            if (m->kind == ASTNodeKind::TypeMember) {
                rewriteTypeRef(c, static_cast<TypeMember*>(m.get())->type.get());
            }
        }
        break;
    }
    default:
        break;   // Declare/Delegate/Event/Interface: v1 不动这些位置 (D54-④)
    }
}

void rewriteStmt(Ctx& c, ExprRewriter& v, Stmt& s) {
    switch (s.kind) {
    case ASTNodeKind::Block:
        rewriteStmtList(c, v, static_cast<Block&>(s).stmts);
        break;
    case ASTNodeKind::LocalDeclStmt: {
        auto& x = static_cast<LocalDeclStmt&>(s);
        if (x.decl) rewriteDecl(c, v, *x.decl);
        break;
    }
    case ASTNodeKind::ReDimStmt: {
        auto& x = static_cast<ReDimStmt&>(s);
        rewriteTypeRef(c, x.asType.get());
        for (auto& dim : x.dimensions) {
            rewriteExpr(c, v, dim.lower.get());
            rewriteExpr(c, v, dim.upper.get());
        }
        break;
    }
    case ASTNodeKind::SetStmt: {
        auto& x = static_cast<SetStmt&>(s);
        swapCreateObject(c, x.value);
        rewriteExpr(c, v, x.target.get());
        rewriteExpr(c, v, x.value.get());
        break;
    }
    case ASTNodeKind::LetStmt: {
        auto& x = static_cast<LetStmt&>(s);
        swapCreateObject(c, x.value);
        rewriteExpr(c, v, x.target.get());
        rewriteExpr(c, v, x.value.get());
        break;
    }
    case ASTNodeKind::AssignmentStmt: {
        auto& x = static_cast<AssignmentStmt&>(s);
        swapCreateObject(c, x.value);
        rewriteExpr(c, v, x.target.get());
        rewriteExpr(c, v, x.value.get());
        break;
    }
    case ASTNodeKind::IfStmt: {
        auto& x = static_cast<IfStmt&>(s);
        rewriteExpr(c, v, x.condition.get());
        rewriteStmtList(c, v, x.thenBody);
        for (auto& cl : x.elseIfs) {
            if (!cl) continue;
            rewriteExpr(c, v, cl->condition.get());
            rewriteStmtList(c, v, cl->body);
        }
        rewriteStmtList(c, v, x.elseBody);
        break;
    }
    case ASTNodeKind::ForStmt: {
        auto& x = static_cast<ForStmt&>(s);
        rewriteExpr(c, v, x.start.get());
        rewriteExpr(c, v, x.end.get());
        rewriteExpr(c, v, x.step.get());
        rewriteStmtList(c, v, x.body);
        break;
    }
    case ASTNodeKind::ForEachStmt: {
        auto& x = static_cast<ForEachStmt&>(s);
        rewriteExpr(c, v, x.collection.get());
        rewriteStmtList(c, v, x.body);
        break;
    }
    case ASTNodeKind::DoLoopStmt: {
        auto& x = static_cast<DoLoopStmt&>(s);
        rewriteExpr(c, v, x.condition.get());
        rewriteStmtList(c, v, x.body);
        break;
    }
    case ASTNodeKind::WhileWendStmt: {
        auto& x = static_cast<WhileWendStmt&>(s);
        rewriteExpr(c, v, x.condition.get());
        rewriteStmtList(c, v, x.body);
        break;
    }
    case ASTNodeKind::SelectCaseStmt: {
        auto& x = static_cast<SelectCaseStmt&>(s);
        rewriteExpr(c, v, x.testExpr.get());
        for (auto& cl : x.cases) {
            if (!cl) continue;
            for (auto& cv : cl->values) {
                rewriteExpr(c, v, cv.value.get());
                rewriteExpr(c, v, cv.toValue.get());
            }
            rewriteStmtList(c, v, cl->body);
        }
        rewriteStmtList(c, v, x.elseCase);
        break;
    }
    case ASTNodeKind::WithStmt: {
        auto& x = static_cast<WithStmt&>(s);
        rewriteExpr(c, v, x.object.get());
        rewriteStmtList(c, v, x.body);
        break;
    }
    case ASTNodeKind::CallStmt:
        rewriteExpr(c, v, static_cast<CallStmt&>(s).callee.get());
        break;
    default:
        break;
    }
}

}  // namespace

void activateCoClassNames(std::vector<std::unique_ptr<Module>>& modules,
                          const std::vector<CoClassActivation>& acts,
                          const std::vector<std::string>& implLessBlocks,
                          Diagnostics& diag) {
    if (acts.empty() && implLessBlocks.empty()) return;

    Ctx c;
    c.acts = &acts;
    c.diag = &diag;
    c.refs.assign(acts.size(), 0);
    c.swaps.assign(acts.size(), 0);
    for (size_t i = 0; i < acts.size(); i++) {
        c.byName[ifaceLower(acts[i].blockName)] = i;
        if (!acts[i].progId.empty()) c.byProgId[ifaceLower(acts[i].progId)] = i;
    }
    for (const std::string& n : implLessBlocks) c.implLess.insert(ifaceLower(n));

    ExprRewriter v(c);
    for (auto& mod : modules) {
        if (!mod) continue;
        for (auto& decl : mod->declarations) {
            if (decl) rewriteDecl(c, v, *decl);
        }
    }

    // 可观测面 = stderr 的 `C3:` 信息行（D46-①：note 诊断在成功的编译里根本看不见）。
    // 只在**真的被用到**时打 —— 光声明块、从不把块名当类型用的工程（cc_id 就是）一个字不少。
    for (size_t i = 0; i < acts.size(); i++) {
        if (!c.refs[i] && !c.swaps[i]) continue;
        const CoClassActivation& a = acts[i];
        std::cerr << "C3: CoClass '" << a.blockName << "' activated in-project: type name -> class '"
                  << a.implName << "' (" << c.refs[i] << " type reference(s)";
        if (c.swaps[i]) std::cerr << ", " << c.swaps[i] << " CreateObject rewrite(s)";
        std::cerr << ")" << std::endl;
    }
}

}  // namespace vb6c3
