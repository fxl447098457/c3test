#pragma once
// vb6c3 - AST 克隆 + 类型变量替换 (泛型单态化基建, G2/G3)
//
// 设计 (计划冻结版): 泛型模板不进主管线; 泛型器 (driver_generics) 对每个使用
// 点把模板声明 深拷贝 一份, 拷贝过程中把类型参数名 (T/U…) 就地替换为具体实参,
// 产物是一条普通声明 —— 语义/cgen 对泛型零感知. 因此这里不做"克隆后遍历替换",
// 而是 单趟 clone+subst: 每个节点构造时其子节点已是替换后的新副本.
//
// 覆盖面: G2 = TypeRef/常量表达式/TypeDecl; G3 起 = 表达式与语句全域 +
// Sub/Function/Property 声明克隆. 未覆盖 kind 一律置 failed_ + 返回 nullptr
// (泛型器统一报 unsupported, 绝不静默产坏码).

#include "ast/ast.hpp"
#include <map>
#include <string>
#include <vector>
#include <memory>

namespace vb6c3 {

class ASTCloner {
public:
    // typeVarLower → 实参 TypeRef (宿主生命周期内有效, 本类读取时深拷贝)
    void bind(const std::string& typeVarLower, const ASTNode* subst) {
        subst_[typeVarLower] = subst;
    }
    // 过程模板专用: 旧过程名(lower) → 特化扁名. VB 里"给函数名赋值"即给返回值
    // 赋值, 克隆体必须连同标识符引用一起改名, 否则 self-assign 落到无主裸名.
    void bindProcSelf(const std::string& oldNameLower, const std::string& newName) {
        idRename_[oldNameLower] = newName;
    }

    // nullptr 透传; 未支持 kind → failed_ + nullptr
    TypeRefPtr cloneTypeRef(const ASTNode* ref);
    ExprPtr cloneExpr(const Expr* e);
    StmtPtr cloneStmt(const Stmt* s);
    StmtList cloneStmtList(const StmtList& list);

    std::unique_ptr<TypeDecl> cloneTypeDecl(const TypeDecl& d, const std::string& newName);
    std::unique_ptr<SubDecl> cloneSubDecl(const SubDecl& d, const std::string& newName);
    std::unique_ptr<FunctionDecl> cloneFunctionDecl(const FunctionDecl& d, const std::string& newName);
    std::unique_ptr<PropertyDecl> clonePropertyDecl(const PropertyDecl& d, const std::string& newName);
    // 泛型类 (G4): 整模块特化克隆 — moduleName/Attribute VB_Name 改扁名,
    // 声明逐条 clone+subst. v1 只收 类字段/Const/Sub/Function/Property/Type;
    // 其余成员形态由调用方 (driver_generics) 先行拒绝, 这里再兜底置 failed_.
    std::unique_ptr<Module> cloneModule(const Module& m, const std::string& newName);

    bool failed() const { return failed_; }

private:
    std::unique_ptr<ParameterDecl> cloneParam(const ParameterDecl& p);
    DeclPtr cloneDeclAny(const Decl* d);           // LocalDeclStmt 用
    ExprPtr cloneExprInner(const Expr* e);
    std::vector<ExprPtr> cloneExprVec(const std::vector<ExprPtr>& v);
    // 名字位替换 (NewExpr.className / TypeOfExpr.typeName 等字符串形态的类型引用)
    std::string substName(const std::string& name) const;

    std::map<std::string, const ASTNode*> subst_;
    std::map<std::string, std::string> idRename_;  // 过程模板: lower(旧名)→扁名
    bool failed_ = false;
};

std::string toLowerStr(const std::string& s);

} // namespace vb6c3
