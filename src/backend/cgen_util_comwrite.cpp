#include "backend/cgen.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <functional>

namespace vb6c3 {

// --- cgen_util_comwrite.cpp: COM 左值改写 (链式写 / LValue 重写 / Let 参数打包) ---


// ============================================================
// Fix 090ae: 链式 COM 默认属性索引赋值 (P25b helper, 原内联于 AssignmentStmt)
// VB: dic(a)(b) = v 或  Set dic(a)(b) = v — 多层默认属性/Item 索引写.
// emitExpr(target) 只支持链式读 (生成 vb6_VariantFromComResult(vb6_ComCall(...))
// 的 rvalue, 作为 LHS 触发 C2440). 这里逐层把中间层结果解包为对象
// (vb6_ComCallObject = ComCall+UnpackObject), 对最外层用 vb6_ComSetPropArg.
// AssignmentStmt/SetStmt 共用.
// ============================================================
bool CCodeGen::tryEmitChainedComWrite(Expr* targetNode, Expr* valueNode) {
    std::vector<IndexOrCallExpr*> chain;
    Expr* cur = targetNode;
    while (cur && cur->kind == ASTNodeKind::IndexOrCallExpr) {
        chain.push_back(static_cast<IndexOrCallExpr*>(cur));
        cur = static_cast<IndexOrCallExpr*>(cur)->callee.get();
    }
    if (chain.size() < 2 || !cur || cur->kind != ASTNodeKind::IdentifierExpr) {
        return false;
    }
    auto& rootId25 = static_cast<IdentifierExpr&>(*cur);
    std::string rootLower25 = Symbol::toLower(rootId25.name);
    bool rootIsCom25 = knownObjectVars_.count(rootLower25)
                    || knownVariantVars_.count(rootLower25)
                    || knownTypedComVars_.count(rootLower25);
    // Fix 090e: 根也可以是"模块默认成员属性" — PropertyGet 返回 COM 对象
    // (如 cIni.Root 的 VB_UserMemId=0 默认成员, 返回 Dictionary) 时,
    // Root(Section)(Key) = v 与 dic(Section)(Key) = v 同构. emitExpr(Root)
    // 生成 vb6_cIni_prop_get_Root((void*)me) 调用文本作为链起点.
    // 判定: 该返回类型符号带 comDefaultMemberName (Dictionary→Item).
    Symbol* rootRtSym25 = nullptr;  // root 返回的 COM 类型符号 (默认成员名来源)
    if (!rootIsCom25) {
        Symbol* rootSym25 = symTab_.lookupModule(rootId25.name);
        if (rootSym25 && rootSym25->kind == SymbolKind::PropertyGet
            && !rootSym25->variableTypeName.empty()) {
            rootRtSym25 = lookupDotted(rootSym25->variableTypeName);
            if (rootRtSym25 && !rootRtSym25->comDefaultMemberName.empty()) {
                rootIsCom25 = true;
            }
        }
    }
    // Fix 090ae: 根也可以是"当前类的 void* COM 字段" — me->Data 这类 COM 字段
    // (Dim Data As New Dictionary) 的 Data(L)(C) = v 链式写. knownObjectVars_/
    // knownTypedComVars_ 只注册局部/参数, 不含类字段; 类 void* 字段在
    // classVoidFieldMap_ (driver 预扫描, 键=模块名, 集合=字段名小写含 m_ 变体).
    if (!rootIsCom25 && isClassModule_ && classVoidFieldMap_) {
        auto itV90ae = classVoidFieldMap_->find(moduleName_);
        if (itV90ae != classVoidFieldMap_->end()
            && (itV90ae->second.count(rootLower25)
                || itV90ae->second.count("m_" + rootLower25))) {
            rootIsCom25 = true;
        }
    }
    if (!rootIsCom25) return false;

    // 默认成员名: 优先 root 返回类型符号 (Dictionary→Item), 其次类型化 COM
    // 变量注册, 回退 "Item"
    std::string defMem25 = "Item";
    if (rootRtSym25 && !rootRtSym25->comDefaultMemberName.empty()) {
        defMem25 = rootRtSym25->comDefaultMemberRealName;
    } else {
        auto itTyped25 = knownTypedComVars_.find(rootLower25);
        if (itTyped25 != knownTypedComVars_.end()
            && !itTyped25->second->comDefaultMemberName.empty()) {
            defMem25 = itTyped25->second->comDefaultMemberRealName;
        }
    }
    std::string defMemLit25 = "L\"" + defMem25 + "\"";
    // 打包某层索引参数为 (void*[]){pack(a),...} 字符串
    auto packLayer25 = [&](IndexOrCallExpr& ic, std::vector<std::string>& out) -> int32_t {
        for (size_t j = 0; j < ic.positional.size(); j++) {
            std::string packFn25 = comPackExpr(*ic.positional[j]);
            emitExpr(*ic.positional[j]);
            { std::string r25 = resolveComMarkerForPack(packFn25); if (!r25.empty()) lastExpr_ = r25; }
            out.push_back(packFn25 + "(" + lastExpr_ + ")");
        }
        return (int32_t)ic.positional.size();
    };
    emitExpr(*cur);  // 根对象表达式 (COM 变量 / me->Field / PropertyGet 调用)
    std::string accExpr25 = std::move(lastExpr_);
    // chain[0]=最外层索引, chain[last]=最内层索引. 先按最内→外取对象,
    // 即逆序遍历 chain 深度层 (除最外层), 每层取默认成员对象
    for (int32_t i = (int32_t)chain.size() - 1; i >= 1; i--) {
        std::vector<std::string> packedMid;
        int32_t argcMid = packLayer25(*chain[i], packedMid);
        std::string arrMid = "(void*[]){";
        for (size_t k = 0; k < packedMid.size(); k++) {
            if (k > 0) arrMid += ", ";
            arrMid += packedMid[k];
        }
        arrMid += "}";
        accExpr25 = "vb6_ComCallObject(" + accExpr25 + ", " + defMemLit25
                  + ", " + arrMid + ", " + std::to_string(argcMid) + ")";
    }
    // 最外层: 带索引属性 Put
    std::vector<std::string> packedOut25;
    int32_t argcOut25 = packLayer25(*chain[0], packedOut25);
    std::string arrOut25 = "(void*[]){";
    for (size_t k = 0; k < packedOut25.size(); k++) {
        if (k > 0) arrOut25 += ", ";
        arrOut25 += packedOut25[k];
    }
    arrOut25 += "}";
    emitExpr(*valueNode);
    std::string valExpr25 = std::move(lastExpr_);
    std::string packVal25 = comPackExpr(*valueNode);
    c_.emitLine("vb6_ComSetPropArg(" + accExpr25 + ", " + defMemLit25 + ", "
                + arrOut25 + ", " + std::to_string(argcOut25) + ", "
                + packVal25 + "(" + valExpr25 + "));  /* COM chained default-prop assign (P25b) */");
    return true;
}


// ============================================================
// Fix 010r-16: COM/Property-Get 左值重写辅助函数实现
// ============================================================
bool CCodeGen::tryRewriteCOMLvalue(const std::string& target, const std::string& value,
                                   Expr* valueExpr, bool isSet) {
    if (target.empty()) return false;

    // 工具 lambda: 在 target 中查找首个顶层括号 (跳过嵌套括号), 返回 '(' 与 ')'
    // 的位置; 找不到返回 npos.
    auto findCallParens = [](const std::string& s, size_t startAt) -> std::pair<size_t, size_t> {
        size_t openPos = std::string::npos;
        for (size_t i = startAt; i < s.size(); ++i) {
            if (s[i] == '(') { openPos = i; break; }
        }
        if (openPos == std::string::npos) return {std::string::npos, std::string::npos};
        int depth = 0;
        for (size_t i = openPos; i < s.size(); ++i) {
            if (s[i] == '(') depth++;
            else if (s[i] == ')') {
                depth--;
                if (depth == 0) return {openPos, i};
            }
        }
        return {std::string::npos, std::string::npos};
    };
    // 工具 lambda: 按顶层逗号分割调用实参 (跳过括号/方括号/花括号内的逗号), 每段 trim.
    auto splitTopLevelArgs = [](const std::string& s) -> std::vector<std::string> {
        std::vector<std::string> out;
        if (s.empty()) return out;
        int depth = 0;
        size_t start = 0;
        for (size_t i = 0; i < s.size(); ++i) {
            char ch = s[i];
            if (ch == '(' || ch == '[' || ch == '{') { depth++; }
            else if (ch == ')' || ch == ']' || ch == '}') { if (depth > 0) depth--; }
            else if (ch == ',' && depth == 0) {
                out.push_back(s.substr(start, i - start));
                start = i + 1;
            }
        }
        out.push_back(s.substr(start));
        for (auto& seg : out) {
            size_t b = seg.find_first_not_of(" \t");
            if (b == std::string::npos) { seg.clear(); continue; }
            size_t e = seg.find_last_not_of(" \t");
            seg = seg.substr(b, e - b + 1);
        }
        return out;
    };

    // ---- Pattern A: vb6_ComCall(obj, L"Item", args, n) = value ----
    // vb6_ComCall 返回 VARIANT*, 不是左值. 改走 vb6_ComSetPropArg (内部
    // DISPATCH_PROPERTYPUT|PUTREF).
    if (target.find("vb6_ComCall") == 0) {
        auto parens = findCallParens(target, 0);
        if (parens.first != std::string::npos && parens.second != std::string::npos) {
            std::string callArgs = target.substr(parens.first + 1,
                                                 parens.second - parens.first - 1);
            std::string packFn = valueExpr ? comPackExpr(*valueExpr) : "vb6_ComPackVariant";
            std::string tag = isSet ? "Set" : "Let";
            c_.emitLine("vb6_ComSetPropArg(" + callArgs + ", " + packFn + "(" + value +
                        "));  /* COM Item assignment (Pattern A, " + tag + ") */");
            return true;
        }
    }

    // ---- Pattern F: vb6_ComGetStringProp(obj, L"Prop") = value ----
    // With-block ClassInstance member: 当 Property 在跨模块符号表里找不到时,
    // codegen 会走 "vb6_ComGetStringProp(_vb6_with_X, L\"Prop\")" 路径; 但读路径
    // 产生的 wchar_t* 不是左值, 赋值触发 C2106. 改走 vb6_ComSetProp.
    // 另外也匹配 vb6_ComGetIntProp / vb6_ComGetDoubleProp / vb6_ComGetObjectProp.
    const std::vector<std::string> comGetFns = {
        "vb6_ComGetStringProp", "vb6_ComGetIntProp", "vb6_ComGetDoubleProp",
        "vb6_ComGetObjectProp", "vb6_ComGetProp"
    };
    for (const auto& fnName : comGetFns) {
        if (target.find(fnName) == 0) {
            auto parens = findCallParens(target, 0);
            if (parens.first != std::string::npos && parens.second != std::string::npos) {
                std::string callArgs = target.substr(parens.first + 1,
                                                     parens.second - parens.first - 1);
                // isSet=true => 对象引用语义, 用 PackObject (DISPATCH_PROPERTYPUTREF)
                // isSet=false => Let 语义, 用 comPackExpr 推断
                std::string packFn = isSet ? "vb6_ComPackObject"
                                  : (valueExpr ? comPackExpr(*valueExpr) : "vb6_ComPackVariant");
                std::string tag = isSet ? "Set" : "Let";
                c_.emitLine("vb6_ComSetProp(" + callArgs + ", " + packFn + "(" + value +
                            "));  /* COM prop assignment (Pattern F, " + tag + ") */");
                return true;
            }
        }
    }

    // ---- Pattern C/D2: vb6_X_prop_get_Y(args) = value ----
    // Property Get 用作 LHS. 改写为 prop_let_Y(args, value) (Let) 或
    // prop_set_Y(args, value) (Set).
    // Fix 084i: 也匹配 prop_set_/prop_let_ 前缀 — 当属性只有 PropertySet
    // (无 Get) 时 emitExpr 直接生成 vb6_cX_prop_set_Y(obj) 单参数形式,
    // 追加 value 参数改写成完整调用, 避免 prop_set_(obj) = value 左值错误.
    {
        const char* verbs[] = {"prop_get_", "prop_set_", "prop_let_"};
        size_t pgPos = std::string::npos;
        std::string matchedVerb;
        for (const char* v : verbs) {
            size_t p = target.find(v);
            if (p != std::string::npos) { pgPos = p; matchedVerb = v; break; }
        }
        if (pgPos != std::string::npos) {
            auto parens = findCallParens(target, pgPos);
            if (parens.first != std::string::npos && parens.second != std::string::npos) {
                std::string prefix = target.substr(0, pgPos);              // vb6_cX_
                std::string afterPg = target.substr(pgPos + matchedVerb.size(),
                                                    parens.first - (pgPos + matchedVerb.size()));
                std::string argsStr = target.substr(parens.first + 1,
                                                    parens.second - parens.first - 1);
                std::string newVerbs = (matchedVerb != "prop_get_") ? matchedVerb
                                      : (isSet ? "prop_set_" : "prop_let_");
                // Fix 090w: Property Let 末参 As Variant (cJson.Item Dat As Variant
                // ByRef / cCsv.Value ByVal Dat As Variant / cHttpServerCookieAttr.Expires
                // Let(v As Variant) — Get 无参) — 值实参 (vb6_Now() double / 666 int /
                // BSTR) 需打包成 vb6_VARIANT, 否则 Pattern C/D2 裸拼 → C2440
                // "double/int/BSTR → vb6_VARIANT(/ *)". 仅当改写目标是 prop_let_/
                // prop_set_ 且 **Let/Set 方向**末形参解析为 Variant 时打包 —
                // findClassMemberCallParams 按 Get > Function > Sub > Let > Set 优先,
                // 对 Get 有参/无参而 Let 末参才是 value 的属性 (Item(key),
                // Expires) 会取错方向, 故直接查 PropertyLet/PropertySet 符号.
                std::string valArg = value;
                if (!isSet && newVerbs.find("prop_get_") == std::string::npos
                    && prefix.rfind("vb6_", 0) == 0 && !afterPg.empty()
                    && afterPg.find('(') == std::string::npos) {
                    std::string cls90w = prefix.substr(4);  // 去 "vb6_" → "cCsv_"
                    if (!cls90w.empty() && cls90w.back() == '_') cls90w.pop_back();
                    // Fix 091a: 写方向参数查找 (Phase A 模块作用域 Let 符号;
                    // Phase B Class 符号 memberLetParams — 覆盖 item$pl 等跨模块
                    // storageKey 冲突场景, 此时读方向 memberParams 只有 Get 的
                    // [key] 会取错方向).
                    const ParameterInfo* lastP90w = nullptr;
                    std::vector<ParameterInfo> writeP90w;
                    if (findClassMemberWriteParams(cls90w, afterPg, isSet, writeP90w)
                        && !writeP90w.empty()) {
                        lastP90w = &writeP90w.back();
                    } else {
                        // fallback: 写方向也查不到时沿用旧 findClassMemberCallParams
                        // (Get 优先 — 可能取到 Get 方向参数, 此时保守不打包)
                        std::vector<ParameterInfo> letParams90w;
                        bool letBuiltin90w = false;
                        if (findClassMemberCallParams(cls90w, afterPg, letParams90w,
                                                      letBuiltin90w)
                            && !letParams90w.empty()) {
                            lastP90w = &letParams90w.back();
                        }
                    }
                    if (lastP90w && lastP90w->type == Vb6Type::Variant) {
                        valArg = packLetValueArg(*lastP90w, valueExpr, value);
                    }
                }
                // Fix 090ad: 只写属性 (无 Get, 如 Dictionary.key(OldKey)=NewKey) 的 LHS
                // target — emitExpr 按 prop_let_/prop_set_ 完整调用生成时 (读上下文
                // fallback 到 Let/Set), 对缺失的 value 形参也 pad 了默认值
                // (vb6_BSTR_Empty()), 例如 prop_let_key(me->m_Dict, OldKey,
                // vb6_BSTR_Empty()). 若括号实参顶层段数 == 业务形参数+1 (对象+全部
                // 形参含 value), 末段即被 pad 的 value 位 → 丢弃, 由真实 RHS value
                // 拼接补齐, 否则 C2197 参数太多 (Dictionary.c 110/162).
                std::string finalArgs = argsStr;
                if (matchedVerb != "prop_get_" && !argsStr.empty()
                    && prefix.rfind("vb6_", 0) == 0 && !afterPg.empty()
                    && afterPg.find('(') == std::string::npos) {
                    std::string clsCd2 = prefix.substr(4);  // 去 "vb6_" → "cXx_"
                    if (!clsCd2.empty() && clsCd2.back() == '_') clsCd2.pop_back();
                    if (!clsCd2.empty()) {
                        std::vector<ParameterInfo> paramsCd2;
                        bool builtinCd2 = false;
                        if (findClassMemberCallParams(clsCd2, afterPg, paramsCd2, builtinCd2)
                            && !paramsCd2.empty()) {
                            std::vector<std::string> topArgs = splitTopLevelArgs(argsStr);
                            if (topArgs.size() == paramsCd2.size() + 1 && topArgs.size() > 1) {
                                topArgs.pop_back();  // 移除被 pad 的 value 默认值
                                finalArgs.clear();
                                for (size_t i = 0; i < topArgs.size(); i++) {
                                    if (i) finalArgs += ", ";
                                    finalArgs += topArgs[i];
                                }
                            }
                        }
                    }
                }
                std::string newCall;
                if (finalArgs.empty()) {
                    newCall = prefix + newVerbs + afterPg + "(" + valArg + ")";
                } else {
                    newCall = prefix + newVerbs + afterPg + "(" + finalArgs + ", " + valArg + ")";
                }
                std::string tag = isSet ? "Set" : "Let";
                c_.emitLine(newCall + ";  /* Property " + tag + " via prop_get_ rewrite (Pattern C/D2) */");
                return true;
            }
        }
    }

    return false;
}


// ============================================================
// Fix 090w/090x: Property Let/Set 值实参打包 (声明见 cgen.hpp)
// ============================================================
std::string CCodeGen::packLetValueArg(const ParameterInfo& lastP, Expr* valueExpr,
                                      const std::string& val) const {
    if (lastP.type != Vb6Type::Variant) return val;
    if (lastP.isByVal) return "vb6_VariantFromValue(" + val + ")";
    // ByRef Variant 形参需要可寻址的 vb6_VARIANT*.
    // (&(vb6_VARIANT){vb6_VariantFromValue(x)}) 是非法 C (结构体复合字面量
    // 不能用另一个结构值初始化 → C2440 "vb6_VARIANT→vb6_vartype"), 必须按实参
    // VB 类型字段式构造 (与 cgen_expr M22 ByRef Variant 参数分支一致).
    Vb6Type vtT = valueExpr ? inferExprType(*valueExpr) : Vb6Type::Variant;
    switch (vtT) {
        case Vb6Type::String:
            return "(&(vb6_VARIANT){.vt=VT_BSTR, .bstrVal=" + val + "})";
        case Vb6Type::Long:
        case Vb6Type::Integer:
            return "(&(vb6_VARIANT){.vt=VT_I4, .lVal=(int32_t)(" + val + ")})";
        case Vb6Type::Byte:
            return "(&(vb6_VARIANT){.vt=VT_UI1, .bVal=(uint8_t)(" + val + ")})";
        case Vb6Type::Double:
        case Vb6Type::Single:
        case Vb6Type::Date:
        case Vb6Type::Currency:
        case Vb6Type::Decimal:
            return "(&(vb6_VARIANT){.vt=VT_R8, .dblVal=(double)(" + val + ")})";
        case Vb6Type::Boolean:
            return "(&(vb6_VARIANT){.vt=VT_BOOL, .boolVal=(int16_t)(" + val + ")})";
        default:
            // 与 M22 default 分支对齐: 变体表达式提取 BSTR, 其余未知按 BSTR 兜底
            if (cExprIsVariant(val)) {
                return "(&(vb6_VARIANT){.vt=VT_BSTR, .bstrVal=vb6_VariantToString("
                       + val + ")})";
            }
            return "(&(vb6_VARIANT){.vt=VT_BSTR, .bstrVal=" + val + "})";
    }
}
} // namespace vb6c3
