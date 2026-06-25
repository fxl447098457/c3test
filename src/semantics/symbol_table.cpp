#include "semantics/symbol_table.hpp"
#include <algorithm>

namespace vb6c3 {

// ============================================================
// Scope
// ============================================================

bool Scope::define(std::unique_ptr<Symbol> sym) {
    auto it = symbols_.find(sym->lowerName);
    if (it != symbols_.end()) {
        return false;  // 重定义
    }
    symbols_[sym->lowerName] = std::move(sym);
    return true;
}

Symbol* Scope::lookupLocal(const std::string& name) const {
    std::string lower = Symbol::toLower(name);
    auto it = symbols_.find(lower);
    if (it != symbols_.end()) {
        return it->second.get();
    }
    return nullptr;
}

Symbol* Scope::lookup(const std::string& name) const {
    // 先在本作用域查找
    if (auto sym = lookupLocal(name)) {
        return sym;
    }
    // 递归向父作用域查找
    if (parent_) {
        return parent_->lookup(name);
    }
    return nullptr;
}

// ============================================================
// SymbolTable
// ============================================================

SymbolTable::SymbolTable(Diagnostics& diag)
    : diag_(diag)
{
    // 创建模块级作用域
    auto modScope = std::make_unique<Scope>(ScopeKind::Module);
    moduleScope_ = modScope.get();
    current_ = moduleScope_;
    scopes_.push_back(std::move(modScope));
}

void SymbolTable::pushScope(ScopeKind kind) {
    auto scope = std::make_unique<Scope>(kind, current_);
    current_ = scope.get();
    scopes_.push_back(std::move(scope));
}

void SymbolTable::popScope() {
    if (current_ && current_->parent()) {
        current_ = current_->parent();
    }
}

bool SymbolTable::define(std::unique_ptr<Symbol> sym) {
    if (!current_) return false;

    std::string lowerName = sym->lowerName;
    SourceLocation loc = sym->location;

    if (!current_->define(std::move(sym))) {
        diag_.error(DiagnosticID::SemDuplicateDeclaration, loc,
            "重复声明: '" + lowerName + "'");
        return false;
    }
    return true;
}

Symbol* SymbolTable::lookup(const std::string& name) const {
    if (!current_) return nullptr;
    return current_->lookup(name);
}

Symbol* SymbolTable::lookupLocal(const std::string& name) const {
    if (!current_) return nullptr;
    return current_->lookupLocal(name);
}

Symbol* SymbolTable::lookupModule(const std::string& name) const {
    if (!moduleScope_) return nullptr;
    return moduleScope_->lookupLocal(name);
}

int SymbolTable::scopeDepth() const {
    int depth = 0;
    Scope* s = current_;
    while (s && s != moduleScope_) {
        depth++;
        s = s->parent();
    }
    return depth;
}

ScopeKind SymbolTable::currentScopeKind() const {
    return current_ ? current_->kind() : ScopeKind::Module;
}

bool SymbolTable::inProcedure() const {
    Scope* s = current_;
    while (s) {
        if (s->kind() == ScopeKind::Procedure) return true;
        s = s->parent();
    }
    return false;
}

} // namespace vb6c3
