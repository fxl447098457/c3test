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
            if (localSym) {
                // Fix 177: 引用类型库的同名 coclass/interface 不算"本地定义"。
                // VB6 语义: 工程内定义优先于引用库 —— 同名时工程类遮蔽类型库类型。
                // 场景: VBMAN.vbp 定义 Class=Dictionary, 同时引用 Microsoft Scripting
                // Runtime (含 Scripting.Dictionary)。类型库符号由 driver_semantics 在
                // 各模块 analyze 之前注入为 builtin ComClass, 故此处 lookupModule 命中
                // 它; 原先直接 continue, 导致工程类 Dictionary 永远进不了模块作用域 →
                // "As Dictionary" 被判为 COM 对象 (vb6_NewObject(L"Scripting.Dictionary")
                // + 晚绑定调用) → 调用自有扩展 API (Count() / Exists(Key, CompareCase))
                // 时返回 DISP_E_MEMBERNOTFOUND (0x80020003), 其低 16 位 = 3 → 被
                // vb6_ComCheckError 误报为 VB6 错误 3 "没有GoSub时的Return"
                // (VBMAN demo 弹框、800 端口不起)。
                const bool shadowableComType =
                    (srcSym->kind == SymbolKind::Class)
                    && localSym->isBuiltin
                    && (localSym->kind == SymbolKind::ComClass
                        || localSym->kind == SymbolKind::ComInterface);
                if (!shadowableComType) continue;  // 已有本地定义，不需要外部符号
                // 移除被遮蔽的类型库符号, 让工程内类符号占位
                symTab.eraseModuleSymbol(localSym->name);
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

    if (diag_->hasErrors()) return false;
    return true;
}

} // namespace vb6c3
