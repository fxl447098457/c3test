#include "backend/cgen.hpp"
#include <algorithm>
#include <cctype>

namespace vb6c3 {

// --- cgen_delegate.cpp: Delegate (tB 扩展) 后端 ---
// 值表示: 委托值 = 生成调用桩 (thunk) 的地址, 与 LongPtr 位兼容.
//   - typedef: 委托声明的调用约定 + 参数 C 映射 (cast 调用点与 API 传参共用)
//   - thunk:   对外签名 = 委托约定; 参数逐字复制目标过程实际 C 签名, 体内纯
//              转发转调 cdecl 目标 — 约定差异被桩吸收, 零 ABI/参数漂移.
// x64 下 MSVC 接受并忽略 __stdcall, 两种架构用同一份生成代码.

static std::string delLower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    return r;
}

std::string CCodeGen::delegateTyName(const std::string& delName) {
    return "vb6_del_t_" + delLower(delName);
}

std::string CCodeGen::delegateThunkName(const std::string& delName,
                                        const std::string& procName) {
    return "vb6_delthunk_" + delLower(delName) + "_" + delLower(procName);
}

// 在本模块声明里按 VB 名找 Sub/FunctionDecl (大小写不敏感).
static Decl* findProcDecl(Module& module, const std::string& name) {
    for (auto& d : module.declarations) {
        if (d->kind == ASTNodeKind::SubDecl &&
            delLower(static_cast<SubDecl&>(*d).name) == delLower(name)) return d.get();
        if (d->kind == ASTNodeKind::FunctionDecl &&
            delLower(static_cast<FunctionDecl&>(*d).name) == delLower(name)) return d.get();
    }
    return nullptr;
}

void CCodeGen::emitDelegateDecls(Module& module) {
    for (auto& d : module.declarations) {
        if (d->kind != ASTNodeKind::DelegateDecl) continue;
        auto& del = static_cast<DelegateDecl&>(*d);
        auto* delSym = symTab_.lookupModuleByKind(del.name, SymbolKind::Delegate);

        std::string conv = del.callingConv == CallConv::CDecl ? "__cdecl" : "__stdcall";
        std::string retC = del.procKind == ProcKind::Function
                           ? mapTypeRef(del.returnType.get()) : "void";
        c_.emitLine("typedef " + retC + " (" + conv + " *" + delegateTyName(del.name) +
                    ")(" + makeParamList(del.params, true) + ");");

        if (!delSym) continue;
        for (auto& t : delSym->delegateTargets) {
            Decl* proc = findProcDecl(module, t.procName);
            if (!proc) continue;  // 语义层已校验目标存在, 防御性跳过
            std::string sig = (proc->kind == ASTNodeKind::FunctionDecl)
                ? makeProcSignature(static_cast<FunctionDecl&>(*proc))
                : makeProcSignature(static_cast<SubDecl&>(*proc));
            // sig = "RET name(params)". 提取三段后用委托约定重组为桩签名.
            size_t lp = sig.find('(');
            size_t sp = sig.rfind(' ', lp);
            std::string ret = sig.substr(0, sp);
            std::string targetName = sig.substr(sp + 1, lp - sp - 1);
            std::string params = sig.substr(lp);  // "(...)"
            std::string thunkSig =
                ret + " " + conv + " " + delegateThunkName(del.name, t.procName) + params;
            c_.emitLine("static " + thunkSig + ";");

            // 转发实参名 = 桩形参声明的尾标识符 (与目标逐字一致).
            std::string inner = params.substr(1, params.size() - 2);
            std::string args;
            if (inner != "void" && !inner.empty()) {
                size_t pos = 0;
                bool first = true;
                int depth = 0;  // 不切函数指针参数里的逗号 (防御)
                for (size_t i = 0; i <= inner.size(); i++) {
                    char ch = (i < inner.size()) ? inner[i] : ',';
                    if (ch == '(') depth++;
                    else if (ch == ')') depth--;
                    if (ch != ',' || depth != 0) continue;
                    std::string p = inner.substr(pos, i - pos);
                    pos = i + 1;
                    size_t nameEnd = p.find_last_not_of(" \t");
                    if (nameEnd == std::string::npos) continue;
                    size_t nameStart = p.rfind(' ', nameEnd);
                    std::string nm = (nameStart == std::string::npos)
                        ? p.substr(0, nameEnd + 1)
                        : p.substr(nameStart + 1, nameEnd - nameStart);
                    if (!first) args += ", ";
                    first = false;
                    args += nm;
                }
            }
            std::string body;
            if (proc->kind == ASTNodeKind::FunctionDecl) {
                body = thunkSig + " { return " + targetName + "(" + args + "); }";
            } else {
                body = thunkSig + " { " + targetName + "(" + args + "); }";
            }
            delegateThunkDefs_.push_back(std::move(body));
        }
        c_.emitLine("");
    }
}

void CCodeGen::emitDelegateThunks() {
    if (delegateThunkDefs_.empty()) return;
    c_.emitLine("// === Delegate call thunks (generated) ===");
    for (auto& def : delegateThunkDefs_) c_.emitLine(def);
    c_.emitLine("");
    delegateThunkDefs_.clear();
}

// 委托变量直调 op(a, b): ((vb6_del_t_<del>)(op))(a, b)
void CCodeGen::emitDelegateCall(IndexOrCallExpr& node) {
    auto* ident = dynamic_cast<IdentifierExpr*>(node.callee.get());
    if (!ident) { lastExpr_ = "0"; return; }  // 语义层只标记 Identifier callee
    Symbol* del = symTab_.lookupModuleByKind(node.delegateTypeName, SymbolKind::Delegate);
    std::string cast = "((" + delegateTyName(node.delegateTypeName) + ")(" +
                       cIdent(ident->name) + "))";
    std::string args;
    for (size_t i = 0; i < node.positional.size(); i++) {
        emitExpr(*node.positional[i]);
        std::string e = lastExpr_;
        // ByRef 形参 + 变量实参 → 取地址 (与常规调用同形).
        if (del && i < del->params.size() && !del->params[i].isByVal &&
            node.positional[i]->kind == ASTNodeKind::IdentifierExpr) {
            e = "&" + cIdent(static_cast<IdentifierExpr&>(*node.positional[i]).name);
        }
        if (i) args += ", ";
        args += e;
    }
    lastExpr_ = cast + "(" + args + ")";
}

} // namespace vb6c3
