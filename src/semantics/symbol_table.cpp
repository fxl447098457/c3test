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

    // P6.3: ComClass/ComInterface与用户变量可同名 (不同namespace)
    // 如果新符号是builtin的ComClass/ComInterface, 且已有同名符号, 跳过不报错
    // 如果已有符号是ComClass/ComInterface, 且新符号是用户变量, 也允许覆盖
    if (sym->isBuiltin && (sym->kind == SymbolKind::ComClass || sym->kind == SymbolKind::ComInterface)) {
        auto* existing = current_->lookupLocal(lowerName);
        if (existing) {
            // 已有同名符号, 跳过COM类型注册
            return true;  // 不报错
        }
    }
    // 如果已有同名ComClass/ComInterface, 新符号是用户变量, 先移除旧的
    {
        auto* existing = current_->lookupLocal(lowerName);
        if (existing && (existing->kind == SymbolKind::ComClass || existing->kind == SymbolKind::ComInterface)
            && sym->kind != SymbolKind::ComClass && sym->kind != SymbolKind::ComInterface) {
            // 移除COM类型符号, 允许用户变量覆盖
            current_->symbols_.erase(lowerName);
        }
    }

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

// --- 跨模块符号操作 ---

void SymbolTable::defineExternal(std::unique_ptr<Symbol> sym) {
    // 在模块级作用域定义外部符号
    // 如果已存在同名符号（本地已有定义），跳过不覆盖
    if (!moduleScope_) return;
    auto it = moduleScope_->symbols_.find(sym->lowerName);
    if (it != moduleScope_->symbols_.end()) {
        // 本地已有定义，不注入外部符号
        return;
    }
    moduleScope_->symbols_[sym->lowerName] = std::move(sym);
}

std::vector<const Symbol*> SymbolTable::getPublicSymbols() const {
    std::vector<const Symbol*> result;
    if (!moduleScope_) return result;
    for (const auto& [key, sym] : moduleScope_->symbols()) {
        if (sym->access == AccessLevel::Public && !sym->isBuiltin) {
            // 仅导出可被其他模块引用的符号类型
            if (sym->kind == SymbolKind::Sub ||
                sym->kind == SymbolKind::Function ||
                sym->kind == SymbolKind::Variable ||
                sym->kind == SymbolKind::Constant ||
                sym->kind == SymbolKind::Class) {
                result.push_back(sym.get());
            }
        }
    }
    return result;
}

std::unordered_set<std::string> SymbolTable::getExternalModuleNames() const {
    std::unordered_set<std::string> result;
    if (!moduleScope_) return result;
    for (const auto& [key, sym] : moduleScope_->symbols()) {
        if (sym->isExternal && !sym->sourceModule.empty()) {
            result.insert(sym->sourceModule);
        }
    }
    return result;
}

} // namespace vb6c3
