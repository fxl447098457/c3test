// vb6c3 - 泛型单态化前端 (tB 扩展, G2/G3)
//
// 职责 (计划冻结版): 语义分析之前, 把 parse 期登记的泛型使用点 (扁名) 物化为
// 宿主模块声明表里的普通声明 —— 模板不进主管线, 特化 = clone+subst+扁名注入.
// 下游 (语义/跨模块/cgen) 只见普通声明, 对"泛型"这一概念零感知.
//
// G2 覆盖: 泛型 UDT (Type X(Of T)) 显式实例化. G3 加入: 泛型过程
// (Sub/Function/Property) 物化 + 调用点类型推断 fixpoint (runGenericsFixpoint,
// 跨模块链接之后收 analyzer 上报的实例化请求 → 再物化 → 增量分析 → 再链接).
// 守卫: 无模板无使用 → 立即 return true, 存量工程零触碰 (逐字节护栏已证).

#include "driver/driver.hpp"
#include "ast/ast_clone.hpp"
#include "semantics/semantic_analyzer.hpp"
#include <algorithm>
#include <cctype>
#include <deque>
#include <set>

namespace vb6c3 {

namespace {

// 声明 → (typeParams, name) 视图 (仅泛型关心的四种 decl; 其余返回 nullptr)
const std::vector<std::string>* templateParamsOf(Decl& d, std::string* outName) {
    switch (d.kind) {
    case ASTNodeKind::TypeDecl: {
        auto& x = static_cast<TypeDecl&>(d);
        if (outName) *outName = x.name;
        return &x.typeParams;
    }
    case ASTNodeKind::SubDecl: {
        auto& x = static_cast<SubDecl&>(d);
        if (outName) *outName = x.name;
        return &x.typeParams;
    }
    case ASTNodeKind::FunctionDecl: {
        auto& x = static_cast<FunctionDecl&>(d);
        if (outName) *outName = x.name;
        return &x.typeParams;
    }
    case ASTNodeKind::PropertyDecl: {
        auto& x = static_cast<PropertyDecl&>(d);
        if (outName) *outName = x.name;
        return &x.typeParams;
    }
    default: return nullptr;
    }
}

// cIdent 同形映射 (非字母数字→'_'), 与后端 cgen_base_naming 的清洗规则一致 —
// 物化前检查特化扁名不会与任何用户声明名在 C 层撞车.
std::string toCIdentLike(const std::string& s) {
    std::string out;
    for (char c : s) out += (std::isalnum((unsigned char)c) || c == '_') ? c : '_';
    return out;
}

std::string lower(const std::string& s) {
    std::string o = s;
    std::transform(o.begin(), o.end(), o.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    return o;
}

// 扁名形态: 含 "_G<digits>_" 标记 (Parser::makeFlatGenericName 同规则).
// 用户名理论上可写出同形态串 → 物化时与用户声明名集合做显式撞车检测.
bool isFlatName(const std::string& s) {
    for (size_t i = 0; i + 3 <= s.size(); i++) {
        if (s[i] != '_' || s[i + 1] != 'G') continue;
        size_t j = i + 2;
        while (j < s.size() && std::isdigit((unsigned char)s[j])) j++;
        if (j > i + 2 && j < s.size() && s[j] == '_') return true;
    }
    return false;
}

size_t flatDepth(const std::string& s) {
    size_t d = 0;
    for (size_t i = 0; i + 3 <= s.size(); i++) {
        if (s[i] != '_' || s[i + 1] != 'G') continue;
        size_t j = i + 2;
        while (j < s.size() && std::isdigit((unsigned char)s[j])) j++;
        if (j > i + 2 && j < s.size() && s[j] == '_') { d++; i = j; }
    }
    return d;
}

} // namespace

bool Driver::runGenericsPrepass() {
    // ---- 1) 模板登记表 ----
    for (auto& mod : modules_) {
        // 泛型类模板 (G4): 模块级登记 + v1 形态护栏
        if (mod->isClassModule && !mod->classTypeParams.empty()) {
            std::string key = lower(mod->moduleName);
            if (genericTemplates_.count(key) || genericClassTemplates_.count(key)) {
                diag_->error(DiagnosticID::SemDuplicateDeclaration, mod->loc,
                    "泛型模板重名: '" + mod->moduleName + "'");
                continue;
            }
            if (!mod->implements.empty()) {
                diag_->error(DiagnosticID::CodeGenUnsupportedFeature, mod->loc,
                    "泛型类 '" + mod->moduleName + "' 不支持 Implements (v1)");
                continue;
            }
            for (auto& attr : mod->attributes) {
                std::string an = lower(attr->attrName);
                if ((an == "vb_predeclaredid" || an == "vb_exposed") && attr->value &&
                    attr->value->kind == ASTNodeKind::LiteralExpr) {
                    std::string raw = static_cast<LiteralExpr&>(*attr->value).rawText;
                    if (raw == "True" || raw == "true" || raw == "-1") {
                        diag_->error(DiagnosticID::CodeGenUnsupportedFeature, mod->loc,
                            "泛型类 '" + mod->moduleName + "' 不支持 " + attr->attrName +
                            " (v1)");
                    }
                }
            }
            for (auto& decl : mod->declarations) {
                std::string mn;
                bool isProc = decl->kind == ASTNodeKind::SubDecl ||
                              decl->kind == ASTNodeKind::FunctionDecl ||
                              decl->kind == ASTNodeKind::PropertyDecl;
                bool allowed = isProc || decl->kind == ASTNodeKind::VariableDecl ||
                               decl->kind == ASTNodeKind::ConstDecl ||
                               decl->kind == ASTNodeKind::MultiDecl;
                // 只收 过程/字段/常量; Type/Enum/Event/Declare/Delegate 拒绝
                // (类内 Type 是全局 UDT, 特化副本会与他模块重定义撞车)
                if (!allowed) {
                    diag_->error(DiagnosticID::CodeGenUnsupportedFeature, decl->loc,
                        "泛型类 '" + mod->moduleName + "' 含不支持的成员声明 (v1: 仅过程/字段/常量)");
                }
                // 泛型类成员不得自身再是泛型模板 (类级 T 无法进入其特化绑定)
                if (isProc) {
                    const auto* memberTp = templateParamsOf(*decl, &mn);
                    if (memberTp && !memberTp->empty()) {
                        diag_->error(DiagnosticID::CodeGenUnsupportedFeature, decl->loc,
                            "泛型类 '" + mod->moduleName + "' 的成员 '" + mn +
                            "' 不能再是泛型模板 (v1)");
                    }
                }
            }
            genericClassTemplates_[key] = mod.get();
        }
        for (auto& decl : mod->declarations) {
            if (mod->isClassModule && !mod->classTypeParams.empty()) break; // 上面已拒绝
            std::string name;
            const auto* tp = templateParamsOf(*decl, &name);
            if (!tp || tp->empty()) continue;
            std::string key = lower(name);
            if (genericTemplates_.count(key) || genericClassTemplates_.count(key)) {
                diag_->error(DiagnosticID::SemDuplicateDeclaration, decl->loc,
                    "泛型模板重名: '" + name + "'");
                continue;
            }
            // 类型参数互查重 (Of T, T) + 保留原始大小写 (替换按小写匹配)
            std::set<std::string> seen;
            for (auto& p : *tp) {
                if (!seen.insert(lower(p)).second) {
                    diag_->error(DiagnosticID::SemTypeMismatch, decl->loc,
                        "泛型模板 '" + name + "' 类型参数重复: " + p);
                }
            }
            genericTemplates_[key] = GenericTemplateInfo{mod.get(), decl.get(), *tp};
        }
    }

    // analyzer 推断侧的只读视图 (与 genericTemplates_ 同步; UDT 项被 tryBind 跳过)
    genView_.clear();
    for (auto& [k, info] : genericTemplates_) {
        genView_[k] = GenTemplateView{info.decl, info.typeParams};
    }

    if (genericTemplates_.empty() && genericUses_.empty()) return true;  // 零泛型快退

    if (genericUses_.empty() && !genericTemplates_.empty()) return true; // 模板未使用: 不物化 (tB 同口径)

    return materializeGenerics(nullptr);
}

// ============================================================
// 物化 fixpoint (单轮): 消费 genericUses_ 中未物化的扁名
// ============================================================

bool Driver::materializeGenerics(std::vector<std::pair<Module*, Decl*>>* freshOut) {
    // 用户声明名全集 (C 同形冲突检查用; 含此前各轮已注入的特化副本)
    std::set<std::string> userIdents;
    for (auto& mod : modules_) {
        userIdents.insert(toCIdentLike(mod->moduleName));
        for (auto& decl : mod->declarations) {
            std::string n;
            if (templateParamsOf(*decl, &n)) userIdents.insert(toCIdentLike(n));
            else if (decl->kind == ASTNodeKind::EnumDecl)
                userIdents.insert(toCIdentLike(static_cast<EnumDecl&>(*decl).name));
            else if (decl->kind == ASTNodeKind::ConstDecl)
                userIdents.insert(toCIdentLike(static_cast<ConstDecl&>(*decl).name));
        }
    }

    std::deque<std::string> queue;
    for (auto& [k, u] : genericUses_) {
        if (!genericMaterialized_.count(k)) queue.push_back(k);
    }

    while (!queue.empty()) {
        std::string flat = queue.front();
        queue.pop_front();
        if (genericMaterialized_.count(flat)) continue;
        auto useIt = genericUses_.find(flat);
        if (useIt == genericUses_.end()) continue;  // 手工合成外的意外键, 忽略
        const GenericUseRec& use = useIt->second;
        SourceLocation loc{};

        auto tmplIt = genericTemplates_.find(lower(use.base));
        if (tmplIt == genericTemplates_.end()) {
            auto cit = genericClassTemplates_.find(lower(use.base));
            if (cit == genericClassTemplates_.end()) {
                diag_->error(DiagnosticID::SemTypeMismatch, loc,
                    "使用未声明的泛型 '" + use.base + "' (实例化: " + flat + ")");
                continue;
            }
            // ---- 泛型类特化 (G4): 整模块克隆注入 modules_ ----
            // 只允许发生在 prepass (stage 2.6): fixpoint 期 modules_ 已与
            // analyzers_ 1:1 冻结, 再塞模块会破坏平行索引管线. 类实例化请求
            // 不可能来自推断 (genView_ 不含类模板), 走到这里即显式使用点.
            Module* src = cit->second;
            loc = src->loc;
            if (!analyzers_.empty()) {
                diag_->error(DiagnosticID::SemTypeMismatch, loc,
                    "泛型类 '" + use.base + "' 的实例化必须显式写出 (Of ...) (v1)");
                continue;
            }
            if (use.args.size() != src->classTypeParams.size()) {
                diag_->error(DiagnosticID::SemWrongNumberOfArguments, loc,
                    "泛型类 '" + use.base + "' 类型实参个数不符: 模板 " +
                    std::to_string(src->classTypeParams.size()) + " 个, 使用 " +
                    std::to_string(use.args.size()) + " 个");
                continue;
            }
            if (flatDepth(flat) > kGenericMaxDepth ||
                genericMaterialized_.size() + queue.size() > kGenericMaxInstances) {
                diag_->error(DiagnosticID::SemTypeMismatch, loc,
                    "无界泛型实例化 (上限: 深度" + std::to_string(kGenericMaxDepth) +
                    "/总数" + std::to_string(kGenericMaxInstances) + "): " + flat);
                continue;
            }
            std::string cNameC = toCIdentLike(flat);
            if (userIdents.count(cNameC)) {
                diag_->error(DiagnosticID::SemDuplicateDeclaration, loc,
                    "泛型实例化名 '" + flat + "' 与已有标识符 (C 名 " + cNameC + ") 冲突");
                continue;
            }
            bool cDepsReady = true;
            for (auto& a : use.args) {
                if (!isFlatName(a)) continue;
                if (genericMaterialized_.count(lower(a))) continue;
                if (!genericUses_.count(lower(a))) {
                    diag_->error(DiagnosticID::SemTypeMismatch, loc,
                        "泛型实参 '" + a + "' 不是有效的已登记实例化");
                    cDepsReady = false;
                    break;
                }
                queue.push_back(lower(a));
            }
            if (cDepsReady) {
                for (auto& a : use.args) {
                    if (isFlatName(a) && !genericMaterialized_.count(lower(a))) {
                        queue.push_back(flat);
                        cDepsReady = false;
                        break;
                    }
                }
            }
            if (!cDepsReady) continue;
            ASTCloner ccloner;
            std::vector<std::unique_ptr<SimpleTypeRef>> cArgNodes;
            for (size_t i = 0; i < src->classTypeParams.size(); i++) {
                cArgNodes.push_back(std::make_unique<SimpleTypeRef>(loc, use.args[i]));
                ccloner.bind(lower(src->classTypeParams[i]), cArgNodes.back().get());
            }
            // 体内自引用 (As Holder / New Holder / Dim x As New Holder): 类名整体
            // 视为可替换类型 → TypeRef 位经 subst_ 改扁名, 标识符/名字位经 idRename_.
            cArgNodes.push_back(std::make_unique<SimpleTypeRef>(loc, flat));
            ccloner.bind(lower(src->moduleName), cArgNodes.back().get());
            ccloner.bindProcSelf(lower(src->moduleName), flat);
            auto modClone = ccloner.cloneModule(*src, flat);
            if (!modClone) {
                diag_->error(DiagnosticID::CodeGenUnsupportedFeature, loc,
                    "泛型类 '" + use.base + "' 含当前不支持的成员形态");
                continue;
            }
            modules_.push_back(std::move(modClone));
            userIdents.insert(cNameC);
            genericMaterialized_[flat] = true;
            continue;
        }
        GenericTemplateInfo& tmpl = tmplIt->second;
        loc = tmpl.decl->loc;

        if (use.args.size() != tmpl.typeParams.size()) {
            diag_->error(DiagnosticID::SemWrongNumberOfArguments, loc,
                "泛型 '" + use.base + "' 类型实参个数不符: 模板 " +
                std::to_string(tmpl.typeParams.size()) + " 个, 使用 " +
                std::to_string(use.args.size()) + " 个 (需显式写出全部实参)");
            continue;
        }
        if (flatDepth(flat) > kGenericMaxDepth ||
            genericMaterialized_.size() + queue.size() > kGenericMaxInstances) {
            diag_->error(DiagnosticID::SemTypeMismatch, loc,
                "无界泛型实例化 (上限: 深度" + std::to_string(kGenericMaxDepth) +
                "/总数" + std::to_string(kGenericMaxInstances) + "): " + flat);
            continue;
        }
        std::string cName = toCIdentLike(flat);
        if (userIdents.count(cName)) {
            diag_->error(DiagnosticID::SemDuplicateDeclaration, loc,
                "泛型实例化名 '" + flat + "' 与已有标识符 (C 名 " + cName + ") 冲突");
            continue;
        }

        // 依赖序: 嵌套实参 (含 _G 扁名) 先物化; 未就绪则自我回炉 (arg 自身总在
        // genericUses_ 中, tryFlatten 每层都登记 → 无死锁; 不在则报无模板)
        bool depsReady = true;
        for (auto& a : use.args) {
            if (!isFlatName(a)) continue;
            if (genericMaterialized_.count(lower(a))) continue;
            if (!genericUses_.count(lower(a))) {
                diag_->error(DiagnosticID::SemTypeMismatch, loc,
                    "泛型实参 '" + a + "' 不是有效的已登记实例化");
                depsReady = false;
                break;
            }
            queue.push_back(lower(a));
        }
        if (!depsReady) continue;
        for (auto& a : use.args) {
            if (isFlatName(a) && !genericMaterialized_.count(lower(a))) {
                queue.push_back(flat);   // 有实参还没物化, 自我回炉
                depsReady = false;
                break;
            }
        }
        if (!depsReady) continue;

        // ---- 物化: 模板声明 → 深拷贝+类型替换 → 扁名注入宿主模块 ----
        // 类型参数 → 实参 SimpleTypeRef (argNodes 保活到克隆结束)
        ASTCloner cloner;
        std::vector<std::unique_ptr<SimpleTypeRef>> argNodes;
        for (size_t i = 0; i < tmpl.typeParams.size(); i++) {
            argNodes.push_back(std::make_unique<SimpleTypeRef>(loc, use.args[i]));
            cloner.bind(lower(tmpl.typeParams[i]), argNodes.back().get());
        }
        std::unique_ptr<Decl> clone;
        std::string tmplName;
        templateParamsOf(*tmpl.decl, &tmplName);
        switch (tmpl.decl->kind) {
        case ASTNodeKind::TypeDecl:
            clone = cloner.cloneTypeDecl(static_cast<TypeDecl&>(*tmpl.decl), flat);
            break;
        case ASTNodeKind::SubDecl:
            cloner.bindProcSelf(lower(tmplName), flat);  // 体内"给函数名赋值"改名
            clone = cloner.cloneSubDecl(static_cast<SubDecl&>(*tmpl.decl), flat);
            break;
        case ASTNodeKind::FunctionDecl:
            cloner.bindProcSelf(lower(tmplName), flat);
            clone = cloner.cloneFunctionDecl(static_cast<FunctionDecl&>(*tmpl.decl), flat);
            break;
        case ASTNodeKind::PropertyDecl:
            cloner.bindProcSelf(lower(tmplName), flat);
            clone = cloner.clonePropertyDecl(static_cast<PropertyDecl&>(*tmpl.decl), flat);
            break;
        default:
            break;
        }
        if (!clone) {
            diag_->error(DiagnosticID::CodeGenUnsupportedFeature, loc,
                "泛型模板 '" + use.base + "' 含当前不支持的成员形态");
            continue;
        }
        Decl* raw = clone.get();
        // 注入到模块声明表**末尾**: 物化 fixpoint 的依赖回炉机制保证
        // 被引用特化先注入 → 头文件里 typedef struct 发射序 = 依赖序
        // (若插在模板之后, box(pair(...)) 会先于 pair 定义发射 → 不完整类型)
        tmpl.module->declarations.push_back(std::move(clone));
        userIdents.insert(cName);  // 同轮后续物化的撞车检查也覆盖新副本
        genericMaterialized_[flat] = true;
        if (freshOut) freshOut->push_back({tmpl.module, raw});
    }

    if (diag_->hasErrors()) return false;

    // 剩余安全网: genericUses_ 中始终没能物化的键 → 漏物化即报错 (绝不出坏码)
    for (auto& [k, u] : genericUses_) {
        if (!genericMaterialized_.count(k)) {
            diag_->error(DiagnosticID::SemTypeMismatch, SourceLocation{},
                "泛型实例化未完成: " + k);
            return false;
        }
    }
    return true;
}

// ============================================================
// G3 推断 fixpoint: 语义/跨模块链接之后, 消费调用点推断请求
// ============================================================
// 时序: 显式实例化已在 runGenericsPrepass 物化; 裸调 (First SomeArr) 由
// analyzer 在 resolveDeferredCrossModuleOverloads 里绑定类型参数, 改写调用点
// 为扁名并上报 GenInstRequest. 本循环 收请求→物化→增量分析(analyzeExtraDecls)
// → 重跑跨模块链接 (特化符号对外可见 + 新体内延后点消费) 至收敛.
bool Driver::runGenericsFixpoint() {
    if (genericTemplates_.empty()) return true;  // 零泛型快退 (护栏路径)

    // 单模块工程主管线不调 runCrossModuleResolution; 延后点在此自行消费一次
    if (modules_.size() < 2) {
        for (auto& a : analyzers_) a->resolveDeferredCrossModuleOverloads();
    }

    for (int iter = 0; iter < 64; iter++) {
        bool anyReq = false;
        for (auto& a : analyzers_) {
            for (auto& req : a->takeGenericRequests()) {
                if (genericUses_.count(req.flat)) continue;  // 已物化/已在队列
                genericUses_[req.flat] = GenericUseRec{req.base, req.args};
                anyReq = true;
            }
        }
        if (!anyReq) return true;  // 收敛: 上一轮注入的体内再无新实例化需求

        std::vector<std::pair<Module*, Decl*>> fresh;
        if (!materializeGenerics(&fresh)) return false;

        for (size_t i = 0; i < modules_.size() && i < analyzers_.size(); i++) {
            std::vector<Decl*> ptrs;
            for (auto& f : fresh) {
                if (f.first == modules_[i].get()) ptrs.push_back(f.second);
            }
            if (!ptrs.empty()) analyzers_[i]->analyzeExtraDecls(ptrs);
        }
        if (diag_->hasErrors()) return false;

        if (modules_.size() > 1) {
            if (!runCrossModuleResolution()) return false;
        } else {
            for (auto& a : analyzers_) a->resolveDeferredCrossModuleOverloads();
        }
    }
    diag_->error(DiagnosticID::SemTypeMismatch, SourceLocation{},
        "泛型推断 fixpoint 未收敛 (超过 64 轮)");
    return false;
}

} // namespace vb6c3
