#include "backend/cgen.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <functional>
#include <cstdio>

namespace vb6c3 {

// --- cgen_expr_call.cpp: IndexOrCallExpr 求值（数组索引 / 函数调用统一路径） ---

void CCodeGen::visit(IndexOrCallExpr& node) {
    // 检测数组访问: arr(i) — callee 是 IdentifierExpr 且在已知数组集合中
    // VB6 不区分数组索引和函数调用, 统一为 IndexOrCallExpr
    bool isArrayAccess = false;
    std::string arrName;
    std::string arrNameLower;  // Fix 056: 小写名称用于查找 arrayUdtElemTypes_ / arrayDimCounts_
    Vb6Type arrElemType = Vb6Type::Variant;



    // P14.1.5: Check if identifier is a ParamArray parameter of current procedure

    bool isParamArrayAccess = false;

    std::string paName;

    if (node.callee && node.callee->kind == ASTNodeKind::IdentifierExpr && node.named.empty() && currentProc_) {

        auto& ident = static_cast<IdentifierExpr&>(*node.callee);

        for (auto& p : currentProc_->params) {

            if (p.isParamArray) {

                std::string pLower = p.name;

                std::transform(pLower.begin(), pLower.end(), pLower.begin(), ::tolower);

                std::string iLower = ident.name;

                std::transform(iLower.begin(), iLower.end(), iLower.begin(), ::tolower);

                if (pLower == iLower) {

                    isParamArrayAccess = true;

                    paName = cIdent(p.name);

                    break;

                }

            }

        }

    }



    if (isParamArrayAccess) {

        // ParamArray access: args(i) -> vb6_PA_GetLong(args, i) or vb6_PA_GetBSTR(args, i)

        if (node.positional.size() == 1) {

            emitExpr(*node.positional[0]);

            std::string index = std::move(lastExpr_);

            lastExpr_ = "vb6_PA_GetLong(" + paName + ", " + index + ")";

        } else {

            lastExpr_ = paName;  // bare reference to the SAFEARRAY*

        }

        return;

    }

    if (node.callee && node.callee->kind == ASTNodeKind::IdentifierExpr && node.named.empty()) {
        auto& ident = static_cast<IdentifierExpr&>(*node.callee);
        std::string lower = ident.name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        // 先查已知数组集合 (cgen过程中维护)
        if (knownArrays_.count(lower)) {
            isArrayAccess = true;
            arrName = cIdent(ident.name);
            arrNameLower = lower;  // Fix 056: 保存小写名
            arrElemType = arrayElemTypes_[lower];
            // Fix 010o: 类模块成员数组需要 me-> 前缀 (除非是局部变量)
            if (isClassModule_ && currentProc_ && !knownLocalVars_.count(lower)) {
                arrName = "me->" + arrName;
            }
            // Fix 010r-6 rev2: ByRef array param needs dereference (*name) since it's now **
            if (currentProc_) {
                for (auto& param : currentProc_->params) {
                    if (Symbol::toLower(param.name) == lower && !param.isByVal
                        && (static_cast<uint16_t>(param.type) & static_cast<uint16_t>(Vb6Type::Array))) {
                        arrName = "(*" + arrName + ")";
                        break;
                    }
                }
            }
        } else {
            // 再查符号表 (模块级数组)
            Symbol* sym = symTab_.lookupModule(ident.name);
            if (sym && sym->kind == SymbolKind::Variable && sym->isArray) {
                isArrayAccess = true;
                arrName = cIdent(ident.name);
                arrNameLower = lower;  // Fix 056: 保存小写名
                arrElemType = sym->type;
                // Fix 010o: 类模块成员数组需要 me-> 前缀
                if (isClassModule_ && currentProc_ && !sym->isExternal && !knownLocalVars_.count(lower)) {
                    arrName = "me->" + arrName;
                }
                // Fix 010r-6 rev2: ByRef array param needs dereference (*name) since it's now **
                if (currentProc_) {
                    for (auto& param : currentProc_->params) {
                        if (Symbol::toLower(param.name) == lower && !param.isByVal
                            && (static_cast<uint16_t>(param.type) & static_cast<uint16_t>(Vb6Type::Array))) {
                            arrName = "(*" + arrName + ")";
                            break;
                        }
                    }
                }
            }
        }
    }
    // Variant数组索引: a(i) 其中a是Variant变量(可能持有SafeArray)
    // VB6: a = Array(1,2,3); MsgBox a(0) → vb6_VariantArrayGet(&a, 0)
    if (!isArrayAccess && node.callee && node.callee->kind == ASTNodeKind::IdentifierExpr && node.named.empty() && !node.positional.empty()) {
        auto& vIdent = static_cast<IdentifierExpr&>(*node.callee);
        std::string vLower = vIdent.name;
        std::transform(vLower.begin(), vLower.end(), vLower.begin(), ::tolower);
        if (knownVariantVars_.count(vLower)) {
            emitExpr(*node.positional[0]);
            // Fix 084o: 索引为 Variant 时转 Long (vb6_VariantArrayGet 第二参是 int32_t)
            std::string vIndex = toLongIfVariant(std::move(lastExpr_), node.positional[0].get());
            // Fix 091k: ByRef Variant 参数在 C 侧已是 vb6_VARIANT* (cgen_util 参数映射
            // ByRef→指针), 再取址会传 vb6_VARIANT** → C4047/C4024 warning 且语义错
            // (cToolsArray prop_let_Extend/DeArray 的 Vars(i)/Arr(i)).
            std::string vArg = "&" + cIdent(vIdent.name);
            if (currentProc_) {
                for (auto& p091k : currentProc_->params) {
                    if (Symbol::toLower(p091k.name) != vLower) continue;
                    bool paArr091k = (static_cast<uint16_t>(p091k.type)
                                      & static_cast<uint16_t>(Vb6Type::Array)) != 0;
                    if (!p091k.isByVal && !paArr091k && !p091k.isParamArray) {
                        vArg = cIdent(vIdent.name);
                    }
                    break;
                }
            }
            lastExpr_ = "vb6_VariantArrayGet(" + vArg + ", " + vIndex + ")";
            return;
        }
    // P24-10: COM默认属性调用 — obj(args) 其中obj是COM变量, 等价于 obj.DefaultMember(args)
    // VB6: dict(0) → dict.Item(0), collection(1) → collection._Item(1)
    // DISPID_VALUE=0标识默认成员, 在TypeLib解析时已提取到Symbol::comDefaultMemberName
    if (!isArrayAccess && node.callee && node.callee->kind == ASTNodeKind::IdentifierExpr && node.named.empty() && !node.positional.empty()) {
        auto& comIdent = static_cast<IdentifierExpr&>(*node.callee);
        std::string comLower = comIdent.name;
        std::transform(comLower.begin(), comLower.end(), comLower.begin(), ::tolower);

        // 前期绑定COM变量 (Dim d As Dictionary → knownTypedComVars_["d"] = &Dictionary)
        auto itTyped = knownTypedComVars_.find(comLower);
        if (itTyped != knownTypedComVars_.end() && !itTyped->second->comDefaultMemberName.empty()) {
            const Symbol* comSym = itTyped->second;
            emitExpr(*node.callee);
            std::string objExpr = std::move(lastExpr_);
            std::string defMember = comSym->comDefaultMemberRealName;

            // 查找默认成员签名以确定返回类型
            std::string defLower = comSym->comDefaultMemberName;
            auto itSig = comSym->comMethods.find(defLower);

            // 有参数的默认属性调用: obj(args) → vb6_ComCall*(obj, L"Item", args, argc)
            std::vector<std::string> packedArgs;
            for (size_t i = 0; i < node.positional.size(); i++) {
                std::string packFn = comPackExpr(*node.positional[i]);
                emitExpr(*node.positional[i]);
                { std::string resolved = resolveComMarkerForPack(packFn); if (!resolved.empty()) lastExpr_ = resolved; }
                packedArgs.push_back(packFn + "(" + lastExpr_ + ")");
            }
            int32_t argc = (int32_t)packedArgs.size();
            std::string argsArray = "(void*[]){";
            for (int i = 0; i < argc; i++) {
                if (i > 0) argsArray += ", ";
                argsArray += packedArgs[i];
            }
            argsArray += "}";

            std::string callArgs = objExpr + ", L\"" + defMember + "\", " + argsArray + ", " + std::to_string(argc);

            // 根据签名返回类型选择类型化调用函数
            if (itSig != comSym->comMethods.end()) {
                std::string returnType = mapType(itSig->second.returnType);
                if (returnType == "BSTR") {
                    lastExpr_ = "vb6_ComCallBSTR(" + callArgs + ")";
                } else if (returnType == "int32_t" || returnType == "int16_t") {
                    lastExpr_ = "vb6_ComCallInt(" + callArgs + ")";
                } else if (returnType == "double" || returnType == "float") {
                    lastExpr_ = "vb6_ComCallDouble(" + callArgs + ")";
                } else if (returnType == "void*") {
                    lastExpr_ = "vb6_ComCallObject(" + callArgs + ")";
                } else {
                    lastExpr_ = "vb6_ComCall(" + callArgs + ")";
                }
            } else {
                lastExpr_ = "vb6_ComCall(" + callArgs + ")";
            }
            return;
        }

        // P24-10 TODO: 后期绑定COM变量 (Dim obj As Object → knownObjectVars_)
        // 后期绑定的 obj(args) 需要VARIANT返回类型推导, 暂不支持
        // 前期绑定 (Dim d As Dictionary) 已完全支持
    }

    }


    if (isArrayAccess) {
        // P8.1: 数组元素访问, 支持多维
        emitExpr(*node.callee);
        std::string callee = std::move(lastExpr_);

        int dimCount = 1;
        std::string dcKey = arrNameLower.empty() ? arrName : arrNameLower;
        auto itDc = arrayDimCounts_.find(dcKey);
        if (itDc != arrayDimCounts_.end()) dimCount = itDc->second;

        // Fix 055: UDT数组元素使用实际UDT C类型而非vb6_VARIANT
        std::string elemCType;
        std::string udKey = arrNameLower.empty() ? arrName : arrNameLower;
        auto itUdtArr = arrayUdtElemTypes_.find(udKey);
        if (itUdtArr != arrayUdtElemTypes_.end()) {
            elemCType = itUdtArr->second;  // e.g. "vb6_type_RECT"
        } else {
            elemCType = mapSaElemCType(arrElemType);
        }

        // Fix 081f: Dynamic array params default dimCount=1, but VB6 arr(i,j) has 2 indices.
        // Use positional.size() as the authoritative dimCount when it's >= 2.
        int actualDimCount = dimCount;
        if ((int)node.positional.size() >= 2 && (int)node.positional.size() > dimCount) {
            actualDimCount = (int)node.positional.size();
        }

        if (actualDimCount == 1 || node.positional.size() == 1) {
            // 一维访问: arr(i) -> VB6_SA_AT(type, arr, i)
            std::string index = "0";
            if (!node.positional.empty()) {
                emitExpr(*node.positional[0]);
                // Fix 090an: Variant 下标 (如 CopyMemory 中 baBuffer(maxLen),
                // maxLen As Optional Variant ByRef) — VB6_SA_AT 宏内 [(idx)-lBound]
                // 对 vb6_VARIANT 做减法 → C2088. 与 084o 一致转 Long.
                index = toLongIfVariant(std::move(lastExpr_), node.positional[0].get());
            }
            lastExpr_ = "VB6_SA_AT(" + elemCType + ", " + arrName + ", " + index + ")";
        } else if (actualDimCount == 2 && node.positional.size() == 2) {
            // 二维访问: arr(i, j) -> VB6_SA_ND_AT2(type, (vb6_SafeArrayND*)arr, i, j)
            // Fix 056: 动态数组声明为vb6_SafeArray1D*但ReDim后可能是ND, 需要强转
            emitExpr(*node.positional[0]);
            std::string idx0 = toLongIfVariant(std::move(lastExpr_), node.positional[0].get());
            emitExpr(*node.positional[1]);
            std::string idx1 = toLongIfVariant(std::move(lastExpr_), node.positional[1].get());
            std::string ndArr = "(vb6_SafeArrayND*)" + arrName;
            lastExpr_ = "VB6_SA_ND_AT2(" + elemCType + ", " + ndArr + ", " + idx0 + ", " + idx1 + ")";
        } else if (actualDimCount == 3 && node.positional.size() == 3) {
            // 三维访问: arr(i, j, k) -> VB6_SA_ND_AT3(type, (vb6_SafeArrayND*)arr, i, j, k)
            emitExpr(*node.positional[0]);
            std::string idx0 = std::move(lastExpr_);
            emitExpr(*node.positional[1]);
            std::string idx1 = std::move(lastExpr_);
            emitExpr(*node.positional[2]);
            std::string idx2 = std::move(lastExpr_);
            std::string ndArr = "(vb6_SafeArrayND*)" + arrName;
            lastExpr_ = "VB6_SA_ND_AT3(" + elemCType + ", " + ndArr + ", " + idx0 + ", " + idx1 + ", " + idx2 + ")";
        } else {
            // 4+维: 通用通过vb6_SafeArrayND_Offset + 直接指针访问
            std::vector<std::string> indices;
            for (auto& arg : node.positional) {
                emitExpr(*arg);
                indices.push_back(std::move(lastExpr_));
            }
            // 构建indices数组 + offset计算
            std::string offVar = "_ndoff_" + std::to_string(tempCounter_++);
            c_.emitLine("int " + offVar + " = vb6_SafeArrayND_Offset(" + arrName + ", " + std::to_string(actualDimCount) + ", (int[]){" + indices[0] + ", " + indices[1] + "});");
            lastExpr_ = "((" + elemCType + "*)(((char*)" + arrName + "->data) + " + offVar + "))[0]";
        }
        return;
    }

    // P14.2.4: IIf特殊处理 - 调用类型化RTL函数(函数调用语义确保两个分支都求值,符合VB6规范)
    if (node.callee && node.callee->kind == ASTNodeKind::IdentifierExpr && node.positional.size() == 3) {
        auto& ident = static_cast<IdentifierExpr&>(*node.callee);
        std::string iifLower = ident.name;
        std::transform(iifLower.begin(), iifLower.end(), iifLower.begin(), ::tolower);
        if (iifLower == "iif") {
            emitExpr(*node.positional[0]);
            std::string cond = std::move(lastExpr_);
            // VB6 True=-1, C needs explicit !=0
            cond = "((" + cond + ") != 0)";
            emitExpr(*node.positional[1]);
            std::string trueVal = std::move(lastExpr_);
            emitExpr(*node.positional[2]);
            std::string falseVal = std::move(lastExpr_);
            // Type dispatch: BSTR > double > long
            auto isBstrResult = [&](const std::string& e) -> bool {
                if (e.compare(0, 8, "vb6_BSTR") == 0) return true;
                if (e.find("VB6_SA_AT(BSTR,") != std::string::npos) return true;
                static const char* bstrPfx[] = {"vb6_BSTR_FromStr","vb6_Left","vb6_Right","vb6_Mid",
                    "vb6_UCase","vb6_LCase","vb6_Trim","vb6_LTrim","vb6_RTrim","vb6_Chr",
                    "vb6_Str","vb6_CStr","vb6_Format","vb6_Replace","vb6_Space","vb6_String",
                    "vb6_Command","vb6_CurDir","vb6_Environ","vb6_Dir",
                    "vb6_IIfBSTR","vb6_InputBox","vb6_App_Path","vb6_App_EXEName","vb6_App_HelpFile",
                    "vb6_GetControlText","vb6_GetControlCaption",nullptr};
                for (int i = 0; bstrPfx[i]; i++)
                    if (e.compare(0, strlen(bstrPfx[i]), bstrPfx[i]) == 0) return true;
                std::string low = e;
                std::transform(low.begin(), low.end(), low.begin(), ::tolower);
                return knownBstrVars_.count(low) > 0;
            };
            // Fix 046: IIf Variant arg extraction — when typed IIf function
            // is selected but an arg is a Variant, extract the concrete value.
            auto iifArgIsVariant = [&](int idx) -> bool {
                const std::string& val = (idx == 1) ? trueVal : falseVal;
                if (cExprIsVariant(val)) return true;
                auto& arg = node.positional[idx];
                if (arg->kind == ASTNodeKind::IdentifierExpr) {
                    auto& idArg = static_cast<IdentifierExpr&>(*arg);
                    std::string argLower = idArg.name;
                    std::transform(argLower.begin(), argLower.end(), argLower.begin(), ::tolower);
                    if (knownVariantVars_.count(argLower)) return true;
                }
                return false;
            };
            auto extractVariantArg = [&](std::string& val, int idx, const char* extractFn) {
                if (iifArgIsVariant(idx)) {
                    val = std::string(extractFn) + "(" + val + ")";
                }
            };
            if (isBstrResult(trueVal) || isBstrResult(falseVal)) {
                extractVariantArg(trueVal, 1, "vb6_VariantToString");
                extractVariantArg(falseVal, 2, "vb6_VariantToString");
                lastExpr_ = "vb6_IIfBSTR(" + cond + ", " + trueVal + ", " + falseVal + ")";
            } else if (trueVal.find('.') != std::string::npos || falseVal.find('.') != std::string::npos) {
                extractVariantArg(trueVal, 1, "vb6_VariantToDouble");
                extractVariantArg(falseVal, 2, "vb6_VariantToDouble");
                lastExpr_ = "vb6_IIfDouble(" + cond + ", " + trueVal + ", " + falseVal + ")";
            } else if (iifArgIsVariant(1) && iifArgIsVariant(2)) {
                // Both args are Variant — use vb6_IIfVariant (accepts VARIANT args)
                lastExpr_ = "vb6_IIfVariant(" + cond + ", " + trueVal + ", " + falseVal + ")";
            } else {
                extractVariantArg(trueVal, 1, "vb6_VariantToLong");
                extractVariantArg(falseVal, 2, "vb6_VariantToLong");
                lastExpr_ = "vb6_IIfLong(" + cond + ", " + trueVal + ", " + falseVal + ")";
            }
            return;
        }
    }

    // P18-D: VarPtr special handling - returns address of variable
    if (node.callee && node.callee->kind == ASTNodeKind::IdentifierExpr && node.positional.size() == 1) {
        auto& ident = static_cast<IdentifierExpr&>(*node.callee);
        std::string vpLower = ident.name;
        std::transform(vpLower.begin(), vpLower.end(), vpLower.begin(), ::tolower);
        if (vpLower == "varptr") {
            emitExpr(*node.positional[0]);
            /* Fix 082: VarPtr must return intptr_t, not int32_t.
               On x64, (int32_t)(intptr_t) truncates 8-byte pointers.
               Use (intptr_t) instead — safe on both x86 (4 bytes) and x64 (8 bytes). */
            /* Fix 084aa: VarPtr(函数调用) — 调用结果不是左值, &(call) → C2102.
               用复合字面量 &(void*){call} 包装 (VarPtr(.Glob(0)) 中的 .Glob 是 COM 属性
               调用 vb6_ComCall(...)); 变量/字段/数组元素保持原 &(expr). */
            bool vpIsCall = false;
            if (!lastExpr_.empty()
                && (std::isalpha(static_cast<unsigned char>(lastExpr_[0])) || lastExpr_[0] == '_')) {
                size_t vpParen = lastExpr_.find('(');
                if (vpParen != std::string::npos) {
                    vpIsCall = true;
                    for (size_t j = 0; j < vpParen; j++) {
                        char ch = lastExpr_[j];
                        if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_') {
                            vpIsCall = false;
                            break;
                        }
                    }
                }
            }
            // Fix 092s: VB6_SA_AT / VB6_SA_ND_ATn 展开后是**左值** (SafeArray 元素),
            // 不能按"调用结果非左值"走复合字面量 — VarPtr(uOutput.Buffer(0)) 曾生成
            // (intptr_t)&(void*){VB6_SA_AT(...)} → C2440 (cTlsSocket 4093).
            // 注意 vb6_VariantArrayGet 按值返回, 不在白名单内.
            bool vpIsLvalue092s = lastExpr_.compare(0, 10, "VB6_SA_AT(") == 0
                || lastExpr_.compare(0, 12, "VB6_SA_ND_AT") == 0;
            if (vpIsCall && !vpIsLvalue092s) {
                lastExpr_ = "(intptr_t)&(void*){" + lastExpr_ + "}";
            } else if (!lastExpr_.empty() && (std::isdigit(static_cast<unsigned char>(lastExpr_[0])) || lastExpr_[0] == '(')) {
                if (lastExpr_.size() > 1 && lastExpr_[0] == '(' && lastExpr_[1] == '*') {
                    // Fix 090h: (*name) / (*name).field / (*name)[idx] — ByRef 参数解引用是左值
                    // (如 VarPtr(File) → (*File), File As ByRef Variant; VarPtr(uFile.BufferArray)
                    // → (*uFile).BufferArray). 此前落入下方复合字面量分支, 生成
                    // (intptr_t)&(int32_t){(*File)} → C2440 (vb6_VARIANT → int32_t 初始化失败).
                    lastExpr_ = "(intptr_t)&(" + lastExpr_ + ")";
                } else {
                    // Fix 086: VarPtr(常量) — 常量被内联为字面量 (或括号表达式), 不可取址.
                    // 用 int32_t 复合字面量承载 (VB6 语义: 取常量临时副本地址).
                    lastExpr_ = "(intptr_t)&(int32_t){" + lastExpr_ + "}";
                }
            } else {
                lastExpr_ = "(intptr_t)&(" + lastExpr_ + ")";
            }
            return;
        }
    }

    // Fix 092t: TypeName(Me) — Me 在 C 侧是类指针 (vb6_cls_pvSubClass*), 而
    // vb6_TypeName 形参是 vb6_VARIANT → C2440 (pvSubClass 543/544,
    // clsSubClass.cls 497/498: App.LogEvent TypeName(Me) & ...).
    // 类名在编译期已知 (vbp 的 Class= 名 == moduleName_), 直接折叠为字符串字面量 —
    // 这也是 VB6 语义上唯一正确的形态: TypeName(对象实例) 返回其实类的名字,
    // 经 Variant 包装对象只会得到 "Object".
    if (node.callee && node.callee->kind == ASTNodeKind::IdentifierExpr
        && node.positional.size() == 1 && node.positional[0]
        && node.positional[0]->kind == ASTNodeKind::MeExpr && isClassModule_) {
        auto& tn092t = static_cast<IdentifierExpr&>(*node.callee);
        std::string tnLower092t = tn092t.name;
        std::transform(tnLower092t.begin(), tnLower092t.end(), tnLower092t.begin(), ::tolower);
        if (tnLower092t == "typename") {
            lastExpr_ = "vb6_BSTR_FromStr(L\"" + moduleName_ + "\")";
            return;
        }
    }


    // P18-E: Choose special handling - nested ternary chain
    if (node.callee && node.callee->kind == ASTNodeKind::IdentifierExpr && !node.positional.empty()) {
        auto& ident = static_cast<IdentifierExpr&>(*node.callee);
        std::string csLower = ident.name;
        std::transform(csLower.begin(), csLower.end(), csLower.begin(), ::tolower);
        if (csLower == "choose" && node.positional.size() >= 2) {
            emitExpr(*node.positional[0]);
            std::string idx = std::move(lastExpr_);
            std::string result = "NULL";
            for (int i = (int)node.positional.size() - 1; i >= 1; i--) {
                emitExpr(*node.positional[i]);
                result = "((" + idx + ")==" + std::to_string(i) + " ? (" + lastExpr_ + ") : (" + result + "))";
            }
            lastExpr_ = result;
            return;
        }
        if (csLower == "switch" && node.positional.size() >= 2 && node.positional.size() % 2 == 0) {
            std::string result = "NULL";
            for (int i = (int)node.positional.size() - 2; i >= 0; i -= 2) {
                emitExpr(*node.positional[i]);
                std::string cond = "((" + lastExpr_ + ")!=0)";
                emitExpr(*node.positional[i + 1]);
                result = "(" + cond + " ? (" + lastExpr_ + ") : (" + result + "))";
            }
            lastExpr_ = result;
            return;
        }
    }

        // P21-B: Array(arglist) special handling - creates a Variant SafeArray
    // Array(1, "hello", 3.14) → vb6_ArrayCreate(3) + vb6_ArraySetLong/SetBSTR/SetDouble
    if (node.callee && node.callee->kind == ASTNodeKind::IdentifierExpr) {
        auto& arrIdent = static_cast<IdentifierExpr&>(*node.callee);
        std::string arrLower = arrIdent.name;
        std::transform(arrLower.begin(), arrLower.end(), arrLower.begin(), ::tolower);
        if (arrLower == "array" && node.positional.size() >= 0) {
            int count = (int)node.positional.size();
            std::string arrVar = "_arr_" + std::to_string(tempCounter_++);
            if (count > 0) {
                c_.emitLine("vb6_SafeArray1D* " + arrVar + " = vb6_ArrayCreate(" + std::to_string(count) + ");");
                for (int i = 0; i < count; i++) {
                    emitExpr(*node.positional[i]);
                    std::string argExpr = std::move(lastExpr_);
                    // Detect argument type for proper setter selection
                    bool isLongArg = false;
                    bool isDoubleArg = false;
                    if (node.positional[i]->kind == ASTNodeKind::LiteralExpr) {
                        auto& lit = static_cast<LiteralExpr&>(*node.positional[i]);
                        if (lit.literalKind == LiteralKind::Integer) isLongArg = true;
                        else if (lit.literalKind == LiteralKind::Double) isDoubleArg = true;
                    }
                    // Fix 038b-3: 检测 Variant 实参 — Array() 元素通过 vb6_ArraySetLong/
                    // SetBSTR/SetDouble 设置, 这些函数期望具体类型. 当实参是 Variant
                    // (如 vb6_VariantArrayGet, Variant 变量等) 时, 需要先提取具体值.
                    // 仅使用 cExprIsVariant (C 字符串级) 和 knownVariantVars_ 检测.
                    bool argIsVariant = cExprIsVariant(argExpr);
                    if (!argIsVariant && node.positional[i]->kind == ASTNodeKind::IdentifierExpr) {
                        auto& idArg = static_cast<IdentifierExpr&>(*node.positional[i]);
                        std::string argLower = idArg.name;
                        std::transform(argLower.begin(), argLower.end(), argLower.begin(), ::tolower);
                        if (knownVariantVars_.count(argLower)) argIsVariant = true;
                    }
                    // 也检测 BSTR 变量
                    bool argIsBstr = false;
                    if (node.positional[i]->kind == ASTNodeKind::IdentifierExpr) {
                        auto& idArg = static_cast<IdentifierExpr&>(*node.positional[i]);
                        std::string argLower = idArg.name;
                        std::transform(argLower.begin(), argLower.end(), argLower.begin(), ::tolower);
                        if (knownBstrVars_.count(argLower)) argIsBstr = true;
                    }
                    if (isLongArg) {
                        c_.emitLine("vb6_ArraySetLong(" + arrVar + ", " + std::to_string(i) + ", " + argExpr + ");");
                    } else if (isDoubleArg) {
                        c_.emitLine("vb6_ArraySetDouble(" + arrVar + ", " + std::to_string(i) + ", " + argExpr + ");");
                    } else if (argIsVariant) {
                        // Variant 实参: 无法确定具体类型, 统一用 VariantToLong 转换
                        // (Array() 的元素通常是数值索引). 若为 BSTR Variant 则需
                        // VariantToString, 但 Array() 上下文中 Long 是最常见的默认.
                        // 更安全: 使用 vb6_VariantFromValue 让 _Generic 选择, 然后
                        // 直接用 vb6_ArraySetVariant (如果存在) 或回退到 Long.
                        bool looksLikeBSTR = (argExpr.find("vb6_BSTR") != std::string::npos ||
                                              argExpr.find("L\"") != std::string::npos);
                        // Fix 081i: 也用 inferExprType 检测 BSTR 类型
                        if (!looksLikeBSTR) {
                            Vb6Type argVt = inferExprType(*node.positional[i]);
                            if (argVt == Vb6Type::String) looksLikeBSTR = true;
                        }
                        if (looksLikeBSTR) {
                            c_.emitLine("vb6_ArraySetBSTR(" + arrVar + ", " + std::to_string(i) + ", vb6_VariantToString(" + argExpr + "));");
                        } else {
                            c_.emitLine("vb6_ArraySetLong(" + arrVar + ", " + std::to_string(i) + ", vb6_VariantToLong(" + argExpr + "));");
                        }
                    } else if (argIsBstr) {
                        c_.emitLine("vb6_ArraySetBSTR(" + arrVar + ", " + std::to_string(i) + ", " + argExpr + ");");
                    } else {
                        bool looksLikeBSTR = (argExpr.find("vb6_BSTR") != std::string::npos ||
                                              argExpr.find("L\"") != std::string::npos);
                        // Fix 081i: 也用 inferExprType 检测, 避免内置函数返回 BSTR
                        // (如 vb6_ErrSource/vb6_ErrDescription) 被误判为 Long
                        if (!looksLikeBSTR) {
                            Vb6Type argVt = inferExprType(*node.positional[i]);
                            if (argVt == Vb6Type::String) looksLikeBSTR = true;
                        }
                        if (looksLikeBSTR) {
                            c_.emitLine("vb6_ArraySetBSTR(" + arrVar + ", " + std::to_string(i) + ", " + argExpr + ");");
                        } else {
                            // Default: treat as Long (covers variable references etc.)
                            c_.emitLine("vb6_ArraySetLong(" + arrVar + ", " + std::to_string(i) + ", " + argExpr + ");");
                        }
                    }
                }
            } else {
                c_.emitLine("vb6_SafeArray1D* " + arrVar + " = vb6_ArrayCreate(0);");
            }
            lastExpr_ = arrVar;
            return;
        }
    }

    // 函数调用路径 (原有逻辑)
    // P20-36: IsMissing(arg) -> (!_has_arg) for Optional params
    // Must intercept BEFORE callee mapping to vb6_IsMissing
    if (node.callee && node.callee->kind == ASTNodeKind::IdentifierExpr && node.named.empty()) {
        auto& ismIdent = static_cast<IdentifierExpr&>(*node.callee);
        std::string ismLower = ismIdent.name;
        std::transform(ismLower.begin(), ismLower.end(), ismLower.begin(), ::tolower);
        if (ismLower == "ismissing" && node.positional.size() == 1) {
            auto& arg = node.positional[0];
            if (arg->kind == ASTNodeKind::IdentifierExpr) {
                std::string argName = static_cast<IdentifierExpr&>(*arg).name;
                std::string argLower = argName;
                std::transform(argLower.begin(), argLower.end(), argLower.begin(), ::tolower);
                if (currentProc_) {
                    for (auto& p : currentProc_->params) {
                        std::string pLower = p.name;
                        std::transform(pLower.begin(), pLower.end(), pLower.begin(), ::tolower);
                        if (pLower == argLower && p.isOptional && !p.isParamArray) {
                            lastExpr_ = "(!_has_" + cIdent(p.name) + ")";
                            return;
                        }
                        // Fix 091k: ParamArray 整体 — RTL vb6_IsMissing(SAFEARRAY*)
                        // 语义为"未传任何实参" (psa == NULL).
                        if (pLower == argLower && p.isParamArray) {
                            lastExpr_ = "vb6_IsMissing(" + cIdent(p.name) + ")";
                            return;
                        }
                    }
                }
                // Not an Optional param - IsMissing returns False (0)
                lastExpr_ = "(0)";
                return;
            }
            // Fix 091k: 实参是复合表达式 (数组元素 / ParamArray 元素 / 函数结果).
            // VB6 IsMissing 仅对 Optional Variant 形参可能为 True, 其余恒 False;
            // 而 RTL vb6_IsMissing 形参是 SAFEARRAY* (ParamArray 专用), 直接透传
            // Variant 值会 C2440 (cToolsArray: IsMissing(Vars(i)), IsMissing(OutVars(i))).
            lastExpr_ = "(0)";
            return;
        }
    }

    // Fix 038: Len(udt) → sizeof(udt) — VB6 Len on UDT returns size in bytes.
    // vb6_Len is declared as int32_t vb6_Len(BSTR), so passing a UDT causes C2440.
    // Must intercept BEFORE callee mapping to vb6_Len.
    if (node.callee && node.callee->kind == ASTNodeKind::IdentifierExpr && node.named.empty()) {
        auto& lenIdent = static_cast<IdentifierExpr&>(*node.callee);
        std::string lenLower = lenIdent.name;
        std::transform(lenLower.begin(), lenLower.end(), lenLower.begin(), ::tolower);
        if (lenLower == "len" && node.positional.size() == 1) {
            std::string udtCType = inferUdtTypeOfExpr(*node.positional[0]);
            if (!udtCType.empty()) {
                emitExpr(*node.positional[0]);
                lastExpr_ = "((int32_t)sizeof(" + lastExpr_ + "))";
                return;
            }
        }
    }

    // ---- Fix 037: IdentifierExpr callee — implicit Me.field / ByRef Object param / local Object var ----
    // 处理 name(idx) 模式 (无显式 obj. 前缀), name 可能是:
    //   Pattern G: 类模块隐式 Me.field (void*/Variant) — VB6 在类模块内省略 Me. 时
    //              codegen 会 emit me->field, 但 field 是数据字段不是函数 → C2064
    //              → vb6_ComCall(me->field, L"Item", ...) / vb6_VariantArrayGet(&me->field, idx)
    //   Pattern F: ByRef Object 参数 (*name)(idx) — VB6 Items(idx) 其中 Items 是 ByRef Object
    //              参数, C 中是 void** → (*Items) 是 void*, 不可调用 → C2064
    //              → vb6_ComCall((*name), L"Item", args, argc)
    //   Pattern F2: 局部 Object 变量 (Dim x As Object) — knownObjectVars_ 后期绑定
    //              → vb6_ComCall(name, L"Item", args, argc)
    // 检查顺序: Pattern F (参数优先, 遮蔽类字段) > Pattern G (类字段) > Pattern F2 (局部 Object)
    if (node.callee && node.callee->kind == ASTNodeKind::IdentifierExpr
        && !node.positional.empty() && node.named.empty()) {
        auto& idExpr = static_cast<IdentifierExpr&>(*node.callee);
        std::string nameLower = Symbol::toLower(idExpr.name);
        std::string nameMLower = "m_" + nameLower;
        bool handled = false;

        // ---- Pattern F: Object 参数 (ByRef 或 ByVal) ----
        // currentProc_->params 中查找同名参数, 非数组, 非ParamArray, 类型为 Object/Variant.
        // ByRef Object 在 C 中是 void** → emitExpr emit "(*name)"; ByVal Object 是 void* → emit "name".
        if (!handled && currentProc_) {
            for (auto& p : currentProc_->params) {
                if (Symbol::toLower(p.name) == nameLower) {
                    bool isArray = (static_cast<uint16_t>(p.type) & static_cast<uint16_t>(Vb6Type::Array)) != 0;
                    if (!isArray && !p.isParamArray
                        && (p.type == Vb6Type::Object || p.type == Vb6Type::Variant)) {
                        emitExpr(*node.callee);  // emits "(*name)" for ByRef or "name" for ByVal
                        std::string objExpr = std::move(lastExpr_);
                        std::vector<std::string> packedArgs;
                        for (size_t i = 0; i < node.positional.size(); i++) {
                            std::string packFn = comPackExpr(*node.positional[i]);
                            emitExpr(*node.positional[i]);
                            { std::string resolved = resolveComMarkerForPack(packFn);
                              if (!resolved.empty()) lastExpr_ = resolved; }
                            packedArgs.push_back(packFn + "(" + lastExpr_ + ")");
                        }
                        int32_t argc = (int32_t)packedArgs.size();
                        std::string argsArray = "(void*[]){";
                        for (int i = 0; i < argc; i++) {
                            if (i > 0) argsArray += ", ";
                            argsArray += packedArgs[i];
                        }
                        argsArray += "}";
                        lastExpr_ = "vb6_ComCall(" + objExpr + ", L\"Item\", "
                                  + argsArray + ", " + std::to_string(argc) + ")";
                        handled = true;
                    }
                    break;  // 参数匹配即终止 (无论是否 handled)
                }
            }
        }

        // ---- Pattern G: 隐式 Me.field (类模块上下文) ----
        // isClassModule_ && name 不是局部变量 && 不是参数 → 类数据字段.
        // emitExpr(*node.callee) 会 emit "me->field" (+ Dim As New 自动实例化守卫).
        if (!handled && isClassModule_ && currentProc_
            && !knownLocalVars_.count(nameLower)) {
            // 确认 name 不是当前过程的参数 (Pattern F 已处理)
            bool isParam = false;
            for (auto& p : currentProc_->params) {
                if (Symbol::toLower(p.name) == nameLower) { isParam = true; break; }
            }
            if (!isParam) {
                // Variant 字段 → vb6_VariantArrayGet(&me->field, idx)
                if (classVariantMembers_.count(nameLower) || classVariantMembers_.count(nameMLower)) {
                    emitExpr(*node.callee);  // emits "me->field"
                    std::string fieldExpr = std::move(lastExpr_);
                    emitExpr(*node.positional[0]);
                    // Fix 084o: 索引为 Variant 时转 Long
                    std::string idx = toLongIfVariant(std::move(lastExpr_), node.positional[0].get());
                    lastExpr_ = "vb6_VariantArrayGet(&" + fieldExpr + ", " + idx + ")";
                    handled = true;
                } else {
                    // void* COM 字段 → vb6_ComCall(me->field, L"Item", args, argc)
                    bool isVoidPtr = false;
                    if (classVoidFieldMap_) {
                        auto itV = classVoidFieldMap_->find(moduleName_);
                        if (itV != classVoidFieldMap_->end()) {
                            if (itV->second.count(nameLower) || itV->second.count(nameMLower)) {
                                isVoidPtr = true;
                            }
                        }
                    }
                    if (isVoidPtr) {
                        emitExpr(*node.callee);  // emits "me->field" (+ auto-instantiate guard)
                        std::string objExpr = std::move(lastExpr_);
                        std::vector<std::string> packedArgs;
                        for (size_t i = 0; i < node.positional.size(); i++) {
                            std::string packFn = comPackExpr(*node.positional[i]);
                            emitExpr(*node.positional[i]);
                            { std::string resolved = resolveComMarkerForPack(packFn);
                              if (!resolved.empty()) lastExpr_ = resolved; }
                            packedArgs.push_back(packFn + "(" + lastExpr_ + ")");
                        }
                        int32_t argc = (int32_t)packedArgs.size();
                        std::string argsArray = "(void*[]){";
                        for (int i = 0; i < argc; i++) {
                            if (i > 0) argsArray += ", ";
                            argsArray += packedArgs[i];
                        }
                        argsArray += "}";
                        lastExpr_ = "vb6_ComCall(" + objExpr + ", L\"Item\", "
                                  + argsArray + ", " + std::to_string(argc) + ")";
                        handled = true;
                    } else if (classTypedFieldMap_) {
                        // Fix 037b: typed (非 void*) 对象字段 — 项目类或 COM 接口
                        auto itT = classTypedFieldMap_->find(moduleName_);
                        if (itT != classTypedFieldMap_->end()) {
                            auto itF = itT->second.find(nameLower);
                            if (itF == itT->second.end()) itF = itT->second.find(nameMLower);
                            if (itF != itT->second.end()) {
                                const std::string& fieldType = itF->second;
                                if (fieldType.compare(0, 4, "COM:") == 0) {
                                    // Pattern K: COM 接口字段 (如 Header As Dictionary)
                                    // → vb6_ComCall((void*)me->field, L"Item", args, argc)
                                    emitExpr(*node.callee);
                                    std::string objExpr = std::move(lastExpr_);
                                    std::vector<std::string> packedArgs;
                                    for (size_t i = 0; i < node.positional.size(); i++) {
                                        std::string packFn = comPackExpr(*node.positional[i]);
                                        emitExpr(*node.positional[i]);
                                        { std::string resolved = resolveComMarkerForPack(packFn);
                                          if (!resolved.empty()) lastExpr_ = resolved; }
                                        packedArgs.push_back(packFn + "(" + lastExpr_ + ")");
                                    }
                                    int32_t argc = (int32_t)packedArgs.size();
                                    std::string argsArray = "(void*[]){";
                                    for (int i = 0; i < argc; i++) {
                                        if (i > 0) argsArray += ", ";
                                        argsArray += packedArgs[i];
                                    }
                                    argsArray += "}";
                                    lastExpr_ = "vb6_ComCall((void*)" + objExpr + ", L\"Item\", "
                                              + argsArray + ", " + std::to_string(argc) + ")";
                                    handled = true;
                                } else if (node.positional.size() == 1) {
                                    // Pattern L: 项目类字段 (如 Rows As cCollection)
                                    // → vb6_<Type>_prop_get_Item(me->field, vb6_VariantFromValue(arg))
                                    std::string itemFn = resolveClassMemberCall(fieldType, "Item");
                                    if (!itemFn.empty()) {
                                        emitExpr(*node.callee);
                                        std::string objExpr = std::move(lastExpr_);
                                        emitExpr(*node.positional[0]);
                                        std::string arg = std::move(lastExpr_);
                                        lastExpr_ = itemFn + "(" + objExpr + ", vb6_VariantFromValue(" + arg + "))";
                                        lastExprNeedsObjectUnpack_ = true;  // Set 语句需转 void*
                                        handled = true;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // ---- Pattern F2: 局部 Object 变量 (后期绑定) ----
        // knownObjectVars_: Dim x As Object → C 中 x 是 void*, x(idx) 应为 vb6_ComCall(x, ...).
        if (!handled && knownObjectVars_.count(nameLower)) {
            emitExpr(*node.callee);  // emits "name"
            std::string objExpr = std::move(lastExpr_);
            std::vector<std::string> packedArgs;
            for (size_t i = 0; i < node.positional.size(); i++) {
                std::string packFn = comPackExpr(*node.positional[i]);
                emitExpr(*node.positional[i]);
                { std::string resolved = resolveComMarkerForPack(packFn);
                  if (!resolved.empty()) lastExpr_ = resolved; }
                packedArgs.push_back(packFn + "(" + lastExpr_ + ")");
            }
            int32_t argc = (int32_t)packedArgs.size();
            std::string argsArray = "(void*[]){";
            for (int i = 0; i < argc; i++) {
                if (i > 0) argsArray += ", ";
                argsArray += packedArgs[i];
            }
            argsArray += "}";
            lastExpr_ = "vb6_ComCall(" + objExpr + ", L\"Item\", "
                      + argsArray + ", " + std::to_string(argc) + ")";
            handled = true;
        }

        if (handled) return;
    }

    // ---- Fix 037: MemberAccessExpr callee with positional args — array field/COM dispatch ----
    // 处理 obj.field(idx) 模式, field 可能是:
    //   - UDT 数组字段 (固定/动态)     — Pattern A/B → obj.member[idx] / VB6_SA_AT(...)
    //   - 类 Variant 字段持有 SafeArray — Pattern C → vb6_VariantArrayGet(&obj->member, idx)
    //   - 类 void* 字段持有 COM 对象    — Pattern D → vb6_ComCall(obj->member, L"Item", args, argc)
    // 若 member 是该类的方法/属性, resolveClassMemberCall 返回非空 → 不拦截 (正常函数调用路径).
    // 不拦截则 fallback path (line ~2158) 把 field 误当函数调用 → C2064.
    if (node.callee && node.callee->kind == ASTNodeKind::MemberAccessExpr
        && !node.positional.empty() && node.named.empty()) {
        auto& maExpr = static_cast<MemberAccessExpr&>(*node.callee);
        std::string memLower = Symbol::toLower(maExpr.memberName);
        std::string memLowerM = "m_" + memLower;
        bool handled = false;

        // ---- Pattern A/B: UDT 数组字段 ----
        // 仅当 maExpr.object 的 UDT 类型可推断 (IdentifierExpr / WithMemberExpr / 嵌套
        // MemberAccessExpr). inferUdtTypeOfExpr 找到 UDT C 类型 → 在 udtMembers 中查 member.
        if (maExpr.object) {
            std::string udtCType = inferUdtTypeOfExpr(*maExpr.object);
            if (!udtCType.empty()) {
                const std::string prefix = "vb6_type_";
                if (udtCType.size() > prefix.size()
                    && udtCType.compare(0, prefix.size(), prefix) == 0) {
                    std::string udtName = udtCType.substr(prefix.size());
                    Symbol* udtSym = symTab_.lookupModule(udtName);
                    if (udtSym && udtSym->kind == SymbolKind::UserDefinedType) {
                        for (auto& mi : udtSym->udtMembers) {
                            if (Symbol::toLower(mi.name) == memLower) {
                                if (mi.arraySize > 0) {
                                    // Pattern A: 固定大小数组成员 → obj.member[idx]
                                    emitExpr(*node.callee);  // emits "obj.member"
                                    std::string fieldExpr = std::move(lastExpr_);
                                    emitExpr(*node.positional[0]);
                                    std::string idx = std::move(lastExpr_);
                                    lastExpr_ = fieldExpr + "[" + idx + "]";
                                    handled = true;
                                } else if (mi.isArrayDynamic) {
                                    // Pattern B: 动态数组成员 → VB6_SA_AT(elemType, obj.member, idx)
                                    emitExpr(*node.callee);
                                    std::string fieldExpr = std::move(lastExpr_);
                                    emitExpr(*node.positional[0]);
                                    std::string idx = std::move(lastExpr_);
                                    // Fix 092s: UDT 元素类型成员数组须用 vb6_type_<UDT>
                                    // (mapSaElemCType 对 UserDefinedType 退化为 vb6_VARIANT
                                    // → 元素大小/取址全错, cTlsSocket 4086/4093)
                                    std::string elemCType = (mi.type == Vb6Type::UserDefinedType
                                                             && !mi.typeRefName.empty())
                                        ? ("vb6_type_" + cIdent(mi.typeRefName))
                                        : mapSaElemCType(mi.type);
                                    lastExpr_ = "VB6_SA_AT(" + elemCType + ", "
                                              + fieldExpr + ", " + idx + ")";
                                    handled = true;
                                } else if (mi.type == Vb6Type::Variant) {
                                    // Pattern J: UDT Variant 字段 (持有 SafeArray) →
                                    // vb6_VariantArrayGet(&obj.field, idx)
                                    emitExpr(*node.callee);  // emits "obj.field"
                                    std::string fieldExpr = std::move(lastExpr_);
                                    emitExpr(*node.positional[0]);
                                    // Fix 084o: 索引为 Variant 时转 Long
                                    std::string idx = toLongIfVariant(std::move(lastExpr_), node.positional[0].get());
                                    lastExpr_ = "vb6_VariantArrayGet(&" + fieldExpr + ", " + idx + ")";
                                    handled = true;
                                } else if (mi.type == Vb6Type::Object) {
                                    // Pattern H: UDT void* (Object) 字段 (持有 COM 对象) →
                                    // vb6_ComCall(obj.field, L"Item", args, argc)
                                    emitExpr(*node.callee);
                                    std::string objExpr = std::move(lastExpr_);
                                    std::vector<std::string> packedArgs;
                                    for (size_t i = 0; i < node.positional.size(); i++) {
                                        std::string packFn = comPackExpr(*node.positional[i]);
                                        emitExpr(*node.positional[i]);
                                        { std::string resolved = resolveComMarkerForPack(packFn);
                                          if (!resolved.empty()) lastExpr_ = resolved; }
                                        packedArgs.push_back(packFn + "(" + lastExpr_ + ")");
                                    }
                                    int32_t argc = (int32_t)packedArgs.size();
                                    std::string argsArray = "(void*[]){";
                                    for (int i = 0; i < argc; i++) {
                                        if (i > 0) argsArray += ", ";
                                        argsArray += packedArgs[i];
                                    }
                                    argsArray += "}";
                                    lastExpr_ = "vb6_ComCall(" + objExpr + ", L\"Item\", "
                                              + argsArray + ", " + std::to_string(argc) + ")";
                                    handled = true;
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }

        // ---- Pattern C/D: 类实例字段访问 ----
        if (!handled && maExpr.object) {
            std::string className = inferClassTypeOfExpr(*maExpr.object);
            if (!className.empty()) {
                // resolveClassMemberCall 验证 member 是否为方法/属性:
                // 是 → 跳过 (正常函数调用 fallback 处理); 否 → 数据字段 → 检查 Variant/void*
                std::string resolvedFn = resolveClassMemberCall(className, maExpr.memberName);
                if (resolvedFn.empty()) {
                    // Pattern C: Variant 字段 → vb6_VariantArrayGet(&obj->member, idx)
                    if (classVariantMembers_.count(memLower)
                        || classVariantMembers_.count(memLowerM)) {
                        emitExpr(*node.callee);  // emits "obj->member  /* class var .X field */"
                        std::string fieldExpr = std::move(lastExpr_);
                        emitExpr(*node.positional[0]);
                        // Fix 084o: 索引为 Variant 时转 Long
                        std::string idx = toLongIfVariant(std::move(lastExpr_), node.positional[0].get());
                        lastExpr_ = "vb6_VariantArrayGet(&" + fieldExpr + ", " + idx + ")";
                        handled = true;
                    } else {
                        // Pattern D: void* COM 字段 → vb6_ComCall(obj->member, L"Item", args, argc)
                        bool isVoidPtr = false;
                        if (classVoidFieldMap_) {
                            auto itV = classVoidFieldMap_->find(className);
                            if (itV != classVoidFieldMap_->end()) {
                                if (itV->second.count(memLower)
                                    || itV->second.count(memLowerM)) {
                                    isVoidPtr = true;
                                }
                            }
                        }
                        if (isVoidPtr) {
                            // 模仿 line 1880-1938 COM 默认属性调用 (comPackExpr/resolveComMarkerForPack)
                            emitExpr(*node.callee);  // emits "obj->member  /* ... voidptr */"
                            std::string objExpr = std::move(lastExpr_);
                            std::vector<std::string> packedArgs;
                            for (size_t i = 0; i < node.positional.size(); i++) {
                                std::string packFn = comPackExpr(*node.positional[i]);
                                emitExpr(*node.positional[i]);
                                { std::string resolved = resolveComMarkerForPack(packFn);
                                  if (!resolved.empty()) lastExpr_ = resolved; }
                                packedArgs.push_back(packFn + "(" + lastExpr_ + ")");
                            }
                            int32_t argc = (int32_t)packedArgs.size();
                            std::string argsArray = "(void*[]){";
                            for (int i = 0; i < argc; i++) {
                                if (i > 0) argsArray += ", ";
                                argsArray += packedArgs[i];
                            }
                            argsArray += "}";
                            // 默认成员名 "Item" — VB6 Collection / ADODB.Recordset 等大多数
                            // void* 字段都是用 .Item(idx) 索引的 (DISPID_VALUE 默认成员).
                            lastExpr_ = "vb6_ComCall(" + objExpr + ", L\"Item\", "
                                      + argsArray + ", " + std::to_string(argc) + ")";
                            handled = true;
                        } else if (classTypedFieldMap_) {
                            // Fix 037b: typed (非 void*) 对象字段 — 项目类或 COM 接口
                            auto itT = classTypedFieldMap_->find(className);
                            if (itT != classTypedFieldMap_->end()) {
                                auto itF = itT->second.find(memLower);
                                if (itF == itT->second.end()) itF = itT->second.find(memLowerM);
                                if (itF != itT->second.end()) {
                                    const std::string& fieldType = itF->second;
                                    if (fieldType.compare(0, 4, "COM:") == 0) {
                                        // Pattern K: COM 接口字段 (如 Header As Dictionary)
                                        // → vb6_ComCall((void*)obj->member, L"Item", args, argc)
                                        emitExpr(*node.callee);
                                        std::string objExpr = std::move(lastExpr_);
                                        std::vector<std::string> packedArgs;
                                        for (size_t i = 0; i < node.positional.size(); i++) {
                                            std::string packFn = comPackExpr(*node.positional[i]);
                                            emitExpr(*node.positional[i]);
                                            { std::string resolved = resolveComMarkerForPack(packFn);
                                              if (!resolved.empty()) lastExpr_ = resolved; }
                                            packedArgs.push_back(packFn + "(" + lastExpr_ + ")");
                                        }
                                        int32_t argc = (int32_t)packedArgs.size();
                                        std::string argsArray = "(void*[]){";
                                        for (int i = 0; i < argc; i++) {
                                            if (i > 0) argsArray += ", ";
                                            argsArray += packedArgs[i];
                                        }
                                        argsArray += "}";
                                        lastExpr_ = "vb6_ComCall((void*)" + objExpr + ", L\"Item\", "
                                                  + argsArray + ", " + std::to_string(argc) + ")";
                                        handled = true;
                                    } else if (node.positional.size() == 1) {
                                        // Pattern L: 项目类字段 (如 Rows As cCollection)
                                        // → vb6_<Type>_prop_get_Item(obj->member, vb6_VariantFromValue(arg))
                                        std::string itemFn = resolveClassMemberCall(fieldType, "Item");
                                        if (!itemFn.empty()) {
                                            emitExpr(*node.callee);
                                            std::string objExpr = std::move(lastExpr_);
                                            emitExpr(*node.positional[0]);
                                            std::string arg = std::move(lastExpr_);
                                            lastExpr_ = itemFn + "(" + objExpr + ", vb6_VariantFromValue(" + arg + "))";
                                            lastExprNeedsObjectUnpack_ = true;  // Set 语句需转 void*
                                            handled = true;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        if (handled) return;
    }

    // ---- Fix 060: WithMemberExpr callee with positional args — With block UDT array field ----
    // 处理 With 块内 .Data(index) 模式, 其中 .Data 是 UDT 动态数组字段
    // WithMemberExpr 不同于 MemberAccessExpr, 需要单独处理
    if (node.callee && node.callee->kind == ASTNodeKind::WithMemberExpr
        && !node.positional.empty() && node.named.empty()
        && !withObjectVars_.empty() && !withObjectInfoStack_.empty()) {
        auto& wmExpr = static_cast<WithMemberExpr&>(*node.callee);
        const auto& info = withObjectInfoStack_.back();
        if (info.kind == WithObjKind::Unknown) {
            // UDT With block — 查找 UDT 类型
            const std::string& tempVar = withObjectVars_.back();
            std::string tempLower = Symbol::toLower(tempVar);
            auto it = knownUdtVars_.find(tempLower);
            if (it != knownUdtVars_.end()) {
                const std::string prefix = "vb6_type_";
                const std::string& udtCType = it->second;
                if (udtCType.size() > prefix.size()
                    && udtCType.compare(0, prefix.size(), prefix) == 0) {
                    std::string udtName = udtCType.substr(prefix.size());
                    Symbol* udtSym = symTab_.lookupModule(udtName);
                    if (udtSym && udtSym->kind == SymbolKind::UserDefinedType) {
                        std::string memLower = Symbol::toLower(wmExpr.memberName);
                        for (auto& mi : udtSym->udtMembers) {
                            if (Symbol::toLower(mi.name) == memLower) {
                                bool isHandled = false;
                                if (mi.arraySize > 0) {
                                    // Pattern A: 固定大小数组字段 → tempVar->member[idx]
                                    // Fix 081j-2: With 块临时变量是指针，用 -> 访问成员
                                    emitExpr(*node.positional[0]);
                                    std::string idx = std::move(lastExpr_);
                                    lastExpr_ = tempVar + "->" + cIdent(mi.name) + "[" + idx + "]";
                                    isHandled = true;
                                } else if (mi.isArrayDynamic) {
                                    // Pattern B: 动态数组字段 → VB6_SA_AT(elemType, tempVar->member, idx)
                                    // Fix 081j-2: With 块临时变量是指针，用 -> 访问成员
                                    emitExpr(*node.positional[0]);
                                    std::string idx = std::move(lastExpr_);
                                    // Fix 092s: 同 3626 — UDT 元素类型成员数组
                                    std::string elemCType = (mi.type == Vb6Type::UserDefinedType
                                                             && !mi.typeRefName.empty())
                                        ? ("vb6_type_" + cIdent(mi.typeRefName))
                                        : mapSaElemCType(mi.type);
                                    lastExpr_ = "VB6_SA_AT(" + elemCType + ", "
                                              + tempVar + "->" + cIdent(mi.name) + ", " + idx + ")";
                                    isHandled = true;
                                }
                                if (isHandled) return;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    // M22: 设置asCallCallee_标志, 让IdentifierExpr知道当前是函数调用callee上下文
    // 这确保递归调用时(如 Factorial(n-1))返回函数名而非返回值变量
    // Fix 015: 加 save/restore. 原代码硬编码 `asCallCallee_=false` 会丢失嵌套 callee
    // 上下文 (如 CallStmt→MemberAccessExpr.Fix015→IndexOrCallExpr 链中, 内层 IndexOrCallExpr
    // 反复重置成 false, 外层 Fix 015 看到的是 false 而非 CallStmt 设置的 true).
    // 同时清空 pendingChainObj_ 防止跨调用泄漏 (Fix 015 在 callee emission 时设置).
    pendingChainObj_.clear();
    bool savedAsCallCallee = asCallCallee_;
    asCallCallee_ = true;
    emitExpr(*node.callee);
    asCallCallee_ = savedAsCallCallee;
    std::string callee = std::move(lastExpr_);

    // Bug #3 fix: 清除COM Picture标志 — 每次新的IndexOrCallExpr重置
    // 后续根据被调用函数的返回类型重新设置
    lastExprIsComPicture_ = false;

    // Fix 032: args (positional + named + early-COM 早期路径) 是 值上下文, 不可能是
    // callee 上下文. 外层 CallStmt(2159-2162) 或嵌套 IndexOrCallExpr 可能把
    // asCallCallee_ 留在 true 状态. 若不强制 false, 后续所有 emitExpr(子表达式)
    // 作为参数发联时, 会被 IdentifierExpr 的自引用检查 (line ~199-213) 误判为
    // callee 上下文, 导致 Property Get / Function 返回值变量名引用错误地返回
    // 过程名 (如 vb6_cTlsReMaster_LocalHostName 而非 vb6_ret_LocalHostName).
    // RAII: 函数退出时 (任何 return 或自然走到末尾) 自动恢复为 savedAsCallCallee.
    struct CallCalleeValueScope {
        bool& ref;
        bool saved;
        CallCalleeValueScope(bool& r, bool s) : ref(r), saved(s) { ref = false; }
        ~CallCalleeValueScope() { ref = saved; }
    } _argsValueScope{asCallCallee_, savedAsCallCallee};
    (void)_argsValueScope;  // suppress unused-warning

    // --- P7.9: WebBrowser控件方法调用 ---
    // Navigate/GoBack/GoForward/Refresh via isComMarker_ flag set by MemberAccessExpr
    if (isComMarker_) {
        auto itCtrl = knownFormControls_.find(comObjExpr_);
        if (itCtrl != knownFormControls_.end() && itCtrl->second == FrmControlType::WebBrowser) {
            isComMarker_ = false;
            std::string method = std::move(comMemberName_);
            std::string ctrlName = cIdent(knownFormControlOriginalNames_.count(comObjExpr_) ? knownFormControlOriginalNames_[comObjExpr_] : comObjExpr_);
            comObjExpr_.clear();
            comMemberName_.clear();
            if (method == "navigate") {
                std::string urlArg = "0";
                if (!node.positional.empty()) {
                    emitExpr(*node.positional[0]);
                    urlArg = std::move(lastExpr_);
                }
                lastExpr_ = "(void)vb6_WebViewNavigate((void*)vb6_hwnd_" + ctrlName + ", " + urlArg + ")";
                return;
            } else if (method == "goback" || method == "goforward" || method == "refresh") {
                // Simplified: not yet implemented
                lastExpr_ = "(void)0";
                return;
            }
        }
    }

    // P13.3: ListBox/ComboBox methods (AddItem/RemoveItem/Clear/List) via isComMarker_ flag
    if (isComMarker_) {
        auto itCtrl = knownFormControls_.find(comObjExpr_);
        if (itCtrl != knownFormControls_.end() &&
            (itCtrl->second == FrmControlType::ListBox || itCtrl->second == FrmControlType::ComboBox)) {
            isComMarker_ = false;
            std::string method = std::move(comMemberName_);
            std::string ctrlName = cIdent(knownFormControlOriginalNames_.count(comObjExpr_) ? knownFormControlOriginalNames_[comObjExpr_] : comObjExpr_);
            comObjExpr_.clear();
            comMemberName_.clear();
            if (method == "additem") {
                std::string itemArg = "0";
                if (!node.positional.empty()) {
                    emitExpr(*node.positional[0]);
                    itemArg = std::move(lastExpr_);
                }
                c_.emitLine("vb6_AddItem((void*)vb6_hwnd_" + ctrlName + ", " + itemArg + ");  /* ListBox.AddItem */");
                lastExpr_ = "0";
                return;
            } else if (method == "removeitem") {
                std::string idxArg = "0";
                if (!node.positional.empty()) {
                    emitExpr(*node.positional[0]);
                    idxArg = std::move(lastExpr_);
                }
                c_.emitLine("vb6_RemoveItem((void*)vb6_hwnd_" + ctrlName + ", " + idxArg + ");  /* ListBox.RemoveItem */");
                lastExpr_ = "0";
                return;
            } else if (method == "clear") {
                c_.emitLine("vb6_ClearList((void*)vb6_hwnd_" + ctrlName + ");  /* ListBox.Clear */");
                lastExpr_ = "0";
                return;
            } else if (method == "list") {
                // List(idx) property read - List1.List(0)
                std::string idxArg = "0";
                if (!node.positional.empty()) {
                    emitExpr(*node.positional[0]);
                    idxArg = std::move(lastExpr_);
                }
                lastExpr_ = "vb6_GetListItem((void*)vb6_hwnd_" + ctrlName + ", " + idxArg + ")";
                return;
            }
        }
    }
    // --- COM后期绑定检测 (P6.2) + 前期绑定检测 (P6.3) + P6.4接口调用 ---
    // MemberAccessExpr为COM对象设置isComMarker_标志 + comObjExpr_/comMemberName_
    if (isComMarker_) {
        // ActiveX控件COM属性: 先获取属性对象, 再用Item(idx)索引
        // comObjExpr_以"vb6_com_"开头 = ActiveX控件变量
        if (comObjExpr_.find("vb6_com_") == 0 && !node.positional.empty()) {
            std::string axObjExpr = std::move(comObjExpr_);
            std::string axMember = std::move(comMemberName_);
            isComMarker_ = false;
            // Step 1: 获取属性对象 (如 ListImages 集合)
            std::string collectionExpr = "vb6_ComGetObjectProp(" + axObjExpr + ", L\"" + axMember + "\")";
            // Step 2: 调用 Item(idx) 获取集合中的元素
            std::vector<std::string> packedArgs;
            for (size_t i = 0; i < node.positional.size(); i++) {
                std::string packFn = comPackExpr(*node.positional[i]);
                emitExpr(*node.positional[i]);
                { std::string resolved = resolveComMarkerForPack(packFn); if (!resolved.empty()) lastExpr_ = resolved; }
                packedArgs.push_back(packFn + "(" + lastExpr_ + ")");
            }
            int32_t argc = (int32_t)packedArgs.size();
            std::string argsArray = "(void*[]){";
            for (int i = 0; i < argc; i++) {
                if (i > 0) argsArray += ", ";
                argsArray += packedArgs[i];
            }
            argsArray += "}";
            // 返回对象类型 (用于后续 .Picture 等链式访问)
            lastExpr_ = "vb6_ComCallObject(" + collectionExpr + ", L\"Item\", " + argsArray + ", " + std::to_string(argc) + ")";
            return;
        }
        isComMarker_ = false;  // 消费标记
        std::string objExpr = std::move(comObjExpr_);
        std::string memberName = std::move(comMemberName_);

        // Fix 057: Me.Controls.Add(ProgID, Name) → vb6_Form_ControlsAdd(hwnd, L"ProgID", L"Name")
        // 检测链式COM: objExpr = "vb6_ComGetObjectProp(vb6_hwnd_Form1, L"Controls")", memberName = "Add"
        {
            std::string memLower = memberName;
            std::transform(memLower.begin(), memLower.end(), memLower.begin(), ::tolower);
            std::string objExprLower = objExpr;
            std::transform(objExprLower.begin(), objExprLower.end(), objExprLower.begin(), ::tolower);
            if (memLower == "add" &&
                objExpr.find("vb6_ComGetObjectProp(vb6_hwnd_") == 0 &&
                objExprLower.find("l\"controls\")") != std::string::npos) {
                // 提取 form HWND 变量: vb6_ComGetObjectProp(vb6_hwnd_Form1, L"Controls") → vb6_hwnd_Form1
                size_t parenStart = strlen("vb6_ComGetObjectProp(");
                size_t commaPos = objExpr.find(", ", parenStart);
                std::string hwndExpr = objExpr.substr(parenStart, commaPos - parenStart);
                // 发射参数: Controls.Add(ProgID, Name)
                std::vector<std::string> argExprs;
                for (size_t i = 0; i < node.positional.size() && i < 2; i++) {
                    emitExpr(*node.positional[i]);
                    argExprs.push_back(lastExpr_);
                }
                if (argExprs.size() >= 2) {
                    lastExpr_ = "vb6_Form_ControlsAdd(" + hwndExpr + ", " + argExprs[0] + ", " + argExprs[1] + ")";
                    return;
                }
                // 参数不足, 降级为普通COM调用
            }
        }

        // P6.4: 接口引用方法调用 (Dim x As IFoo → x.Method → x.vtbl->Method(x.obj, args))
        {
            std::string objLower = objExpr;
            std::transform(objLower.begin(), objLower.end(), objLower.begin(), ::tolower);
            auto itIfaceVar = knownIfaceVars_.find(objLower);
            if (itIfaceVar != knownIfaceVars_.end() && !isEarlyBoundCom_) {
                std::string ifaceName = itIfaceVar->second;
                std::string ifaceId = cIdent(ifaceName);
                // 生成参数列表
                std::vector<std::string> callArgs;
                for (size_t i = 0; i < node.positional.size(); i++) {
                    emitExpr(*node.positional[i]);
                    callArgs.push_back(lastExpr_);
                }
                for (auto& named : node.named) {
                    emitExpr(*named.value);
                    callArgs.push_back(lastExpr_);
                }
                std::string argsStr;
                for (size_t i = 0; i < callArgs.size(); i++) {
                    if (i > 0) argsStr += ", ";
                    argsStr += callArgs[i];
                }
                // x.vtbl->Method(x.obj, args...)
                std::string call = objExpr + ".vtbl->" + cIdent(memberName) + "(" + objExpr + ".obj";
                if (!argsStr.empty()) call += ", " + argsStr;
                call += ")";
                lastExpr_ = call;
                return;
            }
        }

        // P6.3: 前期绑定 — 利用类型签名确定返回类型，统一走后期绑定(IDispatch)
        // 原因: vtable直接调用的函数签名可能不是VARIANT* (如get_Count用long*)，
        // vb6_ComVtableGet*辅助函数统一用VARIANT*签名会导致调用错误
        if (isEarlyBoundCom_ && earlyBoundSym_) {
            isEarlyBoundCom_ = false;
            const Symbol* comSym = earlyBoundSym_;
            earlyBoundSym_ = nullptr;

            // 查找方法签名以确定返回类型
            std::string memLower = memberName;
            std::transform(memLower.begin(), memLower.end(), memLower.begin(), ::tolower);
            auto it = comSym->comMethods.find(memLower);
            bool hasArgs = (!node.positional.empty() || !node.named.empty());


            // 无参属性Get: 用后期绑定属性读取，根据返回类型选函数
            if (it != comSym->comMethods.end() && it->second.isPropertyGet && !hasArgs) {
                const auto& sig = it->second;
                std::string returnType = mapType(sig.returnType);
                if (returnType == "BSTR") {
                    lastExpr_ = "vb6_ComGetStringProp(" + objExpr + ", L\"" + memberName + "\")";
                } else if (returnType == "int32_t" || returnType == "int16_t") {
                    lastExpr_ = "vb6_ComGetIntProp(" + objExpr + ", L\"" + memberName + "\")";
                } else if (returnType == "double" || returnType == "float") {
                    lastExpr_ = "vb6_ComGetDoubleProp(" + objExpr + ", L\"" + memberName + "\")";
                } else if (returnType == "void*") {
                    lastExpr_ = "vb6_ComGetObjectProp(" + objExpr + ", L\"" + memberName + "\")";
                } else {
                    lastExpr_ = "vb6_ComGetStringProp(" + objExpr + ", L\"" + memberName + "\")";
                }
                return;
            }
            // P24-07: 有参数的前期绑定方法 → 利用签名选择类型化COM调用函数
            if (it != comSym->comMethods.end() && hasArgs) {
                const auto& sig = it->second;
                std::string returnType = mapType(sig.returnType);
                std::vector<std::string> packedArgs;
                for (size_t i = 0; i < node.positional.size(); i++) {
                    std::string packFn = comPackExpr(*node.positional[i]);
                    emitExpr(*node.positional[i]);
                    { std::string resolved = resolveComMarkerForPack(packFn); if (!resolved.empty()) lastExpr_ = resolved; }
                    packedArgs.push_back(packFn + "(" + lastExpr_ + ")");
                }
                for (auto& named : node.named) {
                    std::string packFn = comPackExpr(*named.value);
                    emitExpr(*named.value);
                    { std::string resolved = resolveComMarkerForPack(packFn); if (!resolved.empty()) lastExpr_ = resolved; }
                    packedArgs.push_back(packFn + "(" + lastExpr_ + ")");
                }
                int32_t argc = (int32_t)packedArgs.size();
                std::string argsArray = "(void*[]){";
                for (int i = 0; i < argc; i++) {
                    if (i > 0) argsArray += ", ";
                    argsArray += packedArgs[i];
                }
                argsArray += "}";
                std::string callArgs = objExpr + ", L\"" + memberName + "\", " + argsArray + ", " + std::to_string(argc);
                if (returnType == "BSTR") {
                    lastExpr_ = "vb6_ComCallBSTR(" + callArgs + ")";
                } else if (returnType == "int32_t" || returnType == "int16_t") {
                    lastExpr_ = "vb6_ComCallInt(" + callArgs + ")";
                } else if (returnType == "double" || returnType == "float") {
                    lastExpr_ = "vb6_ComCallDouble(" + callArgs + ")";
                } else if (returnType == "void*") {
                    lastExpr_ = "vb6_ComCallObject(" + callArgs + ")";
                } else {
                    lastExpr_ = "vb6_ComCall(" + callArgs + ")";  // 未知返回类型: 返回void*
                }
                isComMarker_ = false;
                return;
            }
            // 有参数的方法/属性Put/签名未找到 → 降级为后期绑定 (fall through)
            isEarlyBoundCom_ = false;
        }
        isEarlyBoundCom_ = false;

        if (!node.positional.empty() || !node.named.empty()) {
            // 有参数: obj.Method(args) → vb6_ComCall(obj, L"Method", variantArgs, argc)
            std::vector<std::string> packedArgs;
            for (size_t i = 0; i < node.positional.size(); i++) {
                std::string packFn = comPackExpr(*node.positional[i]);
                emitExpr(*node.positional[i]);
                { std::string resolved = resolveComMarkerForPack(packFn); if (!resolved.empty()) lastExpr_ = resolved; }
                packedArgs.push_back(packFn + "(" + lastExpr_ + ")");
            }
            for (auto& named : node.named) {
                std::string packFn = comPackExpr(*named.value);
                emitExpr(*named.value);
                { std::string resolved = resolveComMarkerForPack(packFn); if (!resolved.empty()) lastExpr_ = resolved; }
                packedArgs.push_back(packFn + "(" + lastExpr_ + ")");
            }

            int32_t argc = (int32_t)packedArgs.size();
            std::string argsArray;
            argsArray = "(void*[]){";
            for (int i = 0; i < argc; i++) {
                if (i > 0) argsArray += ", ";
                argsArray += packedArgs[i];
            }
            argsArray += "}";

            lastExpr_ = "vb6_ComCall(" + objExpr + ", L\"" + memberName + "\", " +
                        argsArray + ", " + std::to_string(argc) + ")";
            isComMarker_ = false;  // P24-04: 参数emission可能设置脏isComMarker_
            return;
        } else {
            // 无参数: obj.Method() → vb6_ComCall(obj, L"Method", NULL, 0)
            lastExpr_ = "vb6_ComCall(" + objExpr + ", L\"" + memberName + "\", NULL, 0)";
            isComMarker_ = false;  // P24-04: 清除残留标记
            return;
        }
    }

    // Fix 044b: Check if callee is a WithMemberExpr class method that might need
    // Optional param padding. If so, skip the early-return and split the callee
    // to allow Optional padding in the normal flow below.
    bool needsSplitForOptionalPad = false;
    if (node.callee && node.callee->kind == ASTNodeKind::WithMemberExpr) {
        if (!withObjectInfoStack_.empty()) {
            const auto& info = withObjectInfoStack_.back();
            if (info.kind == WithObjKind::ClassInstance && !info.className.empty()) {
                needsSplitForOptionalPad = true;
            }
        }
    }
    // Fix 090c: 类成员方法无实参调用 (obj.M() / Trim(obj.M())) — 若 callee 已被 MAE 生成
    // 为完整调用 vb6_cls_X_M(obj) 且该方法声明有参数, 需拆分走 Optional padding 流程
    // (见下方 return 判定与 4040 拆分条件).
    bool needsSplitForMissingArgs = false;
    if (!needsSplitForOptionalPad && node.callee
        && node.callee->kind == ASTNodeKind::MemberAccessExpr
        && node.positional.empty() && node.named.empty()) {
        auto& maeFix090c = static_cast<MemberAccessExpr&>(*node.callee);
        std::string clsFix090c =
            maeFix090c.object ? inferClassTypeOfExpr(*maeFix090c.object) : "";
        if (!clsFix090c.empty()) {
            std::vector<ParameterInfo> ppFix090c;
            bool ibFix090c = false;
            if (findClassMemberCallParams(clsFix090c, maeFix090c.memberName,
                                          ppFix090c, ibFix090c)
                && !ppFix090c.empty() && !ibFix090c) {
                needsSplitForMissingArgs = true;
            }
        }
    }

    // 如果callee已经是func(args)形式(如类方法调用 vb6_Counter_GetCount(c)),
    // 且IndexOrCallExpr没有额外参数, 直接使用callee避免双重括号
    if (callee.size() >= 2 && callee.back() == ')' && node.positional.empty() && node.named.empty()) {
        // 检查是否是完整的函数调用（以右括号结尾且匹配左括号）
        int depth = 0;
        bool isCompleteCall = false;
        for (int i = (int)callee.size() - 2; i >= 0; i--) {
            if (callee[i] == ')') depth++;
            else if (callee[i] == '(') {
                if (depth == 0) { isCompleteCall = true; break; }
                depth--;
            }
        }
        if (isCompleteCall && !needsSplitForOptionalPad && !needsSplitForMissingArgs) {
            lastExpr_ = callee;
            return;
        }
        // needsSplitForMissingArgs / needsSplitForOptionalPad: 需继续执行拆分+补齐流程
    }

    // 如果callee已经是func()形式(无参内置函数调用如vb6_Now())，
    // 需要拆开重组为func(args)，因为IndexOrCallExpr表示带参数调用
    bool calleeIsZeroArgCall = false;
    if (callee.size() >= 2 && callee.substr(callee.size() - 2) == "()") {
        calleeIsZeroArgCall = true;
        callee = callee.substr(0, callee.size() - 2);
    }

    // Fix 083e: Variant 数组嵌套索引 — vGateway(lIdx)(0) 不是 f(a,b) 而是嵌套元素访问.
    // callee 若是 vb6_VariantArrayGet(&arr, idx) 完整调用, 外层 (0) 应对内层结果再取元素.
    // (否则 P6.5 拆开会把索引合并成 vb6_VariantArrayGet(&arr, idx, 0) → C2197 参数太多)
    // Fix 084f: 旧实现用 (vb6_VARIANT){<内层调用>} 复合字面量以值初始化结构体,
    // 首成员是 vb6_vartype → 触发 C2440 "vb6_VARIANT → vb6_vartype".
    // 改用按值辅助函数 vb6_VariantArrayGetVal(内层调用, 外层索引).
    if (callee.compare(0, 20, "vb6_VariantArrayGet(") == 0 && !node.positional.empty()) {
        std::string innerCall = callee;
        std::string outerIdx;
        emitExpr(*node.positional[0]);
        // Fix 084o: 外层索引为 Variant 时转 Long
        outerIdx = toLongIfVariant(std::move(lastExpr_), node.positional[0].get());
        lastExpr_ = "vb6_VariantArrayGetVal(" + innerCall + ", " + outerIdx + ")";
        return;
    }

    // P6.5修复: 如果callee已经是func(obj)形式(如类方法调用 vb6_Button_SetCaption(btn)),
    // 且IndexOrCallExpr有额外参数, 需要拆开重组为func(obj, userArgs...),
    // 避免生成 func(obj)(userArgs) 双重括号
    std::string classMethodObjArg;  // 如果非空, 表示callee已被拆开, 需要前置此参数

    // Fix 015: 若 MemberAccessExpr.Fix015 路径已通过 pendingChainObj_ 交付对象参数,
    // 移交给 classMethodObjArg (随后会被前置到参数列表).
    // 此时 callee 是裸函数名 (如 "vb6_cDataBase_Exec"), 上面的 split 路径因
    // callee.back() != ')' 不会触发, 故不会被双重设置.
    if (!pendingChainObj_.empty()) {
        classMethodObjArg = std::move(pendingChainObj_);
        pendingChainObj_.clear();
    }

    if (callee.size() >= 2 && callee.back() == ')'
        && ((!node.positional.empty() || !node.named.empty())
            || needsSplitForOptionalPad || needsSplitForMissingArgs)) {
        // 检查是否是完整的函数调用（以右括号结尾且匹配左括号）
        int depth = 0;
        int openPos = -1;
        for (int i = (int)callee.size() - 2; i >= 0; i--) {
            if (callee[i] == ')') depth++;
            else if (callee[i] == '(') {
                if (depth == 0) { openPos = i; break; }
                depth--;
            }
        }
        if (openPos > 0) {
            // callee = "funcName(existingArgs)" → 拆开
            std::string funcPart = callee.substr(0, openPos);
            classMethodObjArg = callee.substr(openPos + 1, callee.size() - openPos - 2);
            callee = funcPart;
            // Fix 086: 链式默认属性调用 — 内层是无参 prop_get_(如 .Root("data")) 或
            // COM 调用 (Dic(N)(RouteName)) 时, 内层返回 COM 对象, 外层索引是对返回
            // 对象的 Item 调用. 不能并入内层参数表 (C2197 参数太多).
            // 有声明参数的 prop_get (Prop(k)) 仍走合并 (内层只发了this).
            bool innerIsChainedObj = false;
            if (!node.positional.empty() && node.named.empty()) {
                // funcPart 是拆开后的裸函数名 (无括号)
                if (funcPart == "vb6_ComCall" || funcPart == "vb6_ComCallObject"
                    || funcPart == "vb6_ComGetObjectProp"
                    || funcPart == "vb6_VariantFromComResult") {
                    innerIsChainedObj = true;
                } else {
                    size_t gp = funcPart.find("_prop_get_");
                    if (gp != std::string::npos) {
                        std::string propName = funcPart.substr(gp + 10);
                        Symbol* propSym86 = symTab_.lookupModuleByKind(propName, SymbolKind::PropertyGet);
                        if (propSym86 && propSym86->params.empty()) {
                            innerIsChainedObj = true;
                        } else {
                            // Fix 092w: 内层 prop_get 调用已自带实参 (classMethodObjArg
                            // 顶层段数 >= 2, 即 this + 至少 1 个实参) 说明内层调用已自足,
                            // 外层索引是对其返回对象 (Variant/COM 对象) 的默认成员访问,
                            // 不能并入内层参数表 (否则实参个数超出签名).
                            // Demo_Database 506: TestDB.Rows(1)("score") — Rows 是 cCollection
                            // 字段, (1) 取元素返回 Variant(Dictionary), ("score") 应在其结果上
                            // 走默认成员 Item 的后期绑定调用.
                            int topArgs092w = classMethodObjArg.empty() ? 0 : 1;
                            int depth092w = 0;
                            for (char ch092w : classMethodObjArg) {
                                if (ch092w == '(' || ch092w == '[' || ch092w == '{') depth092w++;
                                else if (ch092w == ')' || ch092w == ']' || ch092w == '}') depth092w--;
                                else if (ch092w == ',' && depth092w == 0) topArgs092w++;
                            }
                            if (topArgs092w >= 2) innerIsChainedObj = true;
                        }
                    }
                }
            }
            if (innerIsChainedObj) {
                std::string innerObj = funcPart + "(" + classMethodObjArg + ")";
                // Fix 092w: 内层函数返回 vb6_VARIANT (如 cCollection.prop_get_Item) 时,
                // vb6_ComCall 的 obj 形参是 void* — 需先经 vb6_VariantToObjectVal 提取
                // 对象指针, 否则 vb6_VARIANT 直传 → C2440. (无参 prop_get 返回 void* 的
                // 既有路径保持直传不变.)
                if (variantReturnFuncs_ && variantReturnFuncs_->count(funcPart)) {
                    innerObj = "vb6_VariantToObjectVal(" + innerObj + ")";
                }
                std::vector<std::string> packedArgs86;
                for (size_t i = 0; i < node.positional.size(); i++) {
                    std::string packFn86 = comPackExpr(*node.positional[i]);
                    emitExpr(*node.positional[i]);
                    { std::string resolved86 = resolveComMarkerForPack(packFn86); if (!resolved86.empty()) lastExpr_ = resolved86; }
                    packedArgs86.push_back(packFn86 + "(" + lastExpr_ + ")");
                }
                int32_t argc86 = (int32_t)packedArgs86.size();
                std::string argsArray86 = "(void*[]){";
                for (int i = 0; i < argc86; i++) {
                    if (i > 0) argsArray86 += ", ";
                    argsArray86 += packedArgs86[i];
                }
                argsArray86 += "}";
                lastExpr_ = "vb6_VariantFromComResult(vb6_ComCall(" + innerObj + ", L\"Item\", "
                          + argsArray86 + ", " + std::to_string(argc86) + "))";
                return;
            }
        }
    }

    // 位置参数
    // 需要检查被调用函数的参数签名: ByRef参数在调用点需要传指针(&arg)
    std::vector<ParameterInfo> calleeParams;
    // Fix 030b: 跟踪被调用者是否为 builtin — builtin 用 calleeParams 仅为触发
    // Fix 024 P2/Fix 029 的参数包装, 但 RTL C 签名不接受 Optional padding 和
    // IsMissing _has_ flag 尾叜 (那些只适用于用户定义函数). 见 line 3207/3237.
    bool calleeIsBuiltin = false;
    // Fix 042a: Declare 函数 (DeclareSub/DeclareFunc) 的 C 签名不接受 _has_ 尾叜,
    // 与 builtin 类似 — Optional padding 仍需要 (C 函数期望所有参数),
    // 但 IsMissing _has_ flags 不应追加.
    bool calleeIsDeclare = false;
    // Fix 041b: Track whether calleeParams was successfully resolved (even if 0 params).
    // Used to distinguish "params not looked up" from "looked up with 0 params" (e.g., Property Get
    // with no params) — needed for arg truncation when args > params.
    bool calleeParamsFound = false;
    // 从IdentifierExpr或MemberAccessExpr获取被调用函数名, 在符号表中查找
    // Fix 027: 加入 DeclareSub / DeclareFunc — 否则 WinAPI Declare 的 ByRef 参数无法 emit &,
    //          导致 ByRef UDT/SafeArray/标量 全部按值传递 (例如 SOCKADDR_IN → SOCKADDR_IN* 错误).
    if (node.callee && node.callee->kind == ASTNodeKind::IdentifierExpr) {
        auto& idExpr = static_cast<IdentifierExpr&>(*node.callee);
        // 尝试多种查找方式: 先lookupModule(过程符号), 再lookup(嵌套作用域)
        Symbol* funcSym = symTab_.lookupModule(idExpr.name);
        if (!funcSym || (funcSym->kind != SymbolKind::Sub && funcSym->kind != SymbolKind::Function
            && funcSym->kind != SymbolKind::PropertyGet && funcSym->kind != SymbolKind::PropertyLet
            && funcSym->kind != SymbolKind::PropertySet
            && funcSym->kind != SymbolKind::DeclareSub && funcSym->kind != SymbolKind::DeclareFunc)) {
            funcSym = symTab_.lookup(idExpr.name);
        }
        if (funcSym && (funcSym->kind == SymbolKind::Sub || funcSym->kind == SymbolKind::Function
            || funcSym->kind == SymbolKind::PropertyGet || funcSym->kind == SymbolKind::PropertyLet
            || funcSym->kind == SymbolKind::PropertySet
            || funcSym->kind == SymbolKind::DeclareSub || funcSym->kind == SymbolKind::DeclareFunc)) {
            calleeParams = funcSym->params;
            calleeIsBuiltin = funcSym->isBuiltin;
            calleeParamsFound = true;
            // Fix 042a: Track Declare functions for _has_ flag suppression
            if (funcSym->kind == SymbolKind::DeclareSub || funcSym->kind == SymbolKind::DeclareFunc) {
                calleeIsDeclare = true;
            }
            // Bug #3 fix: 检测函数返回类型为StdPicture/IPictureDisp (COM Picture)
            // 用于Picture属性赋值时选择vb6_SetControlPictureFromCom
            if (!funcSym->variableTypeName.empty()) {
                std::string vtLower = funcSym->variableTypeName;
                std::transform(vtLower.begin(), vtLower.end(), vtLower.begin(), ::tolower);
                if (vtLower == "stdpicture" || vtLower == "ipicturedisp" || vtLower == "ipicture") {
                    lastExprIsComPicture_ = true;
                }
            }
        }
    } else if (node.callee && node.callee->kind == ASTNodeKind::MemberAccessExpr) {
        // Module.Method 或 obj.Method 调用: 查找方法名的参数签名
        // Fix 027: 同步加入 DeclareSub/DeclareFunc (模块方法形式的 declare 调用).
        auto& maExpr = static_cast<MemberAccessExpr&>(*node.callee);

        // Fix 033: 优先用类感知查找. 原 symTab_.lookupModule(maExpr.memberName) 在
        // 跨模块同名方法冲突下 (例如 cAsyncSocket.Create / cTlsSocket.Create / cPassword.Create
        // 6+ 个类共享 storageKey="create"), 命中首个注册者而非对象真实类的方法,
        // 导致 Optional 参数 _has_ 标志个数填错 → C2197 ("too many arguments").
        // 限制条件: 仅当 maExpr.object 是 IdentifierExpr 时启用 — 此时无副作用,
        // 不需要重复 emit obj 表达式即可推断 className. 复杂链式 obj 留给原回退路径.
        bool classAwareResolved = false;
        if (maExpr.object && maExpr.object->kind == ASTNodeKind::IdentifierExpr) {
            std::string className;
            auto& idObj = static_cast<IdentifierExpr&>(*maExpr.object);
            std::string nameLower = Symbol::toLower(idObj.name);
            if (nameLower == "me") {
                // 当前类模块实例 — className = 本模块名
                if (isClassModule_) className = moduleName_;
            } else {
                auto it = knownClassVars_.find(nameLower);
                if (it != knownClassVars_.end()) {
                    className = it->second;
                } else {
                    // 不是已知类变量 → 可能是模块名或外部类名自身
                    // (例: ToolsStr.HasStr — ToolsStr 是 .bas 模块; cWinsock.SomeStaticMethod — cWinsock 类)
                    // findClassMemberCallParams 内部会按 sourceModule/moduleName_ 匹配
                    // Fix 084z-3: 对象是当前模块的属性 (Property Get 返回类实例, 如
                    // cTlsRemaster.pvSocket → cTlsSocket) 时, inferClassTypeOfExpr
                    // 可推断出类名 → 优先按类解析方法形参; 否则 findClassMemberCallParams
                    // 按属性名查找失败, 回退 lookupModule 命中 storageKey 同名冲突的
                    // 错误类 (SyncReceiveArray 按 cWinsock 12 参展开 → C2197).
                    className = idObj.name;
                    // Fix 090g: 对象是当前函数名引用 (函数体内 FuncName.Add(...) —
                    // 即本函数返回对象, Fix 088c 已把 vb6_ret_FuncName 注册进
                    // knownClassVars_). 例 cToolsStr.SplitLinesToCollection As cCollection
                    // → SplitLinesToCollection.Add Mid(...). 此前 fallthrough 到模块名
                    // 路径, findClassMemberCallParams 失败后又回退 lookupModule(storageKey),
                    // 命中跨类同名方法抢占的错误类 (Add → 其它类的 Add), 形参表错 →
                    // C2197/C2440 (Item 按 ByRef String 打包, 多出 Optional int32).
                    if (currentProc_ && nameLower == Symbol::toLower(currentProc_->name)) {
                        std::string retVar090g = "vb6_ret_" + nameLower;
                        auto itRet090g = knownClassVars_.find(retVar090g);
                        if (itRet090g != knownClassVars_.end()) {
                            className = itRet090g->second;
                        }
                    }
                    if (className == idObj.name) {
                        std::string inferredClass = inferClassTypeOfExpr(*maExpr.object);
                        if (!inferredClass.empty()) {
                            className = inferredClass;
                        }
                    }
                }
            }
            if (!className.empty()) {
                std::vector<ParameterInfo> params;
                bool isBuiltin = false;
                if (findClassMemberCallParams(className, maExpr.memberName, params, isBuiltin)) {
                    calleeParams = std::move(params);
                    calleeIsBuiltin = isBuiltin;
                    classAwareResolved = true;
                    calleeParamsFound = true;
                }
            }
        }
        // Fix 042b: For method chains and complex object expressions (e.g.,
        // db.Table("t").OrderByDesc("id").Limit(10).Offset(20)), the object is
        // not a simple IdentifierExpr but an IndexOrCallExpr or MemberAccessExpr.
        // Use inferClassTypeOfExpr to determine the class type, then look up
        // member params. This enables Optional param padding for cross-module
        // method chain calls.
        if (!classAwareResolved && maExpr.object
            && maExpr.object->kind != ASTNodeKind::IdentifierExpr) {
            std::string className = inferClassTypeOfExpr(*maExpr.object);
            if (!className.empty()) {
                std::vector<ParameterInfo> params;
                bool isBuiltin = false;
                if (findClassMemberCallParams(className, maExpr.memberName, params, isBuiltin)) {
                    calleeParams = std::move(params);
                    calleeIsBuiltin = isBuiltin;
                    classAwareResolved = true;
                    calleeParamsFound = true;
                }
            }
        }

        // Fix 092z: 内置全局对象 (Clipboard/Screen/Printer/Forms/Debug/Err/App) 的成员
        // 由 visit(MemberAccessExpr) 的硬编码分支解析为 RTL 函数 (如 Clipboard.SetText
        // → vb6_Clipboard_SetText), VB 侧**不存在**形参表. 若此处仍用
        // lookupModule(memberName) 兜底, 会命中项目中同名的类成员:
        //   cQRcode.cls "Public Function SetText(ByVal Content As Variant) As cQRcode"
        // 使 FLogs.frm "Clipboard.SetText Text1.Text" 的形参表被误当成 ByVal Variant
        // → 实参被 Fix 086 / Fix 024P2 包装成 vb6_VariantFromValue(...) →
        // vb6_Clipboard_SetText(vb6_VARIANT) C2440 (vb6_VARIANT→BSTR).
        // 这几个名字是 VB6 保留的全局对象, 不可能被用户标识符占用, 直接跳过兜底.
        bool builtinGlobalObj092z = false;
        if (maExpr.object && maExpr.object->kind == ASTNodeKind::IdentifierExpr) {
            static const std::unordered_set<std::string> builtinGlobalObjs092z = {
                "clipboard", "screen", "printer", "forms", "debug", "err", "app"};
            builtinGlobalObj092z = builtinGlobalObjs092z.count(
                Symbol::toLower(static_cast<IdentifierExpr&>(*maExpr.object).name)) > 0;
        }

        // Fix 033 回退: 类感知未命中 (对象为链式表达式 / className 找不到方法符号 /
        // 需要匹配 DeclareSub/DeclareFunc 等) → 用原 class-unaware lookupModule 兜底,
        // 保留旧行为兼容性.
        if (!classAwareResolved && !builtinGlobalObj092z) {
            Symbol* funcSym = symTab_.lookupModule(maExpr.memberName);
            // Fix 084y-7: VBA 内置函数 (VBA.Replace / VBA.Mid$ / VBA.Val 等) 注册在
            // 全局内置符号表, 不在模块作用域 — lookupModule 必然失败. 回退全表
            // lookup (去 $ 后缀: Mid$ → Mid), 拿到含 Optional 的完整形参表,
            // 才能填充默认参数 (vb6_Replace 声明6参, VB6 调用只传3参).
            if (!funcSym) {
                std::string fnName = maExpr.memberName;
                if (!fnName.empty() && fnName.back() == '$') fnName.pop_back();
                funcSym = symTab_.lookup(fnName);
            }
            if (funcSym && (funcSym->kind == SymbolKind::Sub || funcSym->kind == SymbolKind::Function
                || funcSym->kind == SymbolKind::PropertyGet || funcSym->kind == SymbolKind::PropertyLet
                || funcSym->kind == SymbolKind::PropertySet
                || funcSym->kind == SymbolKind::DeclareSub || funcSym->kind == SymbolKind::DeclareFunc)) {
                calleeParams = funcSym->params;
                calleeIsBuiltin = funcSym->isBuiltin;
                calleeParamsFound = true;
                // Fix 042a: Track Declare functions for _has_ flag suppression
                if (funcSym->kind == SymbolKind::DeclareSub || funcSym->kind == SymbolKind::DeclareFunc) {
                    calleeIsDeclare = true;
                }
                // Bug #3 fix: 检测跨模块函数返回类型为StdPicture/IPictureDisp
                if (!funcSym->variableTypeName.empty()) {
                    std::string vtLower = funcSym->variableTypeName;
                    std::transform(vtLower.begin(), vtLower.end(), vtLower.begin(), ::tolower);
                    if (vtLower == "stdpicture" || vtLower == "ipicturedisp" || vtLower == "ipicture") {
                        lastExprIsComPicture_ = true;
                    }
                }
            }
        }
    }

    // Fix 041b: WithMemberExpr callee — With-block member call (e.g., .Add(...) inside With)
    // needs calleeParams for Optional param padding and IsMissing _has_ flags. Without this,
    // cross-module With-block calls with Optional params generate C2198 (too few arguments).
    if (node.callee && node.callee->kind == ASTNodeKind::WithMemberExpr) {
        auto& wmExpr = static_cast<WithMemberExpr&>(*node.callee);
        if (!withObjectInfoStack_.empty()) {
            const auto& info = withObjectInfoStack_.back();
            if (info.kind == WithObjKind::ClassInstance && !info.className.empty()) {
                std::vector<ParameterInfo> params;
                bool isBuiltin = false;
                if (findClassMemberCallParams(info.className, wmExpr.memberName, params, isBuiltin)) {
                    calleeParams = std::move(params);
                    calleeIsBuiltin = isBuiltin;
                    calleeParamsFound = true;
                }
            }
        }
    }

    // M22: 检测Declare ANSI函数调用 - 需要BSTR->ANSI转换
    bool isDeclareAnsiCall = false;
    if (node.callee && node.callee->kind == ASTNodeKind::IdentifierExpr) {
        auto& idExpr = static_cast<IdentifierExpr&>(*node.callee);
        std::string funcLower = idExpr.name;
        std::transform(funcLower.begin(), funcLower.end(), funcLower.begin(), ::tolower);
        if (knownDeclareAnsi_.count(funcLower)) isDeclareAnsiCall = true;
    }

    std::vector<std::string> args;
    for (size_t i = 0; i < node.positional.size(); i++) {
        // Fix 090q: 实参是「返回 UDT 的函数调用」→ Declare As Any ByRef 打包须传
        // 临时 UDT 地址 (void*)&(vb6_type_X){...}; 否则强转 struct 值 → C2440
        // (cZipArchive pvVfsSetEof: SetFileTime ..., pvToFileTime(...) As Any)
        std::string argUdtRetCType090q;
        if (node.positional[i] && node.positional[i]->kind == ASTNodeKind::IndexOrCallExpr) {
            auto& nc090q = static_cast<IndexOrCallExpr&>(*node.positional[i]);
            if (nc090q.callee && nc090q.callee->kind == ASTNodeKind::IdentifierExpr) {
                auto& nid090q = static_cast<IdentifierExpr&>(*nc090q.callee);
                auto it090q = funcUdtRetCType_.find(Symbol::toLower(nid090q.name));
                if (it090q != funcUdtRetCType_.end()) argUdtRetCType090q = it090q->second;
            }
        }
        emitExpr(*node.positional[i]);
      // COM属性标记残留: MsgBox dic.Count 等场景 — 参数是COM属性读取但标记未被消费
        // 统一用后期绑定(IDispatch), 避免vtable签名不匹配问题
        if (isComMarker_) {
            isComMarker_ = false;
            std::string objExpr = std::move(comObjExpr_);
            std::string memName = std::move(comMemberName_);
            if (isEarlyBoundCom_ && earlyBoundSym_) {
                isEarlyBoundCom_ = false;
                const Symbol* comSym = earlyBoundSym_;
                earlyBoundSym_ = nullptr;
                std::string memLower = memName;
                std::transform(memLower.begin(), memLower.end(), memLower.begin(), ::tolower);
                auto it = comSym->comMethods.find(memLower);
                if (it != comSym->comMethods.end() && it->second.isPropertyGet) {
                    const auto& sig = it->second;
                    std::string returnType = mapType(sig.returnType);
                    if (returnType == "int32_t" || returnType == "int16_t") {
                        lastExpr_ = "vb6_ComGetIntProp(" + objExpr + ", L\"" + memName + "\")";
                    } else if (returnType == "BSTR") {
                        lastExpr_ = "vb6_ComGetStringProp(" + objExpr + ", L\"" + memName + "\")";
                    } else if (returnType == "double" || returnType == "float") {
                        lastExpr_ = "vb6_ComGetDoubleProp(" + objExpr + ", L\"" + memName + "\")";
                    } else if (returnType == "void*") {
                        lastExpr_ = "vb6_ComGetObjectProp(" + objExpr + ", L\"" + memName + "\")";
                    } else {
                        lastExpr_ = "vb6_VariantFromComResult(vb6_ComGetProp(" + objExpr + ", L\"" + memName + "\"))";
                    }
                } else {
                    lastExpr_ = "vb6_VariantFromComResult(vb6_ComGetProp(" + objExpr + ", L\"" + memName + "\"))";
                }
            } else {
                lastExpr_ = "vb6_VariantFromComResult(vb6_ComGetProp(" + objExpr + ", L\"" + memName + "\"))";  /* P25: late-bound VARIANT */
            }
        }
        std::string argVal = std::move(lastExpr_);

        // M22: Declare ANSI函数 - ByVal String参数需要BSTR->ANSI转换
        // 生成临时char*变量, 调用后释放, 无内存泄露
        if (isDeclareAnsiCall && i < calleeParams.size() && calleeParams[i].isByVal
            && calleeParams[i].type == Vb6Type::String) {
            std::string ansiVar = "_ansi_" + std::to_string(ansiCounter_++);
            c_.emitLine("char* " + ansiVar + " = vb6_BSTR_ToANSI(" + argVal + ");");
            ansiTempsToFree_.push_back(ansiVar);
            argVal = ansiVar;
        }

        // ByRef参数: 调用点传指针. 如果实参已经是解引用形式(*x), 取地址还原为x;
        // 如果是普通变量, 加&取地址
        // Fix 072: VB6 允许 ByVal 覆盖 ByRef 声明 (如 SHCreateMemStream(ByVal 0, 0))
        bool argHasByValOverride = node.byvalOverrides.count(i) > 0;
        // Fix 078 rev2: ByRef array parameters are now vb6_SafeArray1D**,
        // so they DO need & at the call site — same as other ByRef params.
        // Removed isArrayParamType exclusion so arrays get & just like non-arrays.
        bool isByRef = (i < calleeParams.size() && !calleeParams[i].isByVal && !calleeParams[i].isParamArray)
                       && !argHasByValOverride;
        bool calleeParamIsArray = (i < calleeParams.size()
            && (static_cast<uint16_t>(calleeParams[i].type) & static_cast<uint16_t>(Vb6Type::Array)));
        if (isByRef) {
            if (argVal.size() > 3 && argVal.substr(0, 2) == "(*" && argVal.back() == ')') {
                // (*x) → depends on callee param type:
                // If callee param is array (vb6_SafeArray1D**), arg (*x) means x is already vb6_SafeArray1D**,
                // so pass x directly (not &x which would be ***).
                // If callee param is non-array, (*x) → depends on whether x is a ByRef param of current function:
                //   - If x is a current-function ByRef param (C type T*), then (*x) is T, and callee expects T*.
                //     Since x itself is already T*, pass x directly (not &x which would be T**).
                //   - Otherwise (e.g. local variable holding a pointer), (*x) → &x to restore the pointer.
                std::string innerName = argVal.substr(2, argVal.size() - 3);
                if (calleeParamIsArray) {
                    argVal = innerName;
                } else {
                    // Fix 079: check if innerName is a ByRef param of the current function
                    bool isCurrentByRefParam = false;
                    if (currentProc_) {
                        for (auto& p : currentProc_->params) {
                            if (Symbol::toLower(p.name) == Symbol::toLower(innerName) && !p.isByVal && !p.isParamArray) {
                                bool paramIsArray = (static_cast<uint16_t>(p.type) & static_cast<uint16_t>(Vb6Type::Array)) != 0;
                                if (!paramIsArray) {
                                    isCurrentByRefParam = true;
                                }
                                break;
                            }
                        }
                    }
                    if (isCurrentByRefParam) {
                        // ByRef param of current function → already T*, pass directly
                        argVal = innerName;
                    } else {
                        // Local pointer variable → restore pointer with &
                        argVal = "&" + innerName;
                    }
                }
            } else if (argVal.size() > 2 && argVal.substr(0, 2) == "me" && argVal[2] == '-') {
                // me->field → &(me->field) (类成员字段取地址)
                argVal = "&(" + argVal + ")";
            } else {
                // Fix 064: As Any (Vb6Type::Unknown) ByRef 参数 — 传 void* 指针,
                // 不包装为 VARIANT. VB6 中 As Any 表示"任意类型指针",
                // 对数组元素应传 &(VB6_SA_AT(...)) 即首元素地址.
                // UDT ByRef 参数同理 — C 函数签名已是 UDT*, 传 &argVal 即可.
                bool isAsAnyParam = (i < calleeParams.size() && calleeParams[i].type == Vb6Type::Unknown);
                bool isUdtByRefParam = (i < calleeParams.size() && calleeParams[i].type == Vb6Type::UserDefinedType);

                // Fix 064b: VB6_SA_AT(...) 数组元素作为 ByRef 实参时,
                // 不应包装为 VARIANT 复合字面量, 应直接取地址.
                // 典型: PolyPolygon(hDC, uPoints(0), aSizes(0), nCount)
                //   uPoints(0) → VB6_SA_AT(vb6_type_POINTAPI, uPoints, 0)
                //   应生成 (void*)&(VB6_SA_AT(...)) 而非 (&(vb6_VARIANT){.bstrVal=...})
                bool isSaAtExpr = (argVal.find("VB6_SA_AT(") == 0);
                if (isSaAtExpr && !isAsAnyParam && !isUdtByRefParam) {
                    // 数组元素作为 ByRef 参数: 当 calleeParams 缺失或类型不匹配时,
                    // 对 Declare 函数的 As Any 参数一律传 void*
                    if (calleeIsDeclare || i >= calleeParams.size()) {
                        isAsAnyParam = true;
                    }
                }

                if (isAsAnyParam || isUdtByRefParam) {
                    // As Any ByRef / UDT ByRef: 左值取地址, 非左值(字面量等)强转为void*
                    // Fix 069b: 对字面量(如 0)不能取地址 &(0) → C2101,
                    //   应直接强转 (void*)(intptr_t)(0).
                    bool isSimpleIdent = !argVal.empty() && (std::isalpha(static_cast<unsigned char>(argVal[0])) || argVal[0] == '_');
                    if (isSimpleIdent) {
                        for (char c : argVal) {
                            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
                                isSimpleIdent = false;
                                break;
                            }
                        }
                    }
                    // Fix 056b: UDT字段链 (uFile.Data.ftLastWriteTime) 也是左值,
                    // 作为 ByRef UDT/AsAny 实参应取地址 &(x) 而非强转 (void*)(intptr_t)(x)
                    // Fix 084m: 字段链须允许 -> — With变量展开 (._vb6_with_2->SendBuffer)
                    // 也是左值字段链, 若判为非左值会生成 (void*)(intptr_t)(udt) → C2440
                    bool isUdtFieldChain = !argVal.empty() && (std::isalpha(static_cast<unsigned char>(argVal[0])) || argVal[0] == '_');
                    if (isUdtFieldChain) {
                        for (size_t ci = 0; ci < argVal.size(); ci++) {
                            char c = argVal[ci];
                            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.') continue;
                            if (c == '-' && ci + 1 < argVal.size() && argVal[ci + 1] == '>') { ci++; continue; }
                            isUdtFieldChain = false;
                            break;
                        }
                    }
                    bool isSaAt = (argVal.find("VB6_SA_AT(") == 0);
                    // Fix 086: 宏常量/NULL 不是左值 — TLS_LOCAL_LEGACY_VERSION 是
                    // #define, &(宏) → C2101; NULL 同理.
                    bool isConstMacro = (argVal == "NULL") || isConstIdent(argVal);
                    // Fix 090k: (*uFile).BufferArray / (*uFile)->field — ByRef UDT
                    // 参数解引用的字段链也是左值 (As Any / CopyMemory 实参应取地址
                    // &(field), 而非下方 else 的 (void*)(intptr_t)(expr) 强转 —
                    // Variant 字段值强转指针 → C2440 "无法从 vb6_VARIANT 转 intptr_t").
                    bool isDerefFieldChain = false;
                    if (!argVal.empty() && argVal[0] == '(' && argVal.size() > 2 && argVal[1] == '*') {
                        size_t depth090k = 0;
                        for (size_t ci090k = 0; ci090k < argVal.size(); ci090k++) {
                            char c090k = argVal[ci090k];
                            if (c090k == '(') depth090k++;
                            else if (c090k == ')') {
                                depth090k--;
                                if (depth090k == 0) {
                                    if (ci090k + 1 < argVal.size() &&
                                        (argVal[ci090k + 1] == '.' ||
                                         (argVal[ci090k + 1] == '-' && ci090k + 2 < argVal.size()
                                          && argVal[ci090k + 2] == '>'))) {
                                        isDerefFieldChain = true;
                                    }
                                    break;
                                }
                            }
                        }
                    }
                    bool isLValue = (isSimpleIdent || isUdtFieldChain || isSaAt || argVal.find("me->") == 0
                        || (argVal.size() > 4 && argVal[0] == '(' && argVal[1] == '*' && argVal.back() == ')')
                        || isDerefFieldChain)
                        && !isConstMacro;
                    if (isLValue) {
                        // 左值: 变量名、数组元素、me->field、(*ptr) 解引用 — 可以取地址
                        argVal = "(void*)&(" + argVal + ")";
                    } else if (!argUdtRetCType090q.empty()) {
                        // Fix 090q: 实参是返回 UDT 的函数调用 — VB6 把返回值拷入临时
                        // UDT 再传址 (As Any ByRef). C 不能直接强转 struct 值
                        // ((void*)(intptr_t)(struct) → C2440), 也不能用复合字面量
                        // 内联初始化 ((T){fnRetUdt()} MSVC C 报 C2440 "初始化…无法从
                        // T 转换") — 只能先求值到已声明临时再取址. 声明行挂 pending,
                        // 在包含本实参的语句行输出前落地 (CodeEmitter::flushPending),
                        // 保证 "声明先于引用". 例: cZipArchive pvVfsSetEof 内
                        // SetFileTime(..., pvToFileTime(dLastWriteTime)) (FILETIME).
                        std::string tmpAny090q = "_vb6_anytmp" + std::to_string(tempCounter_++);
                        c_.addPending(argUdtRetCType090q + " " + tmpAny090q
                                      + " = " + argVal + ";");
                        argVal = "(void*)&" + tmpAny090q;
                    } else if (isConstMacro && argVal != "NULL") {
                        // Fix 086: 宏常量 → 用对应类型的复合字面量承载地址
                        std::string ccType = mapType(constIdentType(argVal));
                        if (ccType.empty() || ccType == "vb6_VARIANT") ccType = "int32_t";
                        argVal = "(void*)&(" + ccType + "){" + argVal + "}";
                    } else {
                        // 非左值: 字面量或复杂表达式 — 直接强转为 void*
                        argVal = "(void*)(intptr_t)(" + argVal + ")";
                    }
                } else {
                // Fix 072b: ByVal 覆盖 As Any 参数 — 传 (void*)(intptr_t)(val),
                // 不走复合字面量路径. 典型: SHCreateMemStream(ByVal 0, 0) 中
                // pInit As Any 是 ByRef, 但 ByVal 0 覆盖为传值, 应生成 (void*)0.
                bool isAsAnyByVal = (i < calleeParams.size() && calleeParams[i].type == Vb6Type::Unknown)
                                    && argHasByValOverride;
                if (isAsAnyByVal) {
                    argVal = "(void*)(intptr_t)(" + argVal + ")";
                } else {
                // 变量/非左值 → 复合字面量取地址; 变量 → &变量
                // 检查是否是简单标识符 (变量名, 以字母/下划线开头)
                bool isSimpleIdent = !argVal.empty() && (std::isalpha(static_cast<unsigned char>(argVal[0])) || argVal[0] == '_');
                if (isSimpleIdent) {
                    for (char c : argVal) {
                        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
                            isSimpleIdent = false;
                            break;
                        }
                    }
                }
                if (isSimpleIdent && !argVal.empty()) {
                    // Fix 079: If the simple identifier is a ByRef param of the current function,
                    // it's already a pointer (T*) — pass directly, don't add &.
                    bool argIsCurrentByRefParam = false;
                    if (currentProc_) {
                        for (auto& p : currentProc_->params) {
                            if (Symbol::toLower(p.name) == Symbol::toLower(argVal) && !p.isByVal && !p.isParamArray) {
                                argIsCurrentByRefParam = true;
                                break;
                            }
                        }
                    }
                    if (argIsCurrentByRefParam) {
                        // ByRef param of current function → already T*, pass directly
                    } else if (isConstIdent(argVal)) {
                        // Fix 084aa: 常量是 #define 宏 (vb6_BSTR_FromStr(...) 或数值字面量),
                        // 取址 &ERR_X 展开为函数结果取址 → C2102. 按形参类型复合字面量包装.
                        Vb6Type pType = Vb6Type::Variant;
                        if (i < calleeParams.size()) pType = calleeParams[i].type;
                        argVal = wrapConstArgForByRef(argVal, pType);
                    } else if (argVal == "NULL" || argVal == "nullptr") {
                        // Fix 092l: NULL/nullptr 字面量不可取址 — &NULL 触发
                        // C2101 "常量上的 &" (cTlsSocket 440:
                        //   FireOnCertificate(me, &NULL) → 应为 (&(vb6_VARIANT){0})).
                        // 按形参类型改用 C11 复合字面量.
                        std::string ct092l = "vb6_VARIANT";
                        if (i < calleeParams.size()) ct092l = mapType(calleeParams[i].type);
                        argVal = "(&(" + ct092l + "){0})";
                    } else if (i < calleeParams.size()
                               && !calleeParams[i].isByVal && !calleeParams[i].isParamArray
                               && calleeParams[i].type == Vb6Type::Variant) {
                        // Fix 092y: ByRef Variant 形参 + 简单标识符实参.
                        // C 签名为 vb6_VARIANT*, 直接 &(BSTR) 会把字符串当 Variant
                        // 解析 → 字段错位 → lDataLen=0、QR 段为空 (mdQRTest2_url).
                        // 与 2039 M22 分支一致: 按实参 VB 类型构造字段式复合字面量.
                        Vb6Type argVbType92y = inferExprType(*node.positional[i]);
                        switch (argVbType92y) {
                            case Vb6Type::String:
                                argVal = "(&(vb6_VARIANT){.vt=VT_BSTR, .bstrVal=" + argVal + "})";
                                break;
                            case Vb6Type::Long:
                            case Vb6Type::Integer:
                                argVal = "(&(vb6_VARIANT){.vt=VT_I4, .lVal=(int32_t)(" + argVal + ")})";
                                break;
                            case Vb6Type::Double:
                            case Vb6Type::Single:
                                argVal = "(&(vb6_VARIANT){.vt=VT_R8, .dblVal=(double)(" + argVal + ")})";
                                break;
                            case Vb6Type::Boolean:
                                argVal = "(&(vb6_VARIANT){.vt=VT_BOOL, .boolVal=(int16_t)(" + argVal + ")})";
                                break;
                            case Vb6Type::Byte:
                                argVal = "(&(vb6_VARIANT){.vt=VT_UI1, .bVal=(uint8_t)(" + argVal + ")})";
                                break;
                            case Vb6Type::Date:
                                argVal = "(&(vb6_VARIANT){.vt=VT_DATE, .dblVal=(double)(" + argVal + ")})";
                                break;
                            default:
                                // Variant 变量 / UDT 字段 / 未知类型: 保持原样取址,
                                // 值已是 vb6_VARIANT 且可寻址 &x 语义正确.
                                argVal = "&" + argVal;
                                break;
                        }
                    } else {
                        argVal = "&" + argVal;
                    }
                } else {
                    // Fix 092z: 左值字段链 (With对象 _vb6_with_N->字段 / me->字段 /
                    // 虚UDT字段) 也是可寻址左值 → 生成 &(字段) 而非复合字面量.
                    // With前 ByRef Long 字段 (QRCodegenMakeAlphanumeric →
                    // pvAppendBitsToBuffer ..., .Data, .BitLength) 若走复合字面量
                    // 会复制字段值到临时 int32, 被调方 (*lBitLen)++ 只改临时值 →
                    // 字段永不增长 → 位写入偏移全零 (HELLO WORLD 无法解码).
                    bool isLvChain092z = !argVal.empty() && (std::isalpha(static_cast<unsigned char>(argVal[0])) || argVal[0] == '_');
                    if (!isLvChain092z && argVal.size() > 2 && argVal[0] == '(' && argVal[1] == '*') {
                        // Fix 090k: (*ptr)->field / (*ptr).field 解引用字段链也是左值
                        int depth092z = 0;
                        for (size_t ci092z = 0; ci092z < argVal.size(); ci092z++) {
                            char c092z = argVal[ci092z];
                            if (c092z == '(') depth092z++;
                            else if (c092z == ')') {
                                depth092z--;
                                if (depth092z == 0) {
                                    if (ci092z + 1 < argVal.size() &&
                                        (argVal[ci092z + 1] == '.' ||
                                         (argVal[ci092z + 1] == '-' && ci092z + 2 < argVal.size()
                                          && argVal[ci092z + 2] == '>'))) {
                                        isLvChain092z = true;
                                    }
                                    break;
                                }
                            }
                        }
                    }
                    if (isLvChain092z) {
                        for (size_t ci092z = 0; ci092z < argVal.size(); ci092z++) {
                            char c092z = argVal[ci092z];
                            if (std::isalnum(static_cast<unsigned char>(c092z)) || c092z == '_' || c092z == '.') continue;
                            if (c092z == '-' && ci092z + 1 < argVal.size() && argVal[ci092z + 1] == '>') { ci092z++; continue; }
                            isLvChain092z = false;
                            break;
                        }
                    }
                    if (isLvChain092z) {
                        argVal = "&(" + argVal + ")";
                    } else {
                    // 字面量或复杂表达式: 使用C11复合字面量
                    // &(int32_t){10} 或 &(double){3.14}
                    std::string cType = "int32_t";
                    if (i < calleeParams.size()) {
                        cType = mapType(calleeParams[i].type);
                        // Fix 092e: ByRef 数组形参 (如 cToolsUtf8.Decode(ByRef
                        // Utf() As Byte)) 的 C 形参是 vb6_SafeArray1D** (Fix 078
                        // rev2), 复合字面量类型必须一致 — mapType(Byte|Array)
                        // 只给出 "uint8_t*" → (&(uint8_t*){Variant}) C2440
                        // "无法从 vb6_VARIANT 转换为 uint8_t *"
                        // (cHttpClient 418: ToolsUtf8.Decode(Inst.ResponseBody)).
                        bool isArrP092e = (static_cast<uint16_t>(calleeParams[i].type)
                                           & static_cast<uint16_t>(Vb6Type::Array)) != 0;
                        if (isArrP092e && !calleeParams[i].isByVal
                            && !calleeParams[i].isParamArray) {
                            cType = "vb6_SafeArray1D**";
                        }
                    }
                    // M22: VARIANT类型需要指定字段初始化 (.vt=VT_xxx, .field=value)
                    if (cType == "vb6_VARIANT") {
                        // 推断实参的VB6类型来决定VARIANT字段
                        Vb6Type argVbType = inferExprType(*node.positional[i]);
                        switch (argVbType) {
                            case Vb6Type::String:
                                // Fix 027b: 外层加括号让预处理器把 {a,b} 内的逗号视为同一参数
                                // (避免宏调用时 #define RtlCopyMemory(D,s,l) memcpy(D,s,l) 把复合字面量
                                //  内部的逗号错算成宏实参分隔符 → C4002 参数过多)
                                argVal = "(&(vb6_VARIANT){.vt=VT_BSTR, .bstrVal=" + argVal + "})";
                                break;
                            case Vb6Type::Long:
                            case Vb6Type::Integer:
                                argVal = "(&(vb6_VARIANT){.vt=VT_I4, .lVal=(int32_t)(" + argVal + ")})";
                                break;
                            case Vb6Type::Double:
                            case Vb6Type::Single:
                                argVal = "(&(vb6_VARIANT){.vt=VT_R8, .dblVal=(double)(" + argVal + ")})";
                                break;
                            case Vb6Type::Boolean:
                                argVal = "(&(vb6_VARIANT){.vt=VT_BOOL, .boolVal=(int16_t)(" + argVal + ")})";
                                break;
                            case Vb6Type::Byte:
                                argVal = "(&(vb6_VARIANT){.vt=VT_UI1, .bVal=(uint8_t)(" + argVal + ")})";
                                break;
                            case Vb6Type::Date:
                                argVal = "(&(vb6_VARIANT){.vt=VT_DATE, .dblVal=(double)(" + argVal + ")})";
                                break;
                            default:
                                // Fix 054: Variant 或未知类型
                                // 1. 实参是 UDT 的 Variant 字段 (With 成员如 .SourceFile,
                                //    .FileName 等 As Variant) → 值已是 vb6_VARIANT,
                                //    ByRef Variant 形参直接取地址 &(...), 不要包装成
                                //    {.vt=VT_BSTR, .bstrVal=...} (Variant→BSTR C2440).
                                // 2. C 表达式已是 Variant (vb6_VariantArrayGet 等),
                                //    提取 BSTR 值到 .bstrVal (避免 VARIANT→BSTR C2440)
                                // 3. 其他未知类型保持原样 (UDT 等单独处理)
                                if (i < node.positional.size()
                                    && inferUdtFieldVb6Type(node.positional[i].get()) == Vb6Type::Variant) {
                                    argVal = "&(" + argVal + ")";
                                } else if (cExprIsVariant(argVal)) {
                                    argVal = "(&(vb6_VARIANT){.vt=VT_BSTR, .bstrVal=vb6_VariantToString(" + argVal + ")})";
                                } else {
                                    argVal = "(&(vb6_VARIANT){.vt=VT_BSTR, .bstrVal=" + argVal + "})";
                                }
                                break;
                        }
                    } else {
                        // Fix 090d: ByRef 具体类型形参 + 实参是 vb6_VARIANT 值 —
                        // 复合字面量初始化 (&(double){vb6_VARIANT}) 触发 C2440
                        // (vb6_VARIANT→double/BSTR/... 初始化失败). 按 cType 先提取.
                        // 例: cHttpServerCookies Encode: FormatHttpDate(CK.Expires)
                        //     (形参 ByRef Date, Expires As Variant 属性 →
                        //     vb6_cHttpServerCookieAttr_prop_get_Expires 返回 vb6_VARIANT);
                        //     cHttpServerResponse File: ComputeETag(fileInfo.DateLastModified,
                        //     ...) (COM 属性 → vb6_VariantFromComResult(...) vb6_VARIANT).
                        bool argIsVariant090d = isDefinitelyVariantExpr(*node.positional[i]);
                        if (!argIsVariant090d) argIsVariant090d = cExprIsVariant(argVal);
                        if (argIsVariant090d) {
                            if (cType == "double" || cType == "float") {
                                argVal = "vb6_VariantToDouble(" + argVal + ")";
                            } else if (cType == "BSTR") {
                                argVal = "vb6_VariantToString(" + argVal + ")";
                            } else if (cType == "int32_t" || cType == "int16_t"
                                       || cType == "uint8_t" || cType == "int64_t"
                                       || cType == "LONG") {
                                argVal = "vb6_VariantToLong(" + argVal + ")";
                            } else if (cType == "vb6_SafeArray1D*"
                                       || cType == "vb6_SafeArray1D**") {
                                argVal = "vb6_VariantToSafeArray1D(" + argVal + ")";
                            } else if (cType == "void*") {
                                argVal = "vb6_VariantToObjectVal(" + argVal + ")";
                            }
                        }
                        // Fix 027b: 同样加外层括号防止预处理器把复合字面量 `{a, b}` 内的逗号算成宏实参分隔符.
                        argVal = "(&(" + cType + "){" + argVal + "})";
                    }
                    }
                }
                } // end Fix 072b else (As Any ByVal override)
                    } // end Fix 064 else
            }
        } // Fix 024 P2: ByVal Variant 参数 — 调用点用 vb6_VariantFromValue 包装实参.
        // 处理 callee 声明 "ByVal x As Variant" 而实参是标量/BSTR/SafeArray/class ptr 等情形.
        // _Generic 在编译期按实类型选择 ctor: 标量->VariantLong/Int/Double, BSTR->String,
        // SafeArray1D*->Array, void*/class ptr->Object, vb6_VARIANT->Identity(no-op).
        // 仅对 ByVal Variant 生效 (ByRef Variant 走上面复合字面量路径, 取地址需左值).
        if (!isByRef && i < calleeParams.size() && calleeParams[i].isByVal
            && calleeParams[i].type == Vb6Type::Variant) {
            // Fix 084d: 实参若是类对象表达式 (如 me->Database, 类方法返回类对象),
            // 保持对象指针直传, 不要套 vb6_VariantFromValue — 符号表可能把类类型
            // 参数误记为 Variant, 若包装成 vb6_VARIANT 传给 vb6_cls_cXxx* 参数
            // 会触发 C2440 (如 cHttpServer.c LoadFromDatabase(..., me->Database)).
            bool argIsClassObj = false;
            if (i < node.positional.size()) {
                std::string acName091s = inferClassTypeOfExpr(*node.positional[i]);
                if (!acName091s.empty()) {
                    // Fix 091s: 只有"项目类"(SymbolKind::Class) 实参才跳过打包
                    // (084d 的类指针直传语义). Object/COM 类实参 (void*) 传给
                    // ByVal Variant 形参时仍需 vb6_VariantFromValue 包装 —
                    // Demo 857/859: Debug.Print ToolsJsonVba.ConvertToJson(Json)
                    // (Json As Object; ConvertToJson 第 1 参 ByVal JsonValue As
                    // Variant) 原样传 void* → C2440 (void* → vb6_VARIANT).
                    auto* acSym091s = symTab_.lookup(acName091s);
                    argIsClassObj = (acSym091s && acSym091s->kind == SymbolKind::Class);
                }
            }
            if (!argIsClassObj) {
                argVal = "vb6_VariantFromValue(" + argVal + ")";
            } else if (i < node.positional.size()
                       && node.positional[i]->kind == ASTNodeKind::IdentifierExpr) {
                // Fix 091s2: 实参是 Object/COM 变量 (void*) 而形参是 ByVal Variant
                // → 仍需打包 (inferClassTypeOfExpr 对 Object 变量可能返回内建
                // "Object" 类名而误判为项目类实参). Demo 857/859:
                // ToolsJsonVba.ConvertToJson(Json) (Json As Object).
                std::string on091s2 = Symbol::toLower(
                    static_cast<IdentifierExpr&>(*node.positional[i]).name);
                if (knownObjectVars_.count(on091s2) || knownTypedComVars_.count(on091s2)) {
                    argVal = "vb6_VariantFromValue(" + argVal + ")";
                }
            }
        }
        // Fix 029: 反向强制 — ByVal 具体类型参数 + 实参确定为 Variant: 自动调用提取函数.
        // 与 Fix 024 P2 互补: P2 处理 V(callee)=Variant,V(arg)=scalar; 此处处理
        // V(callee)=concrete,V(arg)=Variant. 覆盖 C2440 子类 to_int32/to_BSTR/to_SafeArray.
        // 严格判断: 用 isDefinitelyVariantExpr 避免 inferExprType 默认回退到 Variant
        // 造成对内置函数 (LenB 等) / UDT 字段访问 (uAddr.sin_addr) 的错误包装.
        if (!isByRef && i < calleeParams.size() && calleeParams[i].isByVal
            && calleeParams[i].type != Vb6Type::Variant) {
            // 处理 Array 标志位 (例如 Variant() 参数对应 Vb6Type::Variant|Array)
            bool paramIsArray = (static_cast<uint16_t>(calleeParams[i].type)
                                  & static_cast<uint16_t>(Vb6Type::Array)) != 0;
            Vb6Type paramBase = static_cast<Vb6Type>(
                static_cast<uint16_t>(calleeParams[i].type)
                & ~static_cast<uint16_t>(Vb6Type::Array));
            bool argIsVariantArr = false;
            bool argIsVariant = isDefinitelyVariantExpr(*node.positional[i], &argIsVariantArr);
            // Fix 038b-2: 字符串级 Variant 检测回退 — 补充 isDefinitelyVariantExpr
            // 无法识别的 C 级 Variant 表达式 (vb6_VariantArrayGet, vb6_VariantFromComResult 等)
            if (!argIsVariant && !argIsVariantArr) {
                argIsVariant = cExprIsVariant(argVal);
            }
            if (argIsVariant || argIsVariantArr) {
                // Fix 090c1: 实参顶层是 COM 结果 (void* 承载 Variant) 且判为
                // Variant — 提取前先 vb6_VariantFromComResult (同 090c0).
                // cLogs MakeLogContent: Join(CacheDatas(i), Span) 的 CacheDatas(i)
                // = vb6_ComCall(...) → 原生成 VariantToSafeArray1D(ComCall) C2440.
                bool isComResultVal2 =
                    (argVal.find("vb6_ComCall(") == 0)
                    || (argVal.find("vb6_ComGetProp(") == 0)
                    || (argVal.find("vb6_ComGetObjectProp(") == 0);
                if (isComResultVal2) {
                    argVal = "vb6_VariantFromComResult(" + argVal + ")";
                }
                // Fix 091k: 实参是当前过程的 ParamArray 参数时, C 侧已是 SAFEARRAY*
                // (cgen 参数映射), 不能再按 Variant 数组提取 SafeArray1D* — 否则
                // UBound(ParamArray) 生成 vb6_PA_UBound(vb6_VariantToSafeArray1D(OutVars))
                // C2440 (cToolsArray DeArray).
                bool argIsPA091k = false;
                if (i < node.positional.size()
                    && node.positional[i]->kind == ASTNodeKind::IdentifierExpr && currentProc_) {
                    std::string paLower091k = Symbol::toLower(
                        static_cast<IdentifierExpr&>(*node.positional[i]).name);
                    for (auto& p091k : currentProc_->params) {
                        if (Symbol::toLower(p091k.name) == paLower091k) {
                            argIsPA091k = p091k.isParamArray;
                            break;
                        }
                    }
                }
                // Fix 092g: 实参是字面量数组 (Array(...) → 匿名 _arr_N) — C 侧
                // 本身已是 vb6_SafeArray1D* (由 vb6_ArraySetBSTR 逐个填充),
                // 不能再按 Variant 数组提取 (提取函数形参是 vb6_VARIANT) → C2440:
                //   cPLI 56: Cmd = Join(Array(a, b, c), " ")
                //     → vb6_Join(vb6_VariantToSafeArray1D(_arr_0), ...).
                bool argIsLiteralArr092g = (argVal.compare(0, 5, "_arr_") == 0);
                if (paramIsArray && !argIsPA091k && !argIsLiteralArr092g
                    && (paramBase == Vb6Type::Variant || paramBase == Vb6Type::Byte
                    || paramBase == Vb6Type::String || paramBase == Vb6Type::Long)) {
                    // 数组参数: 从 Variant 提取 SafeArray1D*
                    argVal = "vb6_VariantToSafeArray1D(" + argVal + ")";
                } else if (paramBase == Vb6Type::Long || paramBase == Vb6Type::Integer
                           || paramBase == Vb6Type::Byte || paramBase == Vb6Type::Boolean
                           || paramBase == Vb6Type::LongPtr || paramBase == Vb6Type::ULong) {
                    // Fix 092q: LongPtr (intptr_t) / ULong ByVal 形参接收 COM 属性
                    // Variant 时也需提取为数值; 缺此项导致 C2440
                    // (vb6_VARIANT 转 intptr_t), 如 SelectObject(hDC, pPicture.Handle).
                    argVal = "vb6_VariantToLong(" + argVal + ")";
                } else if (paramBase == Vb6Type::Double || paramBase == Vb6Type::Single
                           || paramBase == Vb6Type::Currency || paramBase == Vb6Type::Date) {
                    // Fix 090r: Date 形参 (OLE date = double) 与 Currency 同取
                    // vb6_VariantToDouble — 缺 Date 导致实参是 Variant 数组元素
                    // (vb6_VariantArrayGet) 时裸传 → C2440 VARIANT→double
                    // (cZipArchive pvVfsOpen: pvToFileTime(me, arr(i)) 形参 ByVal Date)
                    argVal = "vb6_VariantToDouble(" + argVal + ")";
                } else if (paramBase == Vb6Type::String) {
                    // Fix 049b/092a: 仅当 C 表达式**顶层**已是 BSTR 时跳过提取.
                    // 旧实现用子串 find("vb6_BSTR") — 对"内部实参含
                    // vb6_BSTR_FromStr(...)"的 Variant 表达式天然命中 → 整体跳过
                    // 提取 → C2440 (vb6_VARIANT→BSTR):
                    //   cAesCBC 25 StrConv(LoadResData("AES.CBC","JSCRIPT"), 64, 0)
                    //   → argVal = vb6_LoadResData(vb6_BSTR_FromStr(..), ..) 含子串.
                    // 改为与 5080 同源的**顶层前缀**判断 (只看表达式开头).
                    // 注: 进入本分支的前提是实参已被判为 Variant
                    // (isDefinitelyVariantExpr / cExprIsVariant), 顶层判断足以避免
                    // 对 MsgBox 专用逻辑/4622 分支已转换结果的二次包装.
                    static const char* bstrTopPrefixes092a[] = {
                        "vb6_BSTR_",
                        "VB6_SA_AT(BSTR,",
                        "vb6_VariantToString(",
                        "vb6_BSTR_FromStr("};
                    bool topIsBstr092a = false;
                    for (auto* bp092a : bstrTopPrefixes092a) {
                        if (argVal.compare(0, strlen(bp092a), bp092a) == 0) {
                            topIsBstr092a = true;
                            break;
                        }
                    }
                    if (!topIsBstr092a) {
                        argVal = "vb6_VariantToString(" + argVal + ")";
                    }
                } else if (paramBase == Vb6Type::Object) {
                    // 右值兼容: 避免对函数返回值取址
                    argVal = "vb6_VariantToObjectVal(" + argVal + ")";
                }
            }
        }
        // Fix 086: ByVal Variant形参兜底 — 实参为具体标量/BSTR/对象指针时,
        // 用 _Generic vb6_VariantFromValue 包装. _Generic 按实参C类型自动选择
        // 构造函数 (int→VariantLong, BSTR→VariantString, SafeArray*→VariantArray,
        // 类指针→VariantObject, 已是VARIANT→恒等), 从根源消除 int/BSTR/double→
        // vb6_VARIANT 方向的 C2440.
        if (!isByRef && i < calleeParams.size() && calleeParams[i].isByVal
            && (calleeParams[i].type == Vb6Type::Variant
                || calleeParams[i].type == Vb6Type::Empty)) {
            bool alreadyVariant = cExprIsVariant(argVal);
            if (!alreadyVariant && node.positional[i]->kind == ASTNodeKind::IdentifierExpr) {
                auto& idArg86 = static_cast<IdentifierExpr&>(*node.positional[i]);
                if (knownVariantVars_.count(Symbol::toLower(idArg86.name))) alreadyVariant = true;
            }
            // &(x) 形态是 ByRef 风格临时/取址, 不适合按值包装
            if (!alreadyVariant && argVal.compare(0, 2, "&(") != 0) {
                Vb6Type argT86 = inferExprType(*node.positional[i]);
                bool argIsArr86 = (static_cast<uint16_t>(argT86) & static_cast<uint16_t>(Vb6Type::Array)) != 0;
                Vb6Type argBase86 = static_cast<Vb6Type>(
                    static_cast<uint16_t>(argT86) & ~static_cast<uint16_t>(Vb6Type::Array));
                bool scalarLike =
                    argT86 == Vb6Type::Long || argT86 == Vb6Type::Integer
                    || argT86 == Vb6Type::Boolean || argT86 == Vb6Type::Byte
                    || argT86 == Vb6Type::Double || argT86 == Vb6Type::Single
                    || argT86 == Vb6Type::Currency || argT86 == Vb6Type::Date
                    || argT86 == Vb6Type::LongPtr || argT86 == Vb6Type::String
                    || argT86 == Vb6Type::Object || argBase86 == Vb6Type::Object
                    || argIsArr86;
                if (scalarLike) {
                    argVal = "vb6_VariantFromValue(" + argVal + ")";
                }
            }
        }
        // Fix 038b-2: calleeParams 为空 (运行时/内置函数) 时的参数类型转换.
        // 通过 getRuntimeParamCType 查找期望的 C 类型, 当实参为 Variant 时
        // 自动插入 VARIANT→具体类型提取函数. 解决 vb6_ErrRaise, vb6_BSTR_Assign,
        // vb6_BSTR_Concat, vb6_StrCmp 等运行时函数的 C2440 错误.
        // 注意: 仅使用 cExprIsVariant (C 字符串级) 和 knownVariantVars_ 检测,
        // 不使用 isDefinitelyVariantExpr (AST 级), 因为符号表中的 Variant 返回类型
        // 可能与实际 C 函数返回类型不一致 (如 prop_get 返回 void* 而非 vb6_VARIANT).
        // Fix 091e: 形参符号类型不可信 (无非空参数表, 或符号表把 builtin 形参登记为
        // Variant/Empty — 如 LenB/Len 被推断为 Variant) 时同样查运行时表. 此前仅
        // calleeParams 为空才查表, 导致 Variant 实参原样传给 BSTR 形参:
        // cTlsSocket 200 vb6_LenB_BSTR(vb6_VariantFromComResult(ComGetProp(...))) C2440.
        bool rtParamTypeUsable = (i >= calleeParams.size())
            || calleeParams[i].type == Vb6Type::Variant
            || calleeParams[i].type == Vb6Type::Empty;
        if (!isByRef && rtParamTypeUsable) {
            std::string rtParamType = getRuntimeParamCType(callee, i);
            if (!rtParamType.empty() && rtParamType != "vb6_VARIANT"
                 && rtParamType != "vb6_VARIANT*") {
                bool argIsVariant = cExprIsVariant(argVal);
                // 也检查已知 Variant 变量
                if (!argIsVariant && node.positional[i]->kind == ASTNodeKind::IdentifierExpr) {
                    auto& idArg = static_cast<IdentifierExpr&>(*node.positional[i]);
                    std::string argLower = idArg.name;
                    std::transform(argLower.begin(), argLower.end(), argLower.begin(), ::tolower);
                    if (knownVariantVars_.count(argLower)) argIsVariant = true;
                }
                // Fix 089i: 函数调用返回 Variant 的实参 (VB 未声明返回类型 =
                // Variant, 如 json_ParseErrorMessage(...) 作为 Err.Raise 的
                // Description) — cExprIsVariant 只查字符串前缀, 对
                // "vb6_<mod>_<fn>(...)" 用户函数调用文本不生效 → C2440.
                // 查被调函数符号的返回类型: kind Function/PropertyGet 且
                // returnType Variant → C 层返回 vb6_VARIANT, 需按参数类型提取.
                // 不匹配对象属性 (As Object → void*, returnType 非 Variant).
                if (!argIsVariant && node.positional[i]->kind == ASTNodeKind::IndexOrCallExpr) {
                    auto& callNode89i = static_cast<IndexOrCallExpr&>(*node.positional[i]);
                    if (callNode89i.callee
                        && callNode89i.callee->kind == ASTNodeKind::IdentifierExpr) {
                        std::string calleeLower89i =
                            static_cast<IdentifierExpr&>(*callNode89i.callee).name;
                        std::transform(calleeLower89i.begin(), calleeLower89i.end(),
                                       calleeLower89i.begin(), ::tolower);
                        for (const auto& [k89i, s89i] : symTab_.moduleScope()->symbols()) {
                            if (s89i->kind != SymbolKind::Function
                                && s89i->kind != SymbolKind::PropertyGet) continue;
                            if (s89i->lowerName != calleeLower89i) continue;
                            // 返回类型语义 Variant (VB 未声明返回 = Variant).
                            // 对象属性 As Object → void* (returnType Object),
                            // 不会误匹配; PropertyGet 的 Unknown 属性(可能 void*)
                            // 保守不包, 仅模块 Function 的 Unknown(未推断出类型但
                            // 模块函数 C 返回 vb6_VARIANT)才包.
                            bool retIsVar89i =
                                (s89i->type == Vb6Type::Variant
                                 || s89i->type == Vb6Type::Empty);
                            if (s89i->kind == SymbolKind::PropertyGet) {
                                if (retIsVar89i) argIsVariant = true;
                            } else if (retIsVar89i
                                       || s89i->type == Vb6Type::Unknown) {
                                argIsVariant = true;
                            }
                            break;
                        }
                    }
                }
                if (argIsVariant) {
                    // Fix 090c0: 实参是 COM 结果且被判定为 Variant —
                    // vb6_ComCall/vb6_ComGetProp 返回 void* (COM Variant 承载),
                    // 提取 (VariantToSafeArray1D 等期望 vb6_VARIANT 按值) 前需先
                    // vb6_VariantFromComResult 转为 vb6_VARIANT. 如 cLogs
                    // MakeLogContent: Join(CacheDatas(i), Span) — Collection 元素
                    // 是 Variant 数组 → 原生成 VariantToSafeArray1D(vb6_ComCall(...))
                    // → C2440 (无法从 void* 转换为 vb6_VARIANT).
                    // 精确前缀避免误伤 vb6_ComCallInt( (返回 int32) /
                    // vb6_ComGetStringProp( (返回 BSTR) / vb6_ComCallObject( (对象).
                    bool isComResultVal =
                        (argVal.find("vb6_ComCall(") == 0)
                        || (argVal.find("vb6_ComGetProp(") == 0)
                        || (argVal.find("vb6_ComGetObjectProp(") == 0);
                    if (isComResultVal) {
                        argVal = "vb6_VariantFromComResult(" + argVal + ")";
                    }
                    if (rtParamType == "int32_t" || rtParamType == "int16_t"
                        || rtParamType == "uint8_t" || rtParamType == "LONG") {
                        argVal = "vb6_VariantToLong(" + argVal + ")";
                    } else if (rtParamType == "double" || rtParamType == "float") {
                        argVal = "vb6_VariantToDouble(" + argVal + ")";
                    } else if (rtParamType == "BSTR") {
                        // Fix 089k: 顶层表达式是否已是 BSTR — 原 049b 用
                        // argVal.find("vb6_BSTR") 子串包含判断, 对内部实参含
                        // vb6_BSTR_FromStr(...) 的用户函数调用 (如
                        // json_ParseErrorMessage(...)) 误判"已 BSTR" → 跳过
                        // vb6_VariantToString 包装 → C2440 (vb6_VARIANT→BSTR).
                        // 收紧为剥外层括号后的顶层前缀判断.
                        auto exprTopIsBstr = [](const std::string& s) -> bool {
                            std::string t = s;
                            while (t.size() >= 2 && t.front() == '(' && t.back() == ')') {
                                int depth = 0;
                                bool whole = true;
                                for (size_t k = 0; k < t.size(); k++) {
                                    if (t[k] == '(') depth++;
                                    else if (t[k] == ')') {
                                        depth--;
                                        if (depth == 0 && k < t.size() - 1) {
                                            whole = false;
                                            break;
                                        }
                                    }
                                }
                                if (!whole) break;
                                t = t.substr(1, t.size() - 2);
                            }
                            static const char* bstrTopPrefixes[] = {
                                "vb6_BSTR_",
                                "VB6_SA_AT(BSTR,",
                                // 已由 vb6_VariantToString 转换的结果必为 BSTR,
                                // 顶层识别避免二次包装 → C2440 (BSTR→vb6_VARIANT).
                                "vb6_VariantToString(",
                                // Fix 091h: 移除 "vb6_VariantFromComResult(" —
                                // 该函数返回 vb6_VARIANT (vb6rtl.h:918), 不是 BSTR.
                                // 旧前缀把 COM 属性结果误判为"已 BSTR" → 跳过提取 →
                                // C2440 "vb6_VARIANT → BSTR" (cTlsSocket 200 LenB,
                                // cToolsList 25 prop_let_Filter, cAesCBC 25 StrConv,
                                // Demo_Database 310/506/678 BSTR_Concat).
                                "vb6_BSTR_FromStr("};
                            for (auto* bp : bstrTopPrefixes) {
                                size_t bpl = strlen(bp);
                                if (t.compare(0, bpl, bp) == 0) return true;
                            }
                            return false;
                        };
                        if (!exprTopIsBstr(argVal)) {
                            argVal = "vb6_VariantToString(" + argVal + ")";
                        }
                    } else if (rtParamType == "void*") {
                        argVal = "vb6_VariantToObjectVal(" + argVal + ")";
                    } else if (rtParamType == "vb6_SafeArray1D*") {
                        argVal = "vb6_VariantToSafeArray1D(" + argVal + ")";
                    }
                }
            }
        }
        args.push_back(std::move(argVal));
    }

    // Fix 081c: For named-arg path, track which Optional params were actually passed
    // (before gap-filling sets filled[i]=true for padding params too)
    std::vector<bool> actuallyPassedParams;

    // P14.3.3: 命名参数位置展开 - 按参数名映射到正确位置
    if (!node.named.empty() && !calleeParams.empty()) {
        // Build name->index map from callee params (case-insensitive)
        std::unordered_map<std::string, size_t> paramMap;
        for (size_t i = 0; i < calleeParams.size(); i++) {
            std::string lower = calleeParams[i].name;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (!lower.empty()) paramMap[lower] = i;
        }

        // Create position-mapped args vector
        size_t totalParams = calleeParams.size();
        std::vector<std::string> orderedArgs(totalParams);
        std::vector<bool> filled(totalParams, false);

        // Place positional args (already in args[])
        for (size_t i = 0; i < args.size() && i < totalParams; i++) {
            orderedArgs[i] = std::move(args[i]);
            filled[i] = true;
        }

        // ByRef handling helper lambda
        auto applyByRef = [&](std::string& argVal, size_t pi) {
            // Fix 078 rev2: ByRef array params are now vb6_SafeArray1D**,
            // so they need & at call site — same as other ByRef params.
            bool piHasByValOverride = node.byvalOverrides.count(pi) > 0;
            bool isByRef = (pi < calleeParams.size() && !calleeParams[pi].isByVal && !calleeParams[pi].isParamArray)
                           && !piHasByValOverride;
            if (!isByRef) return;
            bool piCalleeParamIsArray = (pi < calleeParams.size()
                && (static_cast<uint16_t>(calleeParams[pi].type) & static_cast<uint16_t>(Vb6Type::Array)));
            if (argVal.size() > 3 && argVal.substr(0, 2) == "(*" && argVal.back() == ')') {
                // (*x) → depends on callee param type (see Fix 078 rev2 details in position-arg path)
                std::string innerName = argVal.substr(2, argVal.size() - 3);
                if (piCalleeParamIsArray) {
                    argVal = innerName;
                } else {
                    // Fix 079: check if innerName is a ByRef param of the current function
                    bool piIsCurrentByRefParam = false;
                    if (currentProc_) {
                        for (auto& p : currentProc_->params) {
                            if (Symbol::toLower(p.name) == Symbol::toLower(innerName) && !p.isByVal && !p.isParamArray) {
                                bool paramIsArray = (static_cast<uint16_t>(p.type) & static_cast<uint16_t>(Vb6Type::Array)) != 0;
                                if (!paramIsArray) {
                                    piIsCurrentByRefParam = true;
                                }
                                break;
                            }
                        }
                    }
                    if (piIsCurrentByRefParam) {
                        argVal = innerName;
                    } else {
                        argVal = "&" + innerName;
                    }
                }
            } else if (argVal.size() > 2 && argVal[0] == '&') {
                // already has &, keep as-is
            } else if (argVal.size() > 2 && argVal.substr(0, 2) == "me" && argVal[2] == '-') {
                argVal = "&(" + argVal + ")";
            } else {
                bool isSimpleIdent = !argVal.empty() && (std::isalpha(static_cast<unsigned char>(argVal[0])) || argVal[0] == '_');
                if (isSimpleIdent) {
                    for (char c : argVal) {
                        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
                            isSimpleIdent = false; break;
                        }
                    }
                }
                if (isSimpleIdent && !argVal.empty()) {
                    // Fix 079: If the simple identifier is a ByRef param of the current function,
                    // it's already a pointer (T*) — pass directly, don't add &.
                    bool piArgIsCurrentByRefParam = false;
                    if (currentProc_) {
                        for (auto& p : currentProc_->params) {
                            if (Symbol::toLower(p.name) == Symbol::toLower(argVal) && !p.isByVal && !p.isParamArray) {
                                piArgIsCurrentByRefParam = true;
                                break;
                            }
                        }
                    }
                    if (piArgIsCurrentByRefParam) {
                        // ByRef param of current function → already T*, pass directly
                    } else if (argVal == "NULL" || argVal == "0") {
                        // Fix 086: NULL/0 无可取址, 直接传空指针 (ByRef 形参收到 NULL)
                    } else if (isConstIdent(argVal)) {
                        // Fix 084aa: 常量宏不可取址 → 按形参类型复合字面量包装
                        Vb6Type pType = Vb6Type::Variant;
                        if (pi < calleeParams.size()) pType = calleeParams[pi].type;
                        argVal = wrapConstArgForByRef(argVal, pType);
                    } else {
                        argVal = "&" + argVal;
                    }
                } else {
                    std::string cType = "int32_t";
                    if (pi < calleeParams.size()) cType = mapType(calleeParams[pi].type);
                    if (cType == "vb6_VARIANT") {
                        // Fix 086: 非左值表达式传 ByRef Variant — 不能用 {argVal}
                        // 首字段初始化 (C2440: 指针初始化VARTYPE). 空对象/零值
                        // (如内置App对象 (void*)0) → 零初始化VARIANT (VT_EMPTY,
                        // 与VB6传Nothing语义一致); 其余退回零初始化以保编译通过.
                        if (!(argVal == "(void*)0" || argVal == "NULL" || argVal == "0")) {
                            diag_.warn(DiagnosticID::CodeGenUnsupportedFeature, SourceLocation{},
                                "P7.5: ByRef Variant arg is not addressable, passing empty Variant (value dropped): " + argVal);
                        }
                        argVal = "&(vb6_VARIANT){0}";
                    } else {
                        argVal = "&(" + cType + "){" + argVal + "}";
                    }
                }
            }
        };

        // Place named args at their parameter positions
        for (auto& named : node.named) {
            std::string lower = named.name;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            auto it = paramMap.find(lower);
            if (it != paramMap.end()) {
                size_t pi = it->second;
                emitExpr(*named.value);
                std::string argVal = std::move(lastExpr_);
                applyByRef(argVal, pi);
                // Fix 091o: 命名实参 (Name:=expr) 此前只做 ByRef 处理, 漏了 ByVal
                // 形参类型适配 — ByVal Variant 形参收具体类型实参需
                // vb6_VariantFromValue 打包 (_Generic 宏按实类型选 ctor).
                // 例: oListenSocket.Accept(m_oSocket, UseTls:=bUseTls)
                //     (Accept 第 4 参 Optional ByVal UseTls As Variant) →
                //     C2440 int16_t→vb6_VARIANT (cWinsock.c 1604).
                // 类对象实参保持指针直传 (同 4824 Fix 084d).
                bool argIsClassObj091o = !inferClassTypeOfExpr(*named.value).empty();
                if (pi < calleeParams.size() && calleeParams[pi].isByVal
                    && calleeParams[pi].type == Vb6Type::Variant && !argIsClassObj091o) {
                    argVal = "vb6_VariantFromValue(" + argVal + ")";
                }
                if (pi < orderedArgs.size()) {
                    orderedArgs[pi] = std::move(argVal);
                    filled[pi] = true;
                }
            }
        }

        // Fill gaps with optional/missing param defaults
        for (size_t i = 0; i < totalParams; i++) {
            if (!filled[i]) {
                if (calleeParams[i].isParamArray) {
                    orderedArgs[i] = "NULL";
                    // filled[i] stays false for actuallyPassed tracking
                } else {
                    orderedArgs[i] = defaultValue(calleeParams[i].type);
                    // filled[i] stays false for actuallyPassed tracking
                }
            }
        }

        // Fix 081c: Save which params were actually passed (before gap-filling set all to true)
        actuallyPassedParams = filled;

        args = std::move(orderedArgs);
    } else {
        // No named args or no param info: append named args as-is (fallback)
        for (auto& named : node.named) {
            emitExpr(*named.value);
            args.push_back(std::move(lastExpr_));
        }
    }

    // P14.1.5: ParamArray packing - if callee has a ParamArray parameter,

    // pack extra arguments into a SAFEARRAY* and adjust argList

    std::string argList;

    int paIndex = -1;  // index of ParamArray parameter in calleeParams

    for (size_t i = 0; i < calleeParams.size(); i++) {

        if (calleeParams[i].isParamArray) { paIndex = (int)i; break; }

    }



    if (paIndex >= 0) {

        // Split args: normal args [0..paIndex-1] + ParamArray args [paIndex..end]

        int normalCount = paIndex;  // number of non-ParamArray params

        int paArgCount = (int)args.size() - normalCount;

        if (paArgCount < 0) paArgCount = 0;



        // Build normal argList

        for (int i = 0; i < normalCount && i < (int)args.size(); i++) {

            if (i > 0) argList += ", ";

            argList += args[i];

        }



        // Pack ParamArray args into SAFEARRAY* using a temp variable

        std::string paVar = "_pa_" + std::to_string(tempCounter_++);

        if (paArgCount > 0) {

            c_.emitLine("SAFEARRAY* " + paVar + " = vb6_PA_Create(" + std::to_string(paArgCount) + ");");

            for (int i = 0; i < paArgCount; i++) {

                int argIdx = normalCount + i;

                if (argIdx < (int)args.size()) {

                    std::string paArgExpr = args[argIdx];

                    bool isLongArg = false;

                    bool isDoubleArg = false;

                    if (argIdx < (int)node.positional.size()) {

                        auto& paArg = node.positional[argIdx];

                        if (paArg->kind == ASTNodeKind::LiteralExpr) {

                            auto& lit = static_cast<LiteralExpr&>(*paArg);
                            if (lit.literalKind == LiteralKind::Integer) isLongArg = true;
                            else if (lit.literalKind == LiteralKind::Double) isDoubleArg = true;

                        }
                    }

                    /* Fix 082: Check if arg is a COM interface pointer or VarPtr result.
                       These are pointer-sized and must use PA_SetLongPtr on x64. */
                    bool isComPtrArg = false;
                    if (argIdx < (int)node.positional.size()) {
                        auto& paArg = node.positional[argIdx];
                        // Check for VarPtr/ObjPtr/StrPtr call
                        if (paArg->kind == ASTNodeKind::IndexOrCallExpr) {
                            auto& callNode = static_cast<IndexOrCallExpr&>(*paArg);
                            if (callNode.callee && callNode.callee->kind == ASTNodeKind::IdentifierExpr) {
                                auto& ident = static_cast<IdentifierExpr&>(*callNode.callee);
                                std::string vpLower = ident.name;
                                std::transform(vpLower.begin(), vpLower.end(), vpLower.begin(), ::tolower);
                                if (vpLower == "varptr" || vpLower == "objptr" || vpLower == "strptr") {
                                    isComPtrArg = true;
                                }
                            }
                        }
                        // Check for LongPtr/Object variable (COM interface pointer)
                        if (paArg->kind == ASTNodeKind::IdentifierExpr) {
                            auto& ident = static_cast<IdentifierExpr&>(*paArg);
                            std::string identLower = ident.name;
                            std::transform(identLower.begin(), identLower.end(), identLower.begin(), ::tolower);
                            if (knownLongPtrVars_.count(identLower)) {
                                isComPtrArg = true;
                            }
                        }
                    }
                    /* Also detect from C expression: VarPtr result, address-of, COM iface */
                    if (!isComPtrArg) {
                        isComPtrArg = (paArgExpr.find("&(") != std::string::npos ||
                                      paArgExpr.find("(intptr_t)") != std::string::npos ||
                                      paArgExpr.find("vb6_ComIface_") != std::string::npos);
                    }

                    if (isLongArg) {

                        c_.emitLine("vb6_PA_SetLong(" + paVar + ", " + std::to_string(i) + ", " + paArgExpr + ");");

                    } else if (isDoubleArg) {

                        c_.emitLine("vb6_PA_SetDouble(" + paVar + ", " + std::to_string(i) + ", " + paArgExpr + ");");

                    } else {

                        bool looksLikeBSTR = (paArgExpr.find("vb6_BSTR") != std::string::npos ||

                                             paArgExpr.find("L\"") != std::string::npos);

                        if (looksLikeBSTR) {

                            c_.emitLine("vb6_PA_SetBSTR(" + paVar + ", " + std::to_string(i) + ", " + paArgExpr + ");");

                        } else if (isComPtrArg) {

                            /* Fix 082: COM interface pointers and VarPtr() results need PA_SetLongPtr
                               (VT_I8 / intptr_t) to avoid pointer truncation on x64. */
                            c_.emitLine("vb6_PA_SetLongPtr(" + paVar + ", " + std::to_string(i) + ", (intptr_t)" + paArgExpr + ");");

                        } else {

                            c_.emitLine("vb6_PA_SetLong(" + paVar + ", " + std::to_string(i) + ", " + paArgExpr + ");");

                        }

                    }

                }

            }

        } else {

            c_.emitLine("SAFEARRAY* " + paVar + " = NULL;");

        }



        // Append SAFEARRAY* to argList

        if (!argList.empty()) argList += ", ";

        argList += paVar;



        // P14.1.4 Optional padding for params BEFORE the ParamArray

        if (normalCount > (int)args.size()) {

            for (int i = (int)args.size(); i < normalCount; i++) {

                if (!argList.empty()) argList += ", ";

                const auto& param = calleeParams[i];

                std::string defVal;

                if (param.hasDefaultValue && !param.defaultValueExpr.empty()) {

                    defVal = param.defaultValueExpr;

                } else {

                    defVal = defaultValue(param.type);

                }

                if (param.isByVal) {

                    argList += defVal;

                } else {

                    std::string cType = mapType(param.type);

                    // P20-36: Variant/struct types can't use {funcCall()} compound literal
                    if (param.type == Vb6Type::Variant || param.type == Vb6Type::Empty ||
                        param.type == Vb6Type::Null || param.type == Vb6Type::Object) {
                        argList += "&(" + cType + "){0}";
                    } else {
                        argList += "&(" + cType + "){" + defVal + "}";
                    }

                }

            }

        }

        // P20-36: IsMissing _has_ flags for Optional params before ParamArray
        for (int i = 0; i < normalCount; i++) {
            const auto& param = calleeParams[i];
            if (param.isOptional && !param.isParamArray) {
                if (!argList.empty()) argList += ", ";
                argList += (i < (int)args.size()) ? "1" : "0";
            }
        }

    } else {

        // No ParamArray - normal argList construction
        // Fix 041c: Truncate extra args when more args than calleeParams.
        // This handles Declare functions called with more args than their C signature
        // (e.g., CallWindowProcW with 8 VB6 args but 5 C params) and Property Get
        // called with index args that should have been default-member calls.
        size_t maxArgs = args.size();
        if (calleeParamsFound && !calleeIsBuiltin && args.size() > calleeParams.size()) {
            maxArgs = calleeParams.size();
        }

        for (size_t i = 0; i < maxArgs; i++) {

            if (i > 0) argList += ", ";

            argList += args[i];

        }

    }

    // P8.1: UBound/LBound - 1D鐢╲b6_UBound, ND鐢╲b6_UBoundND/vb6_LBoundND

    // P14.1.5: Also handle UBound/LBound on ParamArray parameters

    if (callee == "vb6_UBound" || callee == "vb6_LBound") {

        // P14.1.5: Check if first arg is a ParamArray parameter

        bool firstArgIsPA = false;

        if (node.positional.size() >= 1 && currentProc_) {

            auto& firstArg = node.positional[0];

            if (firstArg->kind == ASTNodeKind::IdentifierExpr) {

                std::string argLower = static_cast<IdentifierExpr&>(*firstArg).name;

                std::transform(argLower.begin(), argLower.end(), argLower.begin(), ::tolower);

                for (auto& p : currentProc_->params) {

                    if (p.isParamArray) {

                        std::string pLower = p.name;

                        std::transform(pLower.begin(), pLower.end(), pLower.begin(), ::tolower);

                        if (pLower == argLower) {

                            firstArgIsPA = true;

                            break;

                        }

                    }

                }

            }

        }



        if (firstArgIsPA) {

            // ParamArray: UBound(args) -> vb6_PA_UBound(args), LBound(args) -> vb6_PA_LBound(args)

            if (callee == "vb6_UBound") {

                callee = "vb6_PA_UBound";

            } else {

                callee = "vb6_PA_LBound";

            }

            // argList should be just the PA variable name (no dimension param)

            if (args.size() == 1) {

                argList = args[0];

            }

        } else {

            // Fix 090n: UBound/LBound 首参若是 Variant 数组表达式 (Variant 变量 /
            // UDT 的 As Variant 字段如 vb6_ret_X.BufferArray), 需提取 SafeArray1D*:
            // RTL 签名 vb6_UBound(vb6_SafeArray1D*, ...), 直接传 VARIANT → C2440
            // (cZipArchive pvVfsCreate: UBound(pvVfsCreate.BufferArray, 1)).
            if (!args.empty() && node.positional.size() >= 1
                && args[0].find("vb6_VariantToSafeArray1D") != 0) {
                bool arrFlag090n = false;
                bool vFlag090n = isDefinitelyVariantExpr(*node.positional[0], &arrFlag090n);
                if (vFlag090n) {
                    args[0] = "vb6_VariantToSafeArray1D(&" + args[0] + ")";
                }
            }

            if (args.size() == 1) {

                // 缺省维度参数, 补1

                argList += ", 1";

            }

            // 检查是否为ND数组, 需要用ND版本

            // Bug #1 fix: 两种检测方式:
            // 1) arrayDimCounts_中有记录且维度>1 (适用于Dim声明的ND数组)
            // 2) dimension参数值>1 (适用于ByRef参数, 声明时维度未知但调用时指定)

            bool useND = false;

            if (node.positional.size() >= 1) {

                auto& firstArg = node.positional[0];

                std::string arrLower;

                if (firstArg->kind == ASTNodeKind::IdentifierExpr) {

                    arrLower = static_cast<IdentifierExpr&>(*firstArg).name;

                    std::transform(arrLower.begin(), arrLower.end(), arrLower.begin(), ::tolower);

                }

                auto itDc = arrayDimCounts_.find(arrLower);

                if (itDc != arrayDimCounts_.end() && itDc->second > 1) {

                    useND = true;

                }

                // Bug #1 fix: 如果dimension参数值>1, 也使用ND版本

                // dimension参数是第二个参数 (index 1 in args, after the array arg)

                if (!useND && args.size() >= 2) {

                    // args[1] 是 dimension 参数的C表达式, 检查是否是常量>1

                    const std::string& dimExpr = args[1];

                    // 尝试解析为整数常量

                    try {

                        int dimVal = std::stoi(dimExpr);

                        if (dimVal > 1) {

                            useND = true;

                        }

                    } catch (...) {

                        // 非常量表达式, 无法确定; 不启用ND

                    }

                }

                // Bug #1 fix (082h): 第三种检测 - 同一过程中已有UBound(arr,N>1)使用ND版本

                // 例如: UBound(uVectors, 2) 用了ND版本, 则 UBound(uVectors, 1) 也应使用ND版本

                if (!useND && knownNDArraysInProc_.count(arrLower)) {

                    useND = true;

                }

                if (useND) {

                    // 注册到过程内ND数组集合, 后续 UBound(arr,1) 也会用ND版本

                    if (!arrLower.empty()) {

                        knownNDArraysInProc_.insert(arrLower);

                    }

                    // ND数组 -> 使用vb6_UBoundND/vb6_LBoundND

                    if (callee == "vb6_UBound") {

                        callee = "vb6_UBoundND";

                    } else {

                        callee = "vb6_LBoundND";

                    }

                    // ND版本需要(vb6_SafeArrayND*)转换第一个参数

                    // argList格式是 "arrExpr, dimExpr", 需要改为 "(vb6_SafeArrayND*)(arrExpr), dimExpr"

                    if (args.size() >= 2) {

                        argList = "(vb6_SafeArrayND*)(" + args[0] + "), " + args[1];

                    } else if (args.size() == 1) {

                        argList = "(vb6_SafeArrayND*)(" + args[0] + "), 1";

                    }

                }

            }

        }

    }    // InStr: VB6允许2参数形式 InStr(string1, string2)
    // RTL: vb6_InStr(start, haystack, needle) → 2参数时补start=1
    if (callee == "vb6_InStr" || callee == "vb6_InStrB") {
        if (args.size() == 2) {
            argList = "1, " + argList;
        }
    }

    // P14.2.2: CurDir - VB6 allows 0-arg CurDir() → vb6_CurDir(NULL)
    if (callee == "vb6_CurDir") {
        if (args.empty()) {
            argList = "NULL";
        }
    }

    // P14.2.2: Dir - VB6 allows 1-arg Dir(pattern) → vb6_Dir(pattern, 0)
    if (callee == "vb6_Dir") {
        if (args.size() == 1) {
            argList += ", 0";
        }
        // Fix 086: 无参 Dir() (继续上次搜索) → 传 NULL, 0
        if (args.empty()) {
            argList = "NULL, 0";
        }
    }

    // P14.2.3: Split - VB6 Split(expr[, delim[, limit[, compare]]])
    // RTL: vb6_Split(expr, delim, limit, compare)
    if (callee == "vb6_Split") {
        if (args.size() == 1) {
            argList += ", NULL";        // default delimiter = space
        }
        if (args.size() <= 2) {
            argList += ", -1";          // limit = -1 (unlimited)
        }
        if (args.size() <= 3) {
            argList += ", 0";           // compare = binary
        }
    }

    // P14.2.3: Join - VB6 Join(arr[, delimiter])
    // RTL: vb6_Join(arr, delimiter)
    if (callee == "vb6_Join") {
        if (args.size() == 1) {
            argList += ", NULL";        // default delimiter = space
        }
    }

    // P14.2.4: DateDiff - VB6 DateDiff(interval, date1, date2[, firstDayOfWeek[, firstWeekOfYear]])
    // RTL: vb6_DateDiff(interval, date1, date2, firstDayOfWeek, firstWeekOfYear)
    if (callee == "vb6_DateDiff") {
        if (args.size() == 3) {
            argList += ", 1, 1";  // vbSunday, vbFirstJan1
        } else if (args.size() == 4) {
            argList += ", 1";     // vbFirstJan1
        }
    }

    // P14.2.4: DatePart - VB6 DatePart(interval, date[, firstDayOfWeek[, firstWeekOfYear]])
    // RTL: vb6_DatePart(interval, date, firstDayOfWeek, firstWeekOfYear)
    if (callee == "vb6_DatePart") {
        if (args.size() == 2) {
            argList += ", 1, 1";  // vbSunday, vbFirstJan1
        } else if (args.size() == 3) {
            argList += ", 1";     // vbFirstJan1
        }
    }

    // Replace: VB6允许3参数形式 Replace(string, find, replacement)
    // RTL: Replace(string, find, replacement, start, count, compare)
    if (callee == "vb6_Replace") {
        if (args.size() == 3) {
            argList += ", 1, -1, 0";
        } else if (args.size() == 4) {
            argList += ", -1, 0";
        } else if (args.size() == 5) {
            argList += ", 0";
        }
    }

    // P21-B: Weekday(date[, firstDayOfWeek]) - default firstDayOfWeek=1 (vbSunday)
    if (callee == "vb6_Weekday") {
        if (args.size() == 1) {
            argList += ", 1";
        }
    }

    // P21-10: FormatDateTime(date[, namedFormat]) - default namedFormat=0 (vbGeneralDate)
    if (callee == "vb6_FormatDateTime") {
        if (args.size() == 1) {
            argList += ", 0";
        }
    }

    // P21-16: NPer(rate, pmt, pv[, fv][, type]) - defaults: fv=0, type=0
    if (callee == "vb6_NPer") {
        if (args.size() == 3) argList += ", 0, 0";
        else if (args.size() == 4) argList += ", 0";
    }

    // P21-19: IRR(values[, guess]) - default guess=0.1
    if (callee == "vb6_IRR") {
        if (args.size() == 1) argList += ", 0.1";
    }

    // P20-37: GetSetting(app, section, key[, default]) - default is empty string
    if (callee == "vb6_GetSetting") {
        if (args.size() == 3) argList += ", vb6_BSTR_Empty()";
    }

    // Fix 010e: Err.Raise with Source/Description arguments
    // Err.Raise always calls vb6_ErrRaise(number, source, description) — 3 args
    // Pad with NULL BSTRs when fewer args provided
    if (callee == "vb6_ErrRaise" && args.size() < 3) {
        // Pad to 3 args
        while (args.size() < 3) {
            args.push_back("(BSTR)0");
            if (!argList.empty()) argList += ", ";
            argList += "(BSTR)0";
        }
    }
    if (callee == "vb6_ErrRaise" && args.size() > 3) {
        // Truncate to 3 (Err.Raise can have up to 5 VB6 args, C RTL handles 3)
        argList = args[0] + ", " + args[1] + ", " + args[2];
    }

    // Fix 010r: Builtin RTL functions with optional parameters — pad to required arg count
    // VB6 allows omitting optional args; C RTL functions require all args
    if (callee == "vb6_Mid" && args.size() < 3) {
        while (args.size() < 3) {
            args.push_back("0");
            if (!argList.empty()) argList += ", ";
            argList += "0";
        }
    }
    if (callee == "vb6_StrConv" && args.size() < 3) {
        while (args.size() < 3) {
            args.push_back("0");
            if (!argList.empty()) argList += ", ";
            argList += "0";
        }
    }
    if (callee == "vb6_FormatNumber" && args.size() < 5) {
        while (args.size() < 5) {
            args.push_back("-1");  // -1 = vbUseDefault for optional args
            if (!argList.empty()) argList += ", ";
            argList += "-1";
        }
    }
    // Fix 034: Builtin RTL functions with Optional params — pad to required C RTL arg count.
    // calleeParams 已对 builtin 标 isOptional, 但 general padding (line 3291+) 被 !calleeIsBuiltin
    // 跳过 (RTL C 函数不接受 _has_ 尾叜). 各函数硬编码补默认值, 默认值取自 VB6 语义.
    // InStrRev(string1, string2[, start[, compare]]) — start=-1 (从末尾), compare=0 (Binary)
    if (callee == "vb6_InStrRev") {
        if (args.size() == 2) {
            argList += ", -1, 0";
        } else if (args.size() == 3) {
            argList += ", 0";
        }
    }
    // Round(x[, decimals]) — decimals default = 0
    if (callee == "vb6_Round" && args.size() == 1) {
        argList += ", 0";
    }
    // StrComp(s1, s2[, compare]) — compare default = 0 (Binary)
    if (callee == "vb6_StrComp" && args.size() == 2) {
        argList += ", 0";
    }
    // Shell(pathname[, windowstyle]) — windowstyle default = 2 (vbMinimizedFocus)
    if (callee == "vb6_Shell" && args.size() == 1) {
        argList += ", 2";
    }
    // Rnd([seed]) — seed default = 0 (Rnd with no arg uses last seed or random)
    // Randomize([seed]) — handled as Sub bare-call; see cgen_stmt.cpp Randomize dispatch
    if (callee == "vb6_Rnd" && args.empty()) {
        argList = "0";
    }
    // Randomize As Function-call form `Randomize()` — pad seed = 0.0
    if (callee == "vb6_Randomize" && args.empty()) {
        argList = "0.0";
    }
    // Fix 041: InStr(start, string1, string2, compare) — C function vb6_InStr takes 3 args
    // (no compare parameter). Truncate the 4th arg (compare) when present.
    if (callee == "vb6_InStr" && args.size() > 3) {
        argList = args[0] + ", " + args[1] + ", " + args[2];
    }

    // P14.1.4: General Optional parameter padding for user-defined functions
    // calleeParams is empty for builtin RTL functions (registered without params), so they're auto-skipped
    // Fix 030b: builtin 即使现在有 calleeParams (用于触发包装), 也不走 padding/IsMissing 路径
    // (RTL C 签名不接受尾叜 _has_ flag, 默认值由 builtin 的特殊 codegen 处理如 UBound 补 dimension=
    if (calleeParams.size() > 0 && args.size() < calleeParams.size() && paIndex < 0 && !calleeIsBuiltin) {
        for (size_t i = args.size(); i < calleeParams.size(); i++) {
            if (i > 0 || !args.empty()) argList += ", ";
            const auto& param = calleeParams[i];
            // Determine the default value (explicit or type-zero)
            std::string defVal;
            if (param.hasDefaultValue && !param.defaultValueExpr.empty()) {
                defVal = param.defaultValueExpr;
            } else {
                defVal = defaultValue(param.type);
            }
            // ByRef params need pointer, ByVal need value
            if (param.isByVal) {
                argList += defVal;
            } else {
                // ByRef: pass address of compound literal: &(type){defVal}
                std::string cType = mapType(param.type);
                // P20-36: Variant/struct types can't use {funcCall()} compound literal
                if (param.type == Vb6Type::Variant || param.type == Vb6Type::Empty ||
                    param.type == Vb6Type::Null || param.type == Vb6Type::Object) {
                    argList += "&(" + cType + "){0}";
                } else {
                    argList += "&(" + cType + "){" + defVal + "}";
                }
            }
                    }

                    // Fix 056: ND版本需要vb6_SafeArrayND*参数, 动态数组声明为1D*
                    if (!args.empty()) {
                        args[0] = "(vb6_SafeArrayND*)" + args[0];
                    }

                }
    // P20-36: IsMissing support - append _has_ flags for Optional params
    // For each Optional param in calleeParams: 1 if actually passed, 0 if padded
    // Fix 030b: builtin 跳过 (RTL C 函数无 _has_ 尾叜)
    // Fix 042a: Declare 函数也跳过 (Declare C 签名无 _has_ 尾叜, 但 Optional padding 仍需要)
    if (calleeParams.size() > 0 && paIndex < 0 && !calleeIsBuiltin && !calleeIsDeclare) {
        bool hasOptional = false;
        for (size_t i = 0; i < calleeParams.size(); i++) if (calleeParams[i].isOptional && !calleeParams[i].isParamArray) { hasOptional = true; break; }
        if (hasOptional) {
        for (size_t i = 0; i < calleeParams.size(); i++) {
            const auto& param = calleeParams[i];
            if (param.isOptional && !param.isParamArray) {
                if (!argList.empty()) argList += ", ";
                // Fix 081c: For named-arg path, use actuallyPassedParams to determine _has_ flag.
                // For positional-arg path, args.size() already equals actual arg count (padding
                // doesn't increase args.size()), so (i < args.size()) is correct.
                if (!actuallyPassedParams.empty() && i < actuallyPassedParams.size()) {
                    argList += actuallyPassedParams[i] ? "1" : "0";
                } else {
                    argList += (i < args.size()) ? "1" : "0";
                }
            }
        }
        }  // end if (hasOptional)
    }  // end if (calleeParams.size() > 0 && ...)

    // P6.6: 类模块中调用同类方法(包括递归), 需要自动添加me作为第一个参数
    // 如 Factorial(N-1) -> vb6_MathLib_Factorial(me, (N-1))
    if (classMethodObjArg.empty() && isClassModule_ && currentProc_) {
        std::string modPrefix = "vb6_" + cIdent(moduleName_) + "_";
        if (callee.find(modPrefix) == 0) {
            classMethodObjArg = "(void*)me";
        }
    }

    // P6.5: 如果classMethodObjArg非空, 需要将其作为第一个参数插入
    if (!classMethodObjArg.empty()) {
        if (argList.empty()) {
            argList = classMethodObjArg;
        } else {
            argList = classMethodObjArg + ", " + argList;
        }
    }

    // M22-fix: CStr类型适配 — 根据参数类型选择正确的CStr变体
    if (callee == "vb6_CStr" && !node.positional.empty()) {
        auto& firstArg = node.positional[0];
        if (firstArg->kind == ASTNodeKind::IdentifierExpr) {
            auto& idArg = static_cast<IdentifierExpr&>(*firstArg);
            std::string argLower = idArg.name;
            std::transform(argLower.begin(), argLower.end(), argLower.begin(), ::tolower);
            // 参数是BSTR变量 → CStr是空操作, 直接使用参数
            if (knownBstrVars_.count(argLower)) {
                emitExpr(*firstArg);
                return;
            }
            // 参数是int32_t/Long变量 → 用vb6_CStrLong
            if (knownLongVars_.count(argLower)) {
                callee = "vb6_CStrLong";
            }
            // 参数是double变量 → 用vb6_CStrDbl
            else if (knownDoubleVars_.count(argLower)) {
                callee = "vb6_CStrDbl";
            }
            // 参数是Variant变量 → 保留vb6_CStr(VARIANT)
        } else {


            // 非标识符表达式: 推断类型选择CStr变体
            Vb6Type argType = inferExprType(*firstArg);
            if (argType == Vb6Type::String) {
                // Fix 056b: CStr(String) 是空操作 → 直接使用参数
                // (否则生成 vb6_CStr(BSTR) → C2440: 无法从BSTR转换为vb6_VARIANT)
                if (!args.empty()) { lastExpr_ = args[0]; return; }
                lastExpr_ = "vb6_BSTR_Empty()";
                return;
            }
            if (argType == Vb6Type::Long || argType == Vb6Type::Integer) {
                callee = "vb6_CStrLong";
            } else if (argType == Vb6Type::Double || argType == Vb6Type::Single) {
                callee = "vb6_CStrDbl";
            }
            // P25: COM属性取值已在args[0]中, 根据取值函数选择CStr变体或跳过
            if (!args.empty()) {
                const std::string& a0 = args[0];
                if (a0.find("vb6_ComGetStringProp") == 0 || a0.find("vb6_ComCallBSTR") == 0) {
                    // 已是BSTR, CStr是空操作
                    lastExpr_ = a0;
                    return;
                } else if (a0.find("vb6_ComGetIntProp") == 0 || a0.find("vb6_ComVtableGetInt") == 0) {
                    callee = "vb6_CStrLong";
                } else if (a0.find("vb6_ComGetDoubleProp") == 0 || a0.find("vb6_ComVtableGetDouble") == 0) {
                    callee = "vb6_CStrDbl";
                }
            }
        }
        // Fix 056b: callee 仍为 vb6_CStr 且实参非Variant → 用 vb6_VariantFromValue 包装
        // (如 CStr((int32_t)GetCurrentThreadId()) / CStr(模块级Long变量) →
        //  vb6_CStr(vb6_VariantFromValue(x)), 由 _Generic 按实参类型自动包装, 消除 C2440)
        if (callee == "vb6_CStr" && !args.empty()) {
            lastExpr_ = "vb6_CStr(vb6_VariantFromValue(" + args[0] + "))";
            return;
        }
    }
    // P8.4: Variant参数适配 — 如果目标函数不接受Variant但参数是Variant类型, 使用V后缀函数
    // CInt(Variant)→vb6_CIntV, CDbl(Variant)→vb6_CDblV, CLng(Variant)→vb6_CLngV
    // Fix 090d: 扩展覆盖 CByte/CSng/CBool (CByte("&H"&hex) 字符串实参同样需解析适配)
    if (callee == "vb6_CInt" || callee == "vb6_CLng" || callee == "vb6_CDbl"
        || callee == "vb6_CByte" || callee == "vb6_CSng" || callee == "vb6_CBool") {
        // 检查第一个参数是否为Variant变量
        bool firstArgIsVariant = false;
        bool firstArgIsBstr = false;
        if (!node.positional.empty()) {
            auto& firstArg = node.positional[0];
            if (firstArg->kind == ASTNodeKind::IdentifierExpr) {
                auto& idArg = static_cast<IdentifierExpr&>(*firstArg);
                std::string argLower = idArg.name;
                std::transform(argLower.begin(), argLower.end(), argLower.begin(), ::tolower);
                if (knownVariantVars_.count(argLower)) firstArgIsVariant = true;
                else if (knownBstrVars_.count(argLower)) firstArgIsBstr = true;
            }
            // Fix 036: 非标识符 Variant 表达式 (函数返回 Variant 的调用/类方法等) 也需 V 后缀
            if (!firstArgIsVariant) {
                firstArgIsVariant = isDefinitelyVariantExpr(*firstArg);
            }
            // Fix 038b-4: 字符串级 Variant 检测回退 — 捕获 vb6_VariantArrayGet 等表达式
            if (!firstArgIsVariant && !args.empty()) {
                firstArgIsVariant = cExprIsVariant(args[0]);
            }
        }
        if (firstArgIsVariant) {
            if (callee == "vb6_CInt") callee = "vb6_CIntV";
            else if (callee == "vb6_CLng") callee = "vb6_CLngV";
            else if (callee == "vb6_CDbl") callee = "vb6_CDblV";
            else if (callee == "vb6_CByte") callee = "vb6_CByteV";
            else if (callee == "vb6_CSng") callee = "vb6_CSngV";
            else if (callee == "vb6_CBool") callee = "vb6_CBoolV";
            // Fix 084l: 撤销 Fix 038b-2 的 Variant→double 提前提取 (getRuntimeParamCType
            // 把 CLng/CDbl/CInt 形参当作 double, 已将 lRet 替换为 vb6_VariantToDouble(lRet)).
            // V 后缀函数直接接受 vb6_VARIANT — 保留提取会产生 vb6_CLngV(double) → C2440.
            if (!args.empty()) {
                static const char* extractPrefixes[] = {
                    "vb6_VariantToDouble(", "vb6_VariantToLong("};
                for (auto* pre : extractPrefixes) {
                    size_t pl = strlen(pre);
                    if (args[0].compare(0, pl, pre) == 0 && args[0].size() > pl + 1) {
                        args[0] = args[0].substr(pl, args[0].size() - pl - 1);
                        argList.clear();
                        for (size_t ai = 0; ai < args.size(); ai++) {
                            if (ai > 0) argList += ", ";
                            argList += args[ai];
                        }
                        break;
                    }
                }
            }
        }
        // Fix 036: BSTR 参数 → vb6_NumVal 转换为 double (CInt/CLng/CDbl/CByte/CSng/CBool
        // 接 double). Fix 090d: vb6_Val 不识别 "&H" 前缀 (Val("&HFF")=0), 而 VB6 类型
        // 转换函数解析 &H/&O → CByte("&H" & hex) 需 vb6_NumVal; 十进制语义不变.
        // Fix 084c: 扩展检测到字符串级 BSTR 表达式 (vb6_Trim/vb6_BSTR_Concat/vb6_CStr
        // /vb6_VariantToString 等), 不仅限于已知 BSTR 标识符 — 否则
        // vb6_CLng(vb6_Trim(vb6_VariantToString(...))) 触发 C2440 "BSTR → double".
        bool argIsBstrExpr = firstArgIsBstr;
        if (!argIsBstrExpr && !args.empty()) {
            std::string& a0 = args[0];
            argIsBstrExpr =
                a0.find("vb6_Trim(") == 0 || a0.find("vb6_LTrim(") == 0 ||
                a0.find("vb6_RTrim(") == 0 || a0.find("vb6_StrConv(") == 0 ||
                a0.find("vb6_Mid(") == 0 || a0.find("vb6_Left(") == 0 ||
                a0.find("vb6_Right(") == 0 || a0.find("vb6_Replace(") == 0 ||
                a0.find("vb6_String(") == 0 || a0.find("vb6_Format(") == 0 ||
                a0.find("vb6_UCase(") == 0 || a0.find("vb6_LCase(") == 0 ||
                a0.find("vb6_Space(") == 0 || a0.find("vb6_IIfBSTR(") == 0 ||
                a0.find("vb6_Chr(") == 0 || a0.find("vb6_ChrW(") == 0 ||
                a0.find("vb6_BSTR_Concat(") == 0 || a0.find("vb6_BSTR_FromStr(") == 0 ||
                a0.find("vb6_CStr") == 0 || a0.find("vb6_VariantToString(") == 0 ||
                a0.find("VB6_SA_AT(BSTR,") != std::string::npos;
        }
        if (argIsBstrExpr && !args.empty()) {
            args[0] = "vb6_NumVal(" + args[0] + ")";
            argList.clear();
            for (size_t i = 0; i < args.size(); i++) {
                if (i > 0) argList += ", ";
                argList += args[i];
            }
        }
    }
    // MsgBox自动BSTR转换: MsgBox期望BSTR, 非BSTR的prompt参数需包装
    // 注意: 只转换第一个参数(prompt), 不能把整个argList包进去(MsgBox v, , title时argList含3个参数)
    if (callee == "vb6_MsgBox" || callee == "vb6_MsgBox1") {
        if (!args.empty()) {
            std::string& firstArg = args[0];
            if (firstArg.find("vb6_ComGetIntProp") == 0 ||
                firstArg.find("vb6_ComVtableGetInt") == 0) {
                firstArg = "vb6_CStrLong(" + firstArg + ")";
            } else if (firstArg.find("vb6_ComGetDoubleProp") == 0 ||
                       firstArg.find("vb6_ComVtableGetDouble") == 0) {
                firstArg = "vb6_CStrDbl(" + firstArg + ")";
            } else if (firstArg.find("vb6_VariantFromComResult") == 0) {
                // COM调用结果(VARIANT*)→vb6_VARIANT, 需转BSTR
                firstArg = "vb6_VariantToString(" + firstArg + ")";
            } else if (firstArg.find("vb6_ComCall(") == 0) {
                // P24-01: 后期绑定COM调用返回VARIANT*, 需解包转BSTR
                firstArg = "vb6_VariantToString(vb6_VariantFromComResult(" + firstArg + "))";
            } else if (firstArg.find("vb6_VariantToString(") == 0) {
                // Fix 090u: 实参已是 VariantToString(...) (参数打包段对 Variant 实参
                // 按 BSTR 形参已转换一次, 如 MsgBox .FindFirst(...) As Variant /
                // MsgBox .Value(1,6) As Variant) — 再包一层 → VariantToString(BSTR)
                // C2440 "BSTR→vb6_VARIANT". 此处保持一层.
            } else if (!node.positional.empty() && inferExprType(*node.positional[0]) == Vb6Type::Variant) {
                // Variant类型变量/表达式: MsgBox v → vb6_VariantToString(v)
                firstArg = "vb6_VariantToString(" + firstArg + ")";
            }
        }
    }
    // P25: MsgBox BSTR转换可能修改了args[0], 需重建argList
    if (callee == "vb6_MsgBox" || callee == "vb6_MsgBox1") {
        argList.clear();
        for (size_t i = 0; i < args.size(); i++) {
            if (i > 0) argList += ", ";
            argList += args[i];
        }
    }
    // MsgBox(prompt) -> vb6_MsgBox1(prompt)
    // MsgBox(prompt, buttons) -> vb6_MsgBox(prompt, buttons, NULL)
    if (callee == "vb6_MsgBox") {
        if (node.positional.size() == 1) {
            callee = "vb6_MsgBox1";
        } else if (node.positional.size() == 2) {
            argList += ", NULL";
        }
    }
    // P26: Format 第一个参数需要包装为 vb6_VARIANT
    if (callee == "vb6_Format" && !args.empty()) {
        Vb6Type argType = inferExprType(*node.positional[0]);
        if (argType == Vb6Type::Long || argType == Vb6Type::Integer) {
            args[0] = "vb6_VariantLong(" + args[0] + ")";
        } else if (argType == Vb6Type::Double || argType == Vb6Type::Single) {
            args[0] = "vb6_VariantDouble(" + args[0] + ")";
        } else if (argType == Vb6Type::String) {
            args[0] = "vb6_VariantString(" + args[0] + ")";
        } else if (argType == Vb6Type::Boolean) {
            args[0] = "vb6_VariantInt((int16_t)(" + args[0] + "))";
        } else if (argType == Vb6Type::Byte) {
            args[0] = "vb6_VariantInt((int16_t)(" + args[0] + "))";
        } else if (argType == Vb6Type::Date) {
            args[0] = "vb6_VariantDouble((double)(" + args[0] + "))";
        }
        // Fix 036: Format fallback — 未匹配类型 (Currency/Unknown 等, 非 Variant) 用
        // vb6_VariantFromValue 包装. Variant 类型无需包装 (已是 vb6_VARIANT).
        else if (argType != Vb6Type::Variant) {
            args[0] = "vb6_VariantFromValue(" + args[0] + ")";
        }
        argList.clear();
        for (size_t i = 0; i < args.size(); i++) {
            if (i > 0) argList += ", ";
            argList += args[i];
        }
    }
    // Fix 036: CStr fallback — 当特殊分支未匹配 (Boolean/Byte/Date/Currency/常量/
    // 未注册函数返回值/Const BSTR 等), callee 仍为 vb6_CStr (接 vb6_VARIANT). 用
    // vb6_VariantFromValue 包装 args[0], _Generic 按实参 C 类型自动选择 Variant ctor,
    // 避免 concrete → VARIANT C2440. 对已是 vb6_VARIANT 的实参为 identity (no-op), 安全.
    // 但需排除确定 Variant 的表达式 (如 Me.Segments(i) 返回 Variant): _Generic 宏
    // 对某些 Variant 表达式展开可能产生逗号问题 → C2197, 故用 isDefinitelyVariantExpr
    // 跳过, 保留 vb6_CStr(variantExpr) 原样 (vb6_CStr 接 VARIANT, 直接可用).
    if (callee == "vb6_CStr" && !args.empty()) {
        bool argIsDefVariant = false;
        if (!node.positional.empty()) {
            argIsDefVariant = isDefinitelyVariantExpr(*node.positional[0]);
        }
        // Fix 038b-4: 字符串级 Variant 检测 — 如果是 C 级 Variant 表达式,
        // 也跳过 VariantFromValue 包装 (vb6_CStr 直接接 VARIANT)
        if (!argIsDefVariant) {
            argIsDefVariant = cExprIsVariant(args[0]);
        }
        if (!argIsDefVariant) {
            args[0] = "vb6_VariantFromValue(" + args[0] + ")";
            argList.clear();
            for (size_t i = 0; i < args.size(); i++) {
                if (i > 0) argList += ", ";
                argList += args[i];
            }
        }
    }
    // Fix 040d: CallByName variadic args packing
    // C signature: vb6_CallByName(void* obj, const wchar_t* procName, int32_t callType,
    //                              void* args, int32_t argc)
    // First 3 VB6 args map to obj/procName/callType. Extra args (4+) are method
    // arguments, packed into (void*[]){vb6_ComPackValue(arg), ...} with argc.
    if (callee == "vb6_CallByName" && args.size() >= 3) {
        if (args.size() == 3) {
            // No extra method args: pass NULL, 0
            argList = args[0] + ", " + args[1] + ", " + args[2] + ", NULL, 0";
        } else {
            // Pack extra args [3..] into void*[] array using vb6_ComPackValue
            std::string argsArray = "(void*[]){";
            for (size_t i = 3; i < args.size(); i++) {
                if (i > 3) argsArray += ", ";
                argsArray += "vb6_ComPackValue(" + args[i] + ")";
            }
            argsArray += "}";
            int32_t extraArgc = (int32_t)args.size() - 3;
            argList = args[0] + ", " + args[1] + ", " + args[2] + ", "
                    + argsArray + ", " + std::to_string(extraArgc);
        }
    }
    // Fix 065: LenB(UDT) — MSVC _Generic 对自定义结构体类型匹配有问题,
    // 直接生成 sizeof(vb6_type_XXX) 而不经 vb6_LenB _Generic 宏
    if (callee == "vb6_LenB" && args.size() == 1) {
        // 检查参数是否是已知 UDT 变量
        std::string argLower = args[0];
        // 去除可能的 With 前缀: _vb6_with_N. → 提取变量名
        // 去除 me-> 前缀
        if (argLower.substr(0, 4) == "me->") argLower = argLower.substr(4);
        std::transform(argLower.begin(), argLower.end(), argLower.begin(), ::tolower);
        auto udtIt = knownUdtVars_.find(argLower);
        if (udtIt != knownUdtVars_.end()) {
            lastExpr_ = "(int32_t)sizeof(" + udtIt->second + ")";
            return;
        }
        // Fix 084h: UDT 数组元素 LenB(VB6_SA_AT(vb6_type_XXX, arr, idx)) →
        // sizeof(vb6_type_XXX)。_Generic 宏无法匹配自定义结构体类型, 且
        // 数组元素类型未知, 直接由类型名生成 sizeof。
        {
            std::string rawArg = args[0];
            std::string rawLower = rawArg;
            std::transform(rawLower.begin(), rawLower.end(), rawLower.begin(), ::tolower);
            const char* saPrefix = "vb6_sa_at(vb6_type_";
            if (rawLower.compare(0, 19, saPrefix) == 0) {
                size_t comma = rawArg.find(',');
                if (comma != std::string::npos) {
                    std::string typeName = rawArg.substr(19, comma - 19);
                    lastExpr_ = "(int32_t)sizeof(" + typeName + ")";
                    return;
                }
            }
        }
    }
    lastExpr_ = callee + "(" + argList + ")";
}

} // namespace vb6c3
