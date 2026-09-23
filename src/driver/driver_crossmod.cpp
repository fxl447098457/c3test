// driver_crossmod.cpp - C3 编译器驱动: 跨模块符号链接
// 2026-09-17 从 src/driver/driver.cpp 纯搬移（逐行未改）：
//   原第 1192~1195 行
//   原第 1250~1397 行

#include "driver/driver.hpp"
#include "common/diagnostics.hpp"
#include "ast/ast.hpp"
#include "semantics/semantic_analyzer.hpp"
#include <iostream>
#include <unordered_map>

namespace vb6c3 {

// === 跨模块符号链接 ===
// 遍历每个模块的符号表，查找未定义的标识符，在其他模块的Public符号中查找匹配
// 为匹配到的符号注入 isExternal=true + sourceModule 的外部符号


bool Driver::runCrossModuleResolution() {
    if (modules_.size() != analyzers_.size()) return false;

    // Fix 013: 为每个模块计算模块名 — 使用 module.moduleName (VB_Name)
    // 而非文件名stem, 确保与 clsSym->name / mapTypeRef 生成的类型名一致
    std::vector<std::string> moduleBaseNames;
    for (const auto& module : modules_) {
        moduleBaseNames.push_back(module->moduleName);
    }

    // 收集每个模块导出的Public符号: [模块索引] -> vector<Symbol*>
    std::vector<std::vector<const Symbol*>> exportedSymbols(modules_.size());
    for (size_t i = 0; i < analyzers_.size(); i++) {
        exportedSymbols[i] = analyzers_[i]->symbolTable().getPublicSymbols();
    }

    // 构建 "存储键 -> (模块索引, Symbol*)" 的全局查找表
    // Fix 010r-12: 使用storageKey()而非lowerName做去重键
    // 原因: Property Get/Let/Set同名但有不同storageKey ($pg/$pl/$ps)
    // 用lowerName去重会导致只有第一个变体(通常Get)被保留, Let/Set丢失
    // 消费模块的P6.7查找 lookupModuleByKind(name, PropertyLet) 会失败
    std::unordered_map<std::string, std::pair<size_t, const Symbol*>> globalPublicSyms;
    for (size_t i = 0; i < exportedSymbols.size(); i++) {
        for (const Symbol* sym : exportedSymbols[i]) {
            std::string sKey = sym->storageKey();
            // 同一个storageKey只保留第一个 (VB6行为: 先声明的优先)
            auto itExisting = globalPublicSyms.find(sKey);
            if (itExisting == globalPublicSyms.end()) {
                globalPublicSyms[sKey] = {i, sym};
            } else {
                // Fix 095: 标准模块过程优先于类成员 — 撞名时保留 .bas 版.
                // VB6 语义: 类 Public 成员不可裸调 (VB_PredeclaredId=False 时),
                // 裸名引用 (GenerateWebSocketKey()) 只能解析到标准模块的 Public
                // 过程. 先到先得规则在 mWebSocketUtils.bas 与 cWebSocketUtils.cls
                // 同名双定义 (GenerateWebSocketKey/StringToUTF8/UTF8ToString/
                // ComputeAcceptKey/GetHeaderValue/GetCloseCodeDescription) 时
                // 可能保留类版符号 → 裸调生成 vb6_<Class>_<Proc>() 缺 me 首参
                // → C2198 (cWebSocketClient.c 81 等 13 处).
                // 类版成员仍经 memberParams/memberProcKinds 表走显式限定调用
                // (obj.Method), 此处不丢失其任何信息.
                const Symbol* existing = itExisting->second.second;
                bool existingIsSetter = (existing->kind == SymbolKind::PropertyLet
                                         || existing->kind == SymbolKind::PropertySet);
                if (existingIsSetter && sym->kind == SymbolKind::PropertyGet) {
                    globalPublicSyms[sKey] = {i, sym};
                }
                // Fix 095: 过程符号撞名 — 类版已在, 标准 .bas 版到来时切换
                else if ((sym->kind == SymbolKind::Sub || sym->kind == SymbolKind::Function)
                         && (existing->kind == SymbolKind::Sub || existing->kind == SymbolKind::Function)) {
                    size_t curIdx = itExisting->second.first;
                    bool curIsClass = modules_[curIdx]->isClassModule;
                    bool newIsClass = modules_[i]->isClassModule;
                    if (curIsClass && !newIsClass) {
                        globalPublicSyms[sKey] = {i, sym};
                    }
                }
            }
        }
    }

    // 对每个模块，检查其模块级作用域中的所有符号
    // 找到未定义引用（在visit(IdentifierExpr)中可能失败的标识符）
    // 策略：遍历模块级作用域中尚未定义（但被引用的地方找不到）的标识符
    // 实际上更简单的做法：扫描每个模块的AST，找到所有IdentifierExpr引用的名称，
    // 如果在本地符号表中找不到，就在全局Public表中查找并注入外部符号

    // 但为了避免修改AST遍历，采用更简洁的方式：
    // 对每个模块，遍历全局Public表，如果该符号在本模块没有本地定义，且名称匹配
    // 某些被引用但未在本模块定义的标识符，就注入外部符号
    //
    // 更精确的方案：只遍历在visit(IdentifierExpr)中可能需要跨模块的符号类型
    // (Sub/Function/Variable/Constant)

    for (size_t i = 0; i < analyzers_.size(); i++) {
        SymbolTable& symTab = analyzers_[i]->symbolTable();

        for (const auto& [sKey, entry] : globalPublicSyms) {
            auto [srcIdx, srcSym] = entry;
            // 跳过本模块导出的符号
            if (srcIdx == i) continue;

            // 检查本模块是否已有此符号的本地定义
            // Fix 010r-12: Property变体需按kind分别检查 (Get/Let/Set各自独立)
            Symbol* localSym = nullptr;
            if (srcSym->kind == SymbolKind::PropertyGet ||
                srcSym->kind == SymbolKind::PropertyLet ||
                srcSym->kind == SymbolKind::PropertySet) {
                localSym = symTab.lookupModuleByKind(srcSym->name, srcSym->kind);
            } else {
                localSym = symTab.lookupModule(srcSym->name);
            }
            // 重载变体 (O3): 裸键被同源外部 head 占用时**不**拦变体 —
            // 消费模块需要重建 head+variants 组结构. 拦两种情况:
            // 本地(非外部)定义占裸键 → 整组让位; 外部 head 来自别的源模块
            // (先到先得已定主) 或同签名 → 不注入.
            bool ovlVariantAllowed = false;
            if (srcSym->isOverloadVariant && localSym && localSym->isExternal
                && localSym->sourceModule == moduleBaseNames[srcIdx]
                && !localSym->overloadFp.empty()
                && localSym->overloadFp != srcSym->overloadFp) {
                ovlVariantAllowed = true;
            }
            if (localSym && !ovlVariantAllowed) {
                // Fix 177b: 引用类型库的同名 coclass 被本工程同名类模块遮蔽时,
                // **不替换符号**, 只在 builtin ComClass 上记下工程实现类名.
                // VB6 语义: 工程内定义优先于引用库 (VBMAN.vbp 定义 Class=Dictionary,
                // 同时引用 Microsoft Scripting Runtime 的 Scripting.Dictionary).
                // 为什么不整体替换 (Fix 177 首版): 代码生成期整条 coclass 晚绑定通路
                // (VARIANT 表示 / comDefaultMemberName / ComCall 参数打包 / 返回值
                // 解包) 都建立在类型库符号上, 换成工程类走早绑定通路会大面积改写
                // 生成代码, 87 处 cl 编译错误 (C2063/C2440/C2065...).
                // 现方案: `As Dictionary` 保持 coclass 类型 (晚绑定), 仅创建点
                // (New / Dim As New) 由 CCodeGen::comNewExprFor 改走工程类工厂
                // vb6_ComPack_<类>(vb6_cls_<类>_New()) → 真 IDispatch 包装 (ExeComBridge
                // 03 同通路), 运行期对象即工程类实例, 自有扩展成员 (Count/Exists)
                // 经包装器 GetIDsOfNames 正常分发, DISP_E_MEMBERNOTFOUND 不再出现.
                if (srcSym->kind == SymbolKind::Class
                    && localSym->isBuiltin
                    && (localSym->kind == SymbolKind::ComClass
                        || localSym->kind == SymbolKind::ComInterface)) {
                    localSym->comProjectImplClass = srcSym->name;
                }
                // 泛型 (G2) 暴露的存量洞回填: Fix 047 预注册只建"名字级" UDT
                // 占位符号 (无成员表), 消费模块字段类型推断落空 → String 字段
                // 按 Variant 发 (double)BSTR (普通跨模块 UDT 也复现, C2440).
                // 源模块此时已分析完, 把完整成员表回填进占位符号.
                if (srcSym->kind == SymbolKind::UserDefinedType &&
                    localSym->kind == SymbolKind::UserDefinedType &&
                    localSym->udtMembers.empty() && !srcSym->udtMembers.empty()) {
                    localSym->udtMembers = srcSym->udtMembers;
                }
                continue;  // 已有本地定义，不需要外部符号
            }

            // 注入外部符号
            auto extSym = std::make_unique<Symbol>(
                srcSym->kind, srcSym->name, srcSym->type,
                srcSym->location, srcSym->access
            );
            extSym->isExternal = true;
            extSym->sourceModule = moduleBaseNames[srcIdx];
            extSym->params = srcSym->params;  // 复制参数列表（函数调用需要）
            extSym->isArray = srcSym->isArray;
            // 泛型 (G2) 暴露的存量洞: 跨模块 UDT 未复制成员表 → 消费模块成员
            // 类型全退化 Variant (普通 UDT 复现: g.V As String 被 Debug.Print
            // 按 Variant 发 (double)BSTR 强转 → C2440).
            if (srcSym->kind == SymbolKind::UserDefinedType) {
                extSym->udtMembers = srcSym->udtMembers;
            }
            // O3: 携带重载组身份 — storageKey() 据此把变体注入 "<name>$ov$<fp>"
            // 独立键, 裸键留给 head; 消费模块的 resolveOverload 走 lookupModuleOverloads
            // 收集整组后按实参打分选变体.
            extSym->overloadFp = srcSym->overloadFp;
            extSym->isOverloadVariant = srcSym->isOverloadVariant;
            extSym->ovlCount = srcSym->ovlCount;
            // 类符号: 复制instancing、memberNames、isInterface、implementsNames
            if (srcSym->kind == SymbolKind::Class) {
                extSym->instancing = srcSym->instancing;
                extSym->memberNames = srcSym->memberNames;
                extSym->memberReturnTypes = srcSym->memberReturnTypes;  // Fix 015: 链式调用返回类型表
                extSym->memberProcKinds = srcSym->memberProcKinds;       // Fix 016: 成员过程类型表
                extSym->memberParams = srcSym->memberParams;             // Fix 033: 成员参数表 (calleeParams 跨模块精确查找)
                extSym->memberFieldTypes = srcSym->memberFieldTypes;     // Fix 092m: 字段类型表 (With 字段写 COM 解包)
                extSym->memberFieldNames = srcSym->memberFieldNames;     // Fix 092p: 字段声明原名表 (访问点大小写规范化)
                extSym->publicFieldNames = srcSym->publicFieldNames;     // Fix 099: 显式 Public 字段清单 (COM 暴露)
                extSym->memberFieldDispids = srcSym->memberFieldDispids; // Fix 099: Public 字段 TypeLib DISPID
                extSym->memberLetParams = srcSym->memberLetParams;       // Fix 091a: Let 写方向参数表
                extSym->memberSetParams = srcSym->memberSetParams;       // Fix 091a: Set 写方向参数表
                extSym->memberAccessLevels = srcSym->memberAccessLevels; // tB B08a: 成员访问级别
                extSym->isInterface = srcSym->isInterface;  // P6.4
                extSym->implementsNames = srcSym->implementsNames;  // P6.4
                extSym->interfaceMethodNames = srcSym->interfaceMethodNames;  // P6.4
                extSym->eventNames = srcSym->eventNames;  // P6.5
                extSym->comClsidStr = srcSym->comClsidStr;  // P6.8: CLSID
            }
            // Fix 010r-11 / Fix 015: 复制变量/返回类型名
            // - Variable: 用于跨模块类实例变量识别 (knownClassVars_)
            // - Function / PropertyGet: 用于 method chaining 的返回类型推断
            //   (getClassMethodReturnType 读取该字段判断链式调用能继续到哪一层)
            // 对其它 kind 该字段为空, 无副作用. 原先把此赋值放在 Variable-only 分支内,
            // 导致 Function/PropertyGet 外部符号丢失返回类型名, 链式调用解析失败.
            extSym->variableTypeName = srcSym->variableTypeName;
            // Fix 183: 复制 As New 标志 — 外部模块级 As New 变量在消费模块中
            // 同样需要惰性实例化守卫 (否则全局对象恒为 NULL).
            extSym->isNewVar = srcSym->isNewVar;
            // Fix 017: 复制常量值 (EnumMember / Constant 跨模块注入后需保留值).
            // 外部 EnumMember 若 hasConstValue=false, cgen 会发出裸标识符 (如
            // HASH_ALG_SHA256) 而非数值 → C2065. 此前外部符号构造只复制
            // kind/name/type/location/access, 丢失 hasConstValue/constIntValue.
            // Fix 081d: 也复制 constStringValue/constFloatValue, 否则字符串常量跨模块时丢失.
            extSym->hasConstValue = srcSym->hasConstValue;
            extSym->constIntValue = srcSym->constIntValue;
            extSym->constType = srcSym->constType;
            extSym->constStringValue = srcSym->constStringValue;
            extSym->constFloatValue = srcSym->constFloatValue;
            extSym->constBoolValue = srcSym->constBoolValue;
            if (srcSym->kind == SymbolKind::Variable) {
                extSym->dimCount = srcSym->dimCount;
            }

            symTab.defineExternal(std::move(extSym));
        }
    }

    // O3: 外部重载组已注入各模块表 — 补跑各模块分析期"当时查无此名"的调用点,
    // 让语义层把变体后缀写回 AST (cgen 只消费 calleeOvlSuffix, 不做选择).
    for (auto& analyzer : analyzers_) {
        analyzer->resolveDeferredCrossModuleOverloads();
    }

    if (diag_->hasErrors()) return false;
    return true;
}

} // namespace vb6c3
