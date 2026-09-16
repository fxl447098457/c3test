#include "backend/cgen.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <functional>

namespace vb6c3 {

// --- cgen_redim.cpp: ReDim/Erase 数组语句生成 ---

void CCodeGen::visit(ReDimStmt& node) {
    // Fix 084y-5: ReDim 目标含成员访问 (ByRef UDT 参数数组字段 uOutput.Buffer,
    // With 块成员 .Field) 时按成员访问展开, 避免 cIdent 把 '.' 替换成 '_'
    std::string cName = resolveArrayTargetIdent(node.varName);
    // Fix 010r: Add me-> prefix for class member arrays
    std::string lowerVar = node.varName;
    std::transform(lowerVar.begin(), lowerVar.end(), lowerVar.begin(), ::tolower);
    // Fix 086: ReDim 目标是当前 Function/PropertyGet 自身名 → 返回值变量
    // (VB6: 数组返回函数内 ReDim Preserve FuncName(...) 重设返回数组.
    //  此前生成裸函数名 → C2065, 如 cMemoryStream.Contents / cImage.PictureToBytes)
    if (currentProc_ && !currentReturnVar_.empty()
        && (currentProc_->kind == SymbolKind::Function
            || currentProc_->kind == SymbolKind::PropertyGet)
        && Symbol::toLower(currentProc_->name) == lowerVar
        && !knownLocalVars_.count(lowerVar)) {
        cName = currentReturnVar_;
        lowerVar = currentReturnVar_;
    }
    if (isClassModule_ && classMemberVars_.count(lowerVar) && !knownLocalVars_.count(lowerVar)) {
        cName = "me->" + cName;
    }
    // Fix 010r-6 rev2: ByRef array param in ReDim needs (*name) since it's vb6_SafeArray1D**
    // Fix 084o-6: ByRef Variant 参数也是 vb6_VARIANT*, 同样需要 (*name)
    // (VB6 允许 As Variant 参数后接 ReDim 变数组, 如 cZipArchive.Extract 的
    // OutputTarget As Variant → ReDim OutputTarget(...) As Byte)
    if (currentProc_) {
        for (auto& param : currentProc_->params) {
            std::string paramLower = param.name;
            std::transform(paramLower.begin(), paramLower.end(), paramLower.begin(), ::tolower);
            if (paramLower == lowerVar && !param.isByVal
                && ((static_cast<uint16_t>(param.type) & static_cast<uint16_t>(Vb6Type::Array))
                    || param.type == Vb6Type::Variant)) {
                cName = "(*" + cName + ")";
                break;
            }
        }
    }
    // Fix 061: With块内 ReDim .Data(...) → _vb6_with_N.Data
    // parser 在 varName 前加了 '.' 前缀表示 With 成员引用 (e.g. ".Data")
    if (node.varName.size() > 1 && node.varName[0] == '.'
        && !withObjectVars_.empty() && !withObjectInfoStack_.empty()) {
        std::string memberName = node.varName.substr(1);  // strip leading '.'
        const auto& info = withObjectInfoStack_.back();
        if (info.kind == WithObjKind::Unknown) {
            // Fix 081j-2: UDT With block 临时变量是指针，用 -> 访问成员
            cName = withObjectVars_.back() + "->" + cIdent(memberName);
        } else if (info.kind == WithObjKind::ClassInstance) {
            // Class With block: _vb6_with_N->member
            cName = withObjectVars_.back() + "->" + cIdent(memberName);
        }
    }
    Vb6Type elemType = resolveArrayElemType(node.asType.get());
    std::string saElemType = mapSaElemType(elemType);
    // Bug4-Fix: UDT数组需使用vb6_SafeArrayReDim1D_Udt
    std::string udtCType = resolveArrayUdtElemCType(node.asType.get());
    bool isUdtArray = !udtCType.empty();

    // Fix 084a/090m: ReDim 目标是否为 Variant 数组 — 逻辑见成员函数 isVariantArrayTarget
    auto isVariantArrayVar = [&](const std::string& nm) -> bool { return isVariantArrayTarget(nm); };

    if (node.dimensions.empty()) return;

    int dimCount = (int)node.dimensions.size();

    if (dimCount == 1) {
        // 涓€缁?ReDim (淇濇寔鍘熸湁1D浠ｇ爜)
        auto& dim = node.dimensions[0];
        std::string lBound = "0";
        std::string uBound = "0";
        if (dim.lower) {
            emitExpr(*dim.lower);
            lBound = std::move(lastExpr_);
        }
        if (dim.upper) {
            emitExpr(*dim.upper);
            uBound = std::move(lastExpr_);
        }

        if (node.preserve) {
            // Fix 084a: Variant 数组 ReDim Preserve — 实参提取 SafeArray, 结果包装回 Variant
            std::string callArg = cName;
            std::string assignVal;
            if (isVariantArrayVar(cName)) {
                callArg = "vb6_VariantToSafeArray1D(" + cName + ")";
                assignVal = "vb6_VariantFromValue(vb6_SafeArrayReDimPreserve1D(" + callArg + ", " + lBound + ", " + uBound + "))";
            } else {
                assignVal = "vb6_SafeArrayReDimPreserve1D(" + callArg + ", " + lBound + ", " + uBound + ")";
            }
            c_.emitLine(cName + " = " + assignVal + ";");
        } else {
            // Fix 084a: Variant 数组非 preserve ReDim — 先销毁提取出的 SafeArray, 再包装新数组回 Variant
            std::string destroyArg = cName;
            if (isVariantArrayVar(cName)) destroyArg = "vb6_VariantToSafeArray1D(" + cName + ")";
            c_.emitLine("vb6_SafeArrayDestroy1D(" + destroyArg + ");");
            std::string newVal;
            if (isUdtArray) {
                // Bug4-Fix: UDT数组使用_Udt版本，传入sizeof(UDT类型)
                newVal = "vb6_SafeArrayReDim1D_Udt((int32_t)sizeof(" + udtCType + "), " + lBound + ", " + uBound + ")";
            } else {
                newVal = "vb6_SafeArrayReDim1D(" + saElemType + ", " + lBound + ", " + uBound + ")";
            }
            if (isVariantArrayVar(cName)) newVal = "vb6_VariantFromValue(" + newVal + ")";
            c_.emitLine(cName + " = " + newVal + ";");
        }
    } else {
        // P8.1: 多维 ReDim
        // Use raw variable name for boundsVar (must be a valid C identifier)
        std::string rawCName = cIdent(node.varName);
        if (isClassModule_ && classMemberVars_.count(lowerVar) && !knownLocalVars_.count(lowerVar)) {
            rawCName = "me_" + rawCName;
        }
        std::string boundsVar = "_redim_bounds_" + rawCName;
        c_.emitLine("vb6_SafeArrayBound " + boundsVar + "[] = {");
        c_.indent();
        for (int d = 0; d < dimCount; d++) {
            auto& dim = node.dimensions[d];
            std::string lb = "0", ub = "0";
            if (dim.lower) { emitExpr(*dim.lower); lb = std::move(lastExpr_); }
            if (dim.upper) { emitExpr(*dim.upper); ub = std::move(lastExpr_); }
            std::string trailing = (d < dimCount - 1) ? "," : "";
            // vb6_SafeArrayBound = {lLbound, cElements}
            // cElements = uBound - lBound + 1 (VB6 "0 To 3" has 4 elements)
            c_.emitLine("{" + lb + ", (" + ub + " - " + lb + " + 1)}" + trailing);
        }
        c_.dedent();
        c_.emitLine("};");
        // Fix 084a: Variant 多维数组 ReDim — 实参提取 SafeArray, 结果包装回 Variant
        std::string redimArg = cName;
        std::string wrapBack = "";
        if (isVariantArrayVar(cName)) {
            redimArg = "vb6_VariantToSafeArray1D(" + cName + ")";
            wrapBack = "vb6_VariantFromValue(";
        }
        if (node.preserve) {
            std::string res = "vb6_SafeArrayReDimPreserveND((vb6_SafeArrayND*)" + redimArg + ", " + std::to_string(dimCount) + ", " + boundsVar + ")";
            if (!wrapBack.empty()) res = wrapBack + res + ")";
            c_.emitLine(cName + " = (vb6_SafeArray1D*)" + res + ";");
        } else {
            std::string destroyArg = redimArg;
            c_.emitLine("vb6_SafeArrayDestroyND((vb6_SafeArrayND*)" + destroyArg + ");");
            std::string newVal;
            if (isUdtArray) {
                // Bug4-Fix: UDT多维数组使用_Udt版本，传入sizeof(UDT类型)
                newVal = "vb6_SafeArrayReDimND_Udt((int32_t)sizeof(" + udtCType + "), " + std::to_string(dimCount) + ", " + boundsVar + ")";
            } else {
                newVal = "vb6_SafeArrayReDimND(" + saElemType + ", " + std::to_string(dimCount) + ", " + boundsVar + ")";
            }
            if (!wrapBack.empty()) newVal = wrapBack + newVal + ")";
            c_.emitLine(cName + " = (vb6_SafeArray1D*)" + newVal + ";");
        }

        // 鏇存柊鏁扮粍缁村害淇℃伅
        std::string lower = node.varName;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        arrayDimCounts_[lower] = dimCount;
    }
}

void CCodeGen::visit(EraseStmt& node) {
    for (auto& name : node.varNames) {
        // Fix 084y-5: Erase 目标含成员访问 (With 块成员 .Field, ByRef UDT 参数
        // 数组字段) 时按成员访问展开, 避免 cIdent 把 '.' 替换成 '_' 生成
        // 未声明的单标识符 (_RemoteLegacyNextTrafficKey / uOutput_Buffer → C2065)
        std::string cName = resolveArrayTargetIdent(name);
        // Fix 010r: Add me-> prefix for class member arrays
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (isClassModule_ && classMemberVars_.count(lower) && !knownLocalVars_.count(lower)) {
            cName = "me->" + cName;
        }
        // Fix 090p: UDT 的 As Variant 数组字段 (如 (*uFile).BufferArray) Erase —
        // 裸 vb6_SafeArrayDestroy1D((*uFile).BufferArray) 把 VARIANT 当 SafeArray* →
        // C2440; 用 vb6_VariantClear 释放数组并置 VT_EMPTY (cZipArchive pvVfsSetEof)
        if (isVariantArrayTarget(name)) {
            c_.emitLine("vb6_VariantClear(&" + cName + ");");
            continue;
        }
        // P8.1: 根据维度数选择1D/ND销毁
        auto it = arrayDimCounts_.find(lower);
        if (it != arrayDimCounts_.end() && it->second > 1) {
            c_.emitLine("vb6_SafeArrayDestroyND((vb6_SafeArrayND*)" + cName + "); " + cName + " = NULL;");
        } else {
            c_.emitLine("vb6_SafeArrayDestroy1D(" + cName + "); " + cName + " = NULL;");
        }
    }
}


} // namespace vb6c3
