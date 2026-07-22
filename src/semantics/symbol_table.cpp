#include "semantics/symbol_table.hpp"
#include <algorithm>

namespace vb6c3 {

// ============================================================
// Scope
// ============================================================

bool Scope::define(std::unique_ptr<Symbol> sym) {
    // 使用storageKey作为键: Property Get/Let/Set加后缀区分同名共存
    std::string key = sym->storageKey();
    auto it = symbols_.find(key);
    if (it != symbols_.end()) {
        return false;  // 重定义
    }
    symbols_[key] = std::move(sym);
    return true;
}

Symbol* Scope::lookupLocal(const std::string& name) const {
    std::string lower = Symbol::toLower(name);
    // 先查找非Property符号 (key == lowerName)
    auto it = symbols_.find(lower);
    if (it != symbols_.end()) {
        return it->second.get();
    }
    // 再查找Property Get (key == lowerName + "$pg") - 默认返回Property Get
    it = symbols_.find(lower + "$pg");
    if (it != symbols_.end()) {
        return it->second.get();
    }
    // 再查找Property Let
    it = symbols_.find(lower + "$pl");
    if (it != symbols_.end()) {
        return it->second.get();
    }
    // 再查找Property Set
    it = symbols_.find(lower + "$ps");
    if (it != symbols_.end()) {
        return it->second.get();
    }
    return nullptr;
}

Symbol* Scope::lookupLocalByKind(const std::string& name, SymbolKind kind) const {
    std::string lower = Symbol::toLower(name);
    if (isPropertyKind(kind)) {
        // Property: 使用带后缀的键精确查找
        std::string key = lower;
        switch (kind) {
            case SymbolKind::PropertyGet:  key += "$pg"; break;
            case SymbolKind::PropertyLet:  key += "$pl"; break;
            case SymbolKind::PropertySet:  key += "$ps"; break;
            default: break;
        }
        auto it = symbols_.find(key);
        if (it != symbols_.end()) {
            return it->second.get();
        }
        return nullptr;
    }
    // 非Property: 直接用lowerName
    auto it = symbols_.find(lower);
    if (it != symbols_.end() && it->second->kind == kind) {
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

    // P6.3 + P24-05: COM内置类型与用户变量可同名 (不同namespace)
    // 如果新符号是builtin的ComClass/ComInterface/ComModule/ComGlobalNs, 且已有同名符号, 跳过不报错
    // 如果已有符号是ComClass/ComInterface/ComModule, 且新符号是用户变量, 也允许覆盖
    // Fix 018: builtin EnumMember 同样跳过 (COM 类型库枚举成员跨 enum 同名时 first-wins, 如 MSHTML)
    if (sym->isBuiltin && (sym->kind == SymbolKind::ComClass || sym->kind == SymbolKind::ComInterface
                         || sym->kind == SymbolKind::ComModule || sym->kind == SymbolKind::ComGlobalNs
                         || sym->kind == SymbolKind::EnumMember)) {
        auto* existing = current_->lookupLocal(lowerName);
        if (existing) {
            // 已有同名符号, 跳过COM类型注册 (避免重复注册)
            return true;  // 不报错
        }
    }
    // 如果已有同名ComClass/ComInterface/ComModule, 新符号是用户变量, 先移除旧的
    {
        auto* existing = current_->lookupLocal(lowerName);
        if (existing && (existing->kind == SymbolKind::ComClass || existing->kind == SymbolKind::ComInterface
                      || existing->kind == SymbolKind::ComModule || existing->kind == SymbolKind::ComGlobalNs)
            && sym->kind != SymbolKind::ComClass && sym->kind != SymbolKind::ComInterface
            && sym->kind != SymbolKind::ComModule && sym->kind != SymbolKind::ComGlobalNs) {
            // 移除COM类型符号, 允许用户变量覆盖
            current_->symbols_.erase(lowerName);
        }
    }

    // 允许用户定义的 Sub/Function/Variable/Constant 覆盖内置符号
    // VB6合法: 类方法/变量名可与内置函数同名 (如 Timer, FormatDateTime, LTrim, App)
    // Fix 018: 用户 EnumMember 也可覆盖内置 COM 枚举成员 (如 cDatabase.cls 自定义 adEmpty
    // 覆盖 ADO DataTypeEnum 的 adEmpty — 用户项目声明优先于引用的类型库)
    {
        auto* existing = current_->lookupLocal(lowerName);
        if (existing && existing->isBuiltin
            && !sym->isBuiltin
            && (sym->kind == SymbolKind::Sub
                || sym->kind == SymbolKind::Function
                || sym->kind == SymbolKind::Variable
                || sym->kind == SymbolKind::Constant
                || sym->kind == SymbolKind::EnumMember)) {
            // 移除内置符号, 允许用户符号替换
            current_->symbols_.erase(existing->storageKey());
        }
    }

    // Fix 047: Allow local declarations to overwrite pre-registered external symbols.
    // Cross-module Enum/UDT types are pre-registered before Pass 1 so that parameter
    // types resolve correctly. When the local module's own Pass 1 registers the same
    // Enum/UDT, the external symbol must be replaced by the local definition.
    {
        auto* existing = current_->lookupLocal(lowerName);
        if (existing && existing->isExternal && !sym->isExternal) {
            current_->symbols_.erase(existing->storageKey());
        }
    }

    // Property Get/Let/Set允许同名共存 (VB6合法: Property Get Name + Property Let Name)
    // Scope::define已用storageKey区分, 这里只需确认不报错即可

    if (!current_->define(std::move(sym))) {
        diag_.error(DiagnosticID::SemDuplicateDeclaration, loc,
            "重复声明: \x27" + lowerName + "\x27");
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

Symbol* SymbolTable::lookupLocalByKind(const std::string& name, SymbolKind kind) const {
    if (!current_) return nullptr;
    return current_->lookupLocalByKind(name, kind);
}

Symbol* SymbolTable::lookupModule(const std::string& name) const {
    if (!moduleScope_) return nullptr;
    return moduleScope_->lookupLocal(name);
}

Symbol* SymbolTable::lookupModuleByKind(const std::string& name, SymbolKind kind) const {
    if (!moduleScope_) return nullptr;
    return moduleScope_->lookupLocalByKind(name, kind);
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
    std::string key = sym->storageKey();
    auto it = moduleScope_->symbols_.find(key);
    if (it != moduleScope_->symbols_.end()) {
        // 本地已有定义，不注入外部符号
        return;
    }
    moduleScope_->symbols_[key] = std::move(sym);
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
                sym->kind == SymbolKind::Class ||
                sym->kind == SymbolKind::PropertyGet ||
                sym->kind == SymbolKind::PropertyLet ||
                sym->kind == SymbolKind::PropertySet ||
                sym->kind == SymbolKind::EnumType ||
                sym->kind == SymbolKind::EnumMember ||
                sym->kind == SymbolKind::UserDefinedType) {
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