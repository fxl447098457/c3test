// driver_semantics.cpp - C3 编译器驱动: 语义分析阶段
// 2026-09-17 从 src/driver/driver.cpp 纯搬移（逐行未改）：
//   原第 867~1190 行

#include "driver/driver.hpp"
#include "common/diagnostics.hpp"
#include "semantics/semantic_analyzer.hpp"
#include <iostream>
#include <algorithm>
#include <unordered_set>

namespace vb6c3 {

bool Driver::runSemanticAnalysis(const CompileOptions& options) {
    analyzers_.clear();
    for (auto& module : modules_) {
        auto analyzer = std::make_unique<SemanticAnalyzer>(*diag_, options.verbose);

        // P6.3: 如果有TypeLib解析结果, 注入COM类型信息到符号表
        if (typelibParser_) {
            for (auto& tl : typelibParser_->cachedResults()) {
                for (auto& cc : tl->coclasses) {
                    // 注册ComClass符号
                    auto sym = std::make_unique<Symbol>(
                        SymbolKind::ComClass, cc->name, Vb6Type::Object,
                        SourceLocation{}, AccessLevel::Public);
                    sym->isBuiltin = true;
                    sym->comClsidStr = cc->clsidStr;
                    sym->comProgId = cc->progId;
                    sym->comDefaultIfaceName = cc->defaultIfaceName;

                    // 复制默认接口的方法签名到ComClass
                    if (cc->defaultIface) {
                        sym->comIidStr = cc->defaultIface->iidStr;
                        sym->comIsDual = cc->defaultIface->isDual;
                        sym->comVtblBase = cc->defaultIface->isDispatch ? 7 : 3;

                        // P24-10: 传播默认成员名 (DISPID_VALUE=0)
                        sym->comDefaultMemberName = cc->defaultIface->defaultMemberName;
                        sym->comDefaultMemberRealName = cc->defaultIface->defaultMemberRealName;

                        for (auto& member : cc->defaultIface->members) {
                            Symbol::ComMethodSig sig;
                            sig.realName = member.realName;
                            sig.memid = member.memid;
                            sig.vtableIndex = member.vtableIndex;
                            sig.returnType = member.returnType;
                            sig.isPropertyGet = (member.kind == ComMemberKind::PropertyGet);
                            sig.isPropertyPut = (member.kind == ComMemberKind::PropertyPut);
                            sig.isPropertyPutRef = (member.kind == ComMemberKind::PropertyPutRef);
                            for (auto& param : member.params) {
                                ParameterInfo pi;
                                pi.name = param.name;
                                pi.type = param.type;
                                pi.isByVal = (param.direction == ComParamDir::In);
                                pi.isOptional = param.isOptional;
                                sig.params.push_back(std::move(pi));
                            }
                            sym->comMethods[member.name] = std::move(sig);
                        }
                        // 填充memberNames (类成员名列表, 兼容现有逻辑)
                        for (auto& member : cc->defaultIface->members) {
                            sym->memberNames.push_back(member.realName);
                        }
                    }

                    // P13.23: 填充事件源接口信息 (用于外部COM WithEvents)
                    if (!cc->defaultSourceIfaceName.empty()) {
                        sym->comHasSourceIface = true;
                        sym->comSourceIfaceName = cc->defaultSourceIfaceName;
                        if (cc->defaultSourceIface) {
                            sym->comSourceIfaceIid = cc->defaultSourceIface->iidStr;
                            sym->comSourceIfaceIsDispOnly = cc->defaultSourceIface->isDispatch;
                            for (auto& member : cc->defaultSourceIface->members) {
                                sym->eventNames.push_back(member.realName);
                                sym->comEventDispids[Symbol::toLower(member.name)] = member.memid;
                                Symbol::ComMethodSig sig;
                                sig.realName = member.realName;
                                sig.memid = member.memid;
                                sig.vtableIndex = member.vtableIndex;
                                sig.returnType = member.returnType;
                                for (auto& param : member.params) {
                                    ParameterInfo pi;
                                    pi.name = param.name;
                                    pi.type = param.type;
                                    pi.isByVal = (param.direction == ComParamDir::In);
                                    pi.isOptional = param.isOptional;
                                    sig.params.push_back(pi);
                                }
                                sym->comSourceMethods[Symbol::toLower(member.name)] = std::move(sig);
                            }
                        }
                    }
                    analyzer->symbolTable().define(std::move(sym));
                }

                // 也注册ComInterface符号
                for (auto& iface : tl->interfaces) {
                    auto sym = std::make_unique<Symbol>(
                        SymbolKind::ComInterface, iface->name, Vb6Type::Object,
                        SourceLocation{}, AccessLevel::Public);
                    sym->isBuiltin = true;
                    sym->comIidStr = iface->iidStr;
                    sym->comIsDual = iface->isDual;
                    sym->comVtblBase = iface->isDispatch ? 7 : 3;

                    for (auto& member : iface->members) {
                        Symbol::ComMethodSig sig;
                        sig.realName = member.realName;
                        sig.memid = member.memid;
                        sig.vtableIndex = member.vtableIndex;
                        sig.returnType = member.returnType;
                        sig.isPropertyGet = (member.kind == ComMemberKind::PropertyGet);
                        sig.isPropertyPut = (member.kind == ComMemberKind::PropertyPut);
                        sig.isPropertyPutRef = (member.kind == ComMemberKind::PropertyPutRef);
                        for (auto& param : member.params) {
                            ParameterInfo pi;
                            pi.name = param.name;
                            pi.type = param.type;
                            pi.isByVal = (param.direction == ComParamDir::In);
                            pi.isOptional = param.isOptional;
                            sig.params.push_back(std::move(pi));
                        }
                        sym->comMethods[member.name] = std::move(sig);
                    }

                    analyzer->symbolTable().define(std::move(sym));
                }

                // Fix 018: 注册COM枚举成员为EnumMember符号 (TextCompare/adStateClosed 等)
                // COM 类型库的 TKIND_ENUM 解析后, 成员作为全局可见命名常量注入符号表.
                // hasConstValue=true 使 cgen 发出数值 (经 Fix 017-P1/P3 路径), 避免裸名 C2065.
                // Volume guard: 跳过枚举成员过多的类型库 (如 MSHTML 数千成员), 避免命名空间
                // 污染、内存膨胀 (125 模块 × N 成员) 和跨 enum 同名碰撞. 小型库 (Scripting ~30,
                // ADO ~200) 正常注册, 覆盖 TextCompare / adXXX 等已知 C2065.
                size_t totalEnumMembers = 0;
                for (auto& en : tl->enums) totalEnumMembers += en->members.size();
                if (totalEnumMembers <= 1000) {
                    for (auto& en : tl->enums) {
                        for (auto& m : en->members) {
                            auto enumSym = std::make_unique<Symbol>(
                                SymbolKind::EnumMember, m.name, Vb6Type::Long,
                                SourceLocation{}, AccessLevel::Public);
                            enumSym->isBuiltin = true;
                            enumSym->hasConstValue = true;
                            enumSym->constIntValue = m.value;
                            enumSym->constType = Vb6Type::Long;
                            analyzer->symbolTable().define(std::move(enumSym));
                        }
                    }
                }
                // Fix 084m: ADODB 非标准别名。类型库 ExecuteOptionEnum 只有 adAsyncExecute(=16),
                // 但源码 (cDatabase.Exec) 使用 adExecuteAsync; 未注册时生成裸名 → C2065。
                {
                    std::string tlPathLower = tl->tlbPath;
                    std::transform(tlPathLower.begin(), tlPathLower.end(), tlPathLower.begin(), ::tolower);
                    bool isAdodb = tlPathLower.find("msado") != std::string::npos
                        || tl->typeLibProjectName.find("ADODB") != std::string::npos;
                    if (isAdodb) {
                        static const std::vector<std::pair<const char*, long>> adodbAliases = {
                            {"adExecuteAsync", 16},
                        };
                        for (auto& al : adodbAliases) {
                            std::string alLower = al.first;
                            std::transform(alLower.begin(), alLower.end(), alLower.begin(), ::tolower);
                            if (!analyzer->symbolTable().lookupModule(alLower)) {
                                auto enumSym = std::make_unique<Symbol>(
                                    SymbolKind::EnumMember, al.first, Vb6Type::Long,
                                    SourceLocation{}, AccessLevel::Public);
                                enumSym->isBuiltin = true;
                                enumSym->hasConstValue = true;
                                enumSym->constIntValue = al.second;
                                enumSym->constType = Vb6Type::Long;
                                analyzer->symbolTable().define(std::move(enumSym));
                            }
                        }
                    }
                }
            }
        }
            // P24-04: 注册ComModule符号 (TKIND_MODULE → ActiveX DLL全局函数命名空间)
            // VBMAN.Version() 这种调用: VBMAN是工程名/模块名, Version是全局函数
            for (auto& tl : typelibParser_->cachedResults()) {
                for (auto& mod : tl->modules) {
                    auto sym = std::make_unique<Symbol>(
                        SymbolKind::ComModule, mod->name, Vb6Type::Object,
                        SourceLocation{}, AccessLevel::Public);
                    sym->isBuiltin = true;
                    sym->comModuleDllPath = mod->dllPath;
                    for (auto& func : mod->functions) {
                        Symbol::ComMethodSig sig;
                        sig.realName = func.realName;
                        sig.memid = func.memid;
                        sig.vtableIndex = -1;  // 模块函数无vtable
                        sig.returnType = func.returnType;
                        sig.isPropertyGet = false;
                        sig.isPropertyPut = false;
                        sig.isPropertyPutRef = false;
                        for (auto& param : func.params) {
                            ParameterInfo pi;
                            pi.name = param.name;
                            pi.type = param.type;
                            pi.isByVal = (param.direction == ComParamDir::In);
                            pi.isOptional = param.isOptional;
                            sig.params.push_back(std::move(pi));
                        }
                        sym->comModuleFunctions[func.name] = std::move(sig);
                    }
                    analyzer->symbolTable().define(std::move(sym));
                }
            }

            // P24-04: 注册VB_GlobalNameSpace promoted函数为ComGlobalNs符号
            // GlobalNameSpace coclass (如sGlobal) 的默认接口Public方法提升为全局符号
            // 示例: VBMAN库的sGlobal._sGlobal.VBMAN() → 全局"VBMAN"函数 (SymbolKind::ComGlobalNs)
            // 代码生成: VBMAN() → vb6_ComCallObject(vb6_CreateObject(L"VBMANLIB.sGlobal"), L"VBMAN", NULL, 0)
            for (auto& tl : typelibParser_->cachedResults()) {
                for (auto& cc : tl->coclasses) {
                    if (!cc->isGlobalNamespace || !cc->defaultIface) continue;
                    // 只提升非标准方法 (排除IUnknown/IDispatch的7个标准方法)
                    // IDispatch dispatch接口: 前7个是IUnknown(3)+IDispatch(4)标准方法
                    size_t standardMethods = cc->defaultIface->isDispatch ? 7 : 3;
                    for (size_t mi = standardMethods; mi < cc->defaultIface->members.size(); mi++) {
                        auto& member = cc->defaultIface->members[mi];
                        // 每个promoted方法注册为独立的ComGlobalNs符号
                        auto sym = std::make_unique<Symbol>(
                            SymbolKind::ComGlobalNs, member.realName, member.returnType,
                            SourceLocation{}, AccessLevel::Public);
                        sym->isBuiltin = true;
                        sym->comClsidStr = cc->clsidStr;
                        sym->comProgId = cc->progId;
                        sym->comDefaultIfaceName = cc->defaultIfaceName;
                        sym->comDefaultIfaceIid = cc->defaultIface->iidStr;
                        sym->comDefaultMemberName = cc->defaultIface->defaultMemberName;
                        sym->comDefaultMemberRealName = cc->defaultIface->defaultMemberRealName;
                        sym->comGlobalNsMethodName = member.realName;
                        // 复制方法签名
                        Symbol::ComMethodSig sig;
                        sig.realName = member.realName;
                        sig.memid = member.memid;
                        sig.vtableIndex = member.vtableIndex;
                        sig.returnType = member.returnType;
                        sig.isPropertyGet = (member.kind == ComMemberKind::PropertyGet);
                        sig.isPropertyPut = (member.kind == ComMemberKind::PropertyPut);
                        sig.isPropertyPutRef = (member.kind == ComMemberKind::PropertyPutRef);
                        for (auto& param : member.params) {
                            ParameterInfo pi;
                            pi.name = param.name;
                            pi.type = param.type;
                            pi.isByVal = (param.direction == ComParamDir::In);
                            pi.isOptional = param.isOptional;
                            sig.params.push_back(std::move(pi));
                        }
                        sym->comMethods[member.name] = std::move(sig);
                        analyzer->symbolTable().define(std::move(sym));
                    }
                }
            }
        // P7.5: 注册窗体控件名为符号 (否则语义分析报"未声明的标识符")
        auto frmIt = frmFiles_.find(module->moduleName);
        if (frmIt != frmFiles_.end()) {
            const auto& frmDesc = frmIt->second.form;
            // 注册窗体名本身 (Form1.Caption 访问)
            {
                auto sym = std::make_unique<Symbol>(
                    SymbolKind::Variable, module->moduleName, Vb6Type::Object,
                    SourceLocation{}, AccessLevel::Public);
                sym->isBuiltin = true;
                analyzer->symbolTable().define(std::move(sym));
            }
            // 注册控件名 (去重: 控件数组只注册一次)
            std::unordered_set<std::string> registeredCtrls;
            for (const auto& ctrl : frmDesc.formControl.children) {
                std::string ctrlLower = ctrl.controlName;
                for (auto& c : ctrlLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if (registeredCtrls.count(ctrlLower)) continue;
                registeredCtrls.insert(ctrlLower);
                auto sym = std::make_unique<Symbol>(
                    SymbolKind::Variable, ctrl.controlName, Vb6Type::Object,
                    SourceLocation{}, AccessLevel::Public);
                sym->isBuiltin = true;
                analyzer->symbolTable().define(std::move(sym));
            }
        }

        // Fix 047: Pre-register Public Enum and UDT types from all other modules
        // so that parameter types like "As UcsAsyncSocketEventMaskEnum" resolve
        // to Long/UserDefinedType during Pass 1 (before runCrossModuleResolution).
        // Without this, cross-module Enum/UDT parameter types are resolved as Variant,
        // causing false-positive vb6_VariantFromValue() wrapping at call sites → C2440.
        for (auto& otherModule : modules_) {
            if (otherModule.get() == module.get()) continue;
            for (auto& decl : otherModule->declarations) {
                if (decl->kind == ASTNodeKind::EnumDecl) {
                    auto& enumDecl = static_cast<EnumDecl&>(*decl);
                    // AccessLevel::Default == Public, so all non-Private enums are public
                    if (enumDecl.access != AccessLevel::Private) {
                        std::string lower = Symbol::toLower(enumDecl.name);
                        if (!analyzer->symbolTable().lookupModule(lower)) {
                            auto sym = std::make_unique<Symbol>(
                                SymbolKind::EnumType, enumDecl.name,
                                Vb6Type::Long, enumDecl.loc, AccessLevel::Public
                            );
                            sym->isExternal = true;
                            sym->sourceModule = otherModule->moduleName;
                            analyzer->symbolTable().defineExternal(std::move(sym));
                        }
                    }
                } else if (decl->kind == ASTNodeKind::TypeDecl) {
                    auto& typeDecl = static_cast<TypeDecl&>(*decl);
                    if (typeDecl.access != AccessLevel::Private) {
                        std::string lower = Symbol::toLower(typeDecl.name);
                        if (!analyzer->symbolTable().lookupModule(lower)) {
                            auto sym = std::make_unique<Symbol>(
                                SymbolKind::UserDefinedType, typeDecl.name,
                                Vb6Type::UserDefinedType, typeDecl.loc, AccessLevel::Public
                            );
                            sym->isExternal = true;
                            sym->sourceModule = otherModule->moduleName;
                            analyzer->symbolTable().defineExternal(std::move(sym));
                        }
                    }
                }
            }
        }

        bool ok = analyzer->analyze(*module);

        if (options.dumpSymbols) {
            analyzer->dumpSymbols(std::cout);
        }

        if (!ok) return false;
        analyzers_.push_back(std::move(analyzer));
    }
    return !diag_->hasErrors();
}

} // namespace vb6c3
