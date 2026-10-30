#include "backend/cgen.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <functional>

namespace vb6c3 {

// --- cgen_select.cpp: Select Case 语句生成 ---

// 账 #305 第311刀: 「Case 标签是不是全部落在串档」的**唯一**回答处（声明在
// detail/util/cgen_helpers.inc）。契约：一个标签都不许漏 —— `Case Is > x` 的右操作数、
// `Case a To b` 的两端都是标签；`Case Else` 不在 node.cases 里，不用管。
// 拿不到标签（value 为空）或任何一档答不出 String ⇒ false，**宁可退回现行为也不猜**。
bool CCodeGen::selectCaseLabelsAllString(SelectCaseStmt& node) const {
    bool sawLabel = false;
    for (auto& clause : node.cases) {
        for (auto& cv : clause->values) {
            if (!cv.value || !TypeSystem::isString(inferExprType(*cv.value))) return false;
            if (cv.toValue && !TypeSystem::isString(inferExprType(*cv.toValue))) return false;
            sawLabel = true;
        }
    }
    return sawLabel;
}

void CCodeGen::visit(SelectCaseStmt& node) {
    // 推断测试表达式的类型
    Vb6Type testType = inferExprType(*node.testExpr);
    bool isStringSelect = TypeSystem::isString(testType);
    bool isFloatSelect = TypeSystem::isFloat(testType);

    // 账 #305 第311刀「实施」= 主语静态答 **Variant** 时，承载档改由 Case 标签回答。
    // VB6 对 Variant 主语的 Select Case 是按 Case 项的类型逐项做 VarCmp；我方以前只问
    // inferExprType 一档 ⇒ 答 Variant 就落 int32_t 档 + vb6_VariantToLong 解包，而标签
    // 是串 ⇒ 发成 `int32_t == vb6_BSTR_FromStr(L"…")`（C 模式只警告不报红），**每一档都
    // 假** ⇒ 恒落 Case Else。靶子 = ComCtlsDemo 工具栏 `Select Case Button.Tag`
    // （TbrButton.cls:73 `Property Get Tag() As Variant`）：点 "Show ListBoxW1" 该让
    // SysListView32 隐身、ListBox 上位，实测只弹了一句 "clicked button 5"、零枚窗口翻转。
    // 只放开「标签**全部**是串档」这一形；混合标签（同一条里既有 Case "a" 又有 Case 1）
    // 留在现状 = 需要逐项 VarCmp，本刀不做，已记开口。
    if (!isStringSelect && !isFloatSelect && testType == Vb6Type::Variant
        && selectCaseLabelsAllString(node)) {
        isStringSelect = true;
    }

    emitExpr(*node.testExpr);
    if (isComMarker_) {
        // Fix 161d: 解锁类型必须与下面 tempType **同源**。原先这里是无参调用 ⇒ 走
        // resolveComValue 的默认参数 "BSTR" ⇒ vb6_ComGetStringProp 返回 wchar_t*,
        // 而 tempType 对非字符串测试是 int32_t ⇒ `int32_t v = (wchar_t*)ptr` ——
        // **裸指针被当数值**(x86 截断成低 32 位打出 -1916936344 一类值; x64 是
        // C4047 warning)。这就是 ai/029:429 记的"未登记控件属性按数值读漏裸指针",
        // 只有**未登记**属性中招: 已登记属性走 getControlPropReadFn 的专属 getter
        // (返回 int32_t), 根本不进这个 marker 分支。
        // 同族的两个先例: Fix 092n (For 的 start/end/step) 与 Fix 092r (UnaryExpr),
        // 口径一致 —— 按目标类型解锁, 不按"默认最通用"解锁。
        // 注: 未登记属性的 inferExprType 兜底是 Variant ⇒ 上方两档均为假 ⇒ 落 Long 分支,
        // 与 tempType=int32_t 对齐; 读不到值时 getter 给 0。**第311刀之后**多一条岔口：
        // 同一条 Select 的 Case 标签若是**全部**串档，承载档已按标签改走 BSTR，这里跟着
        // 一起答 BSTR —— 两条问句读的是同一个 isStringSelect，不许各抄一份。
        if (isStringSelect)      resolveComValue("BSTR");
        else if (isFloatSelect)  resolveComValue("Double");
        else                     resolveComValue("Long");
    }
    /* Fix 296 / 4b: `Select Case Extender.Align` 的判据临时量也走实例入口
       (ToolBar.ctl 发成 `int32_t _vb6_select_10 = vb6_Extender_Align;`, 实测 1 处)。
       换道落在**临时量的初始化**这一处 —— 下游 cond / switch 读的都是 tempVar,
       所以 getter 只调一次, 不会每个 Case 调一遍。 */
    std::string testVar = extenderMemberReadExpr(lastExpr_);

    // 为test创建临时变量
    std::string tempVar = "_vb6_select_" + std::to_string(tempCounter_++);
    c_.emitLine("{");
    c_.indent();

    // 根据测试表达式类型选择临时变量类型
    std::string tempType;
    if (isStringSelect) {
        tempType = "BSTR";
    } else if (isFloatSelect) {
        // Fix 136: Single 测试必须按 **float** 承载并参与 Case 比较。
        // 之前临时变量用 double: `Select Case iStep: Case Is = 0.2 * nDec`
        // (ucChartBar 轴步长) 左边 0.2f=0.20000000298…, 右边 double 0.2 →
        // 永不相等 → Case 永不命中 → nDec 循环 26 万次不退出 (白屏死循环)。
        // VB6 两边都是 Single, 相等。Case 侧表达式也 cast (float)。
        tempType = "float";
    } else {
        tempType = "int32_t";
    }

    // 声明并初始化临时变量, 保存测试表达式的值
    // Fix 038b-6: 如果测试表达式是 Variant 但临时变量是具体类型, 插入提取函数
    // 仅使用 cExprIsVariant (C 字符串级) 和 knownVariantVars_ 检测.
    {
        std::string initExpr = testVar;
        bool testIsVariant = cExprIsVariant(testVar);
        if (!testIsVariant && node.testExpr && node.testExpr->kind == ASTNodeKind::IdentifierExpr) {
            auto& id = static_cast<IdentifierExpr&>(*node.testExpr);
            std::string idLower = id.name;
            std::transform(idLower.begin(), idLower.end(), idLower.begin(), ::tolower);
            if (knownVariantVars_.count(idLower)) testIsVariant = true;
        }
        if (testIsVariant && !isStringSelect && !isFloatSelect) {
            // int32_t temp = Variant → vb6_VariantToLong(Variant)
            initExpr = "vb6_VariantToLong(" + testVar + ")";
        } else if (testIsVariant && isStringSelect) {
            // BSTR temp = Variant → vb6_VariantToString(Variant)
            initExpr = "vb6_VariantToString(" + testVar + ")";
        } else if (testIsVariant && isFloatSelect) {
            // double temp = Variant → vb6_VariantToDouble(Variant)
            initExpr = "vb6_VariantToDouble(" + testVar + ")";
        }
        c_.emitLine(tempType + " " + tempVar + " = " + initExpr + ";");
    }

    // 用if-else if链代替switch (VB6 Select Case支持范围比较和字符串)
    bool first = true;
    for (auto& caseClause : node.cases) {
        // 构建条件: 同一Case子句的多个值用||连接 (Case 1, 2, 3 → val==1 || val==2 || val==3)
        std::string combinedCond;

        for (size_t vi = 0; vi < caseClause->values.size(); vi++) {
            auto& cv = caseClause->values[vi];
            std::string cond;

            if (cv.isIsClause) {
                // Case Is > 0 → tempVar > 0 (比较符在 cv.relOp, 右操作数在 cv.value)
                if (cv.hasRelOp && cv.value) {
                    emitExpr(*cv.value);
                    std::string rightVal = std::move(lastExpr_);
                    if (isStringSelect) {
                        // 字符串比较: vb6_StrCmp(tempVar, rightVal) op 0
                        // <vbeclipse>: 本模块 Option Compare Text → 恒文本入口
                        cond = std::string(optionCompareText_ ? "vb6_StrCmpT(" : "vb6_StrCmp(") + tempVar + ", " + rightVal + ") " + mapBinaryOp(cv.relOp) + " 0";
                    } else if (isFloatSelect) {
                        // Fix 136: Single 语义 — Case 侧也按 float 求值
                        cond = "(float)" + tempVar + " " + mapBinaryOp(cv.relOp) + " (float)(" + rightVal + ")";
                    } else {
                        cond = tempVar + " " + mapBinaryOp(cv.relOp) + " " + rightVal;
                    }
                } else {
                    // Case Is (无比较符) → 非零/非空
                    if (isStringSelect) {
                        cond = tempVar + " != NULL && " + tempVar + "[0] != 0";
                    } else {
                        cond = tempVar + " != 0";
                    }
                }
            } else if (cv.toValue) {
                // Case 1 To 10 → tempVar >= 1 && tempVar <= 10
                emitExpr(*cv.value);
                std::string lo = std::move(lastExpr_);
                emitExpr(*cv.toValue);
                std::string hi = std::move(lastExpr_);
                if (isStringSelect) {
                    // 字符串范围比较: wcscmp >= lo && wcscmp <= hi
                    cond = std::string(optionCompareText_ ? "vb6_StrCmpT(" : "vb6_StrCmp(") + tempVar + ", " + lo + ") >= 0 && "
                           + (optionCompareText_ ? "vb6_StrCmpT(" : "vb6_StrCmp(") + tempVar + ", " + hi + ") <= 0";
                } else if (isFloatSelect) {
                    // Fix 136: Single 语义范围比较
                    cond = "(float)" + tempVar + " >= (float)(" + lo + ") && (float)" + tempVar + " <= (float)(" + hi + ")";
                } else {
                    cond = tempVar + " >= " + lo + " && " + tempVar + " <= " + hi;
                }
            } else {
                // 精确匹配
                emitExpr(*cv.value);
                if (isStringSelect) {
                    cond = std::string(optionCompareText_ ? "vb6_StrCmpT(" : "vb6_StrCmp(") + tempVar + ", " + lastExpr_ + ") == 0";
                } else if (isFloatSelect) {
                    // Fix 136: Single 语义精确匹配
                    cond = "(float)" + tempVar + " == (float)(" + lastExpr_ + ")";
                } else {
                    cond = tempVar + " == " + lastExpr_;
                }
            }

            if (!combinedCond.empty()) combinedCond += " || ";
            combinedCond += "(" + cond + ")";
        }

        if (first) {
            c_.emitLine("if (" + combinedCond + ") {");
            first = false;
        } else {
            c_.emitLine("} else if (" + combinedCond + ") {");
        }
        c_.indent();
        emitStmtList(caseClause->body);
        c_.dedent();
    }

    if (!node.elseCase.empty()) {
        c_.emitLine("} else {");
        c_.indent();
        emitStmtList(node.elseCase);
        c_.dedent();
    }

    if (!first) {
        c_.emitLine("}");
    }
    c_.dedent();
    c_.emitLine("}");
}

void CCodeGen::visit(CaseClause& node) {
    // 由SelectCaseStmt内部处理
}

} // namespace vb6c3
