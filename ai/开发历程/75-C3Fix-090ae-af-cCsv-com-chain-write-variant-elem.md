# C3Fix - cCsv COM 链式索引写 + Variant 数组元素对象成员 (090ae/090af)

**日期**: 2026-09-07
**里程碑**: 090ae（P25b 提取 SetStmt 共用）+ 090af（Variant 数组元素后期绑定）
**回归**: 107* → 103*（cCsv 4 错单模块清除；全量被 C3 崩溃阻塞）
**提交**: 54e2258

## 现象与根因

cCsv.cls 生成 C 4 错集中于两形态：

### 形态 1: `Set Data(LineNumber)(ColumnNumber) = Dat`（prop_set_Value 内）
- 生成 `vb6_VariantFromComResult(vb6_ComCall(vb6_ComCall(me->Data, L"Item", ...), L"Item", ...)) = vb6_VariantToObjectVal(Dat); /* Set */`
- C2197/C2440：LHS 是链式**读**值（非左值）。
- 根因：P25b（链式 COM 默认属性索引赋值，090e 时代建）**只存在于 AssignmentStmt**；SetStmt 没有该分支，目标被 emitExpr 按读生成。

### 形态 2: `If TypeOf d(0) Is cJson Then Set d(0) = d(0).Root`（NewLine 内）
- 生成 `VB6_SA_AT(vb6_VARIANT, d, 0) = vb6_VariantToObjectVal(VB6_SA_AT(vb6_VARIANT, d, 0).Root); /* Set */`
- C2039/C2198/C2440：`.Root` 被当 vb6_VARIANT 结构体字段访问（该结构无 Root）。
- 根因 a：MemberAccessExpr 对「Variant 数组元素」（VB6_SA_AT(vb6_VARIANT,...)）无对象处理 → 通用 fallback 拼 `.member`。
- 根因 b：SetStmt 038b-6 对 RHS Variant 一律 `vb6_VariantToObjectVal` 提取对象；051 缺「Variant 数组元素 LHS」判定 → void* 赋 vb6_VARIANT 元素。

## 修复

### 090ae
1. P25b 主体从 AssignmentStmt 内联块提取为 `CCodeGen::tryEmitChainedComWrite(Expr* target, Expr* value)`（cgen_util.cpp），返回是否命中。
2. AssignmentStmt 原位置与 SetStmt（LHS emitExpr 前）都调用。
3. rootIsCom25 判定扩展：局部注册表（knownObjectVars_/knownVariantVars_/knownTypedComVars_）不覆盖类字段 → 增查 `classVoidFieldMap_`（driver 预扫描，键=moduleName_，集合=字段名小写含 m_ 变体），`Data`（As New Dictionary 字段）命中 → rootIsCom25。
4. 生成：`vb6_ComSetPropArg(vb6_ComCallObject(me->Data, L"Item", {Line},1), L"Item", {Col},1, vb6_ComPackValue(Dat))`。
5. cgen.hpp 声明 + 文档注释。

### 090af
1. cgen_expr.cpp visit(MemberAccessExpr)：obj 文本以 `VB6_SA_AT(vb6_VARIANT,` 开头 → 设后期绑定 marker（comObjExpr_ = vb6_VariantToObject(&(elem))，isComMarker_），lastExpr_ = `vb6_VariantFromComResult(vb6_ComGetProp(..., L"member"))`——与 knownVariantVars_ 标识符路径（P24-04）同构。VB6 语义正确：Variant 值成员访问恒为对象后期绑定（Variant 不能装 UDT）。
2. cgen_stmt.cpp SetStmt：targetIsVariant 判定上移共用并扩展 `VB6_SA_AT(vb6_VARIANT,` LHS；038b-6（RHS Variant → ToObjectVal）对 Variant 容器跳过；051（FromValue 包装）统一应用——对 Variant RHS `vb6_VariantFromValue` _Generic 走 Identity，直接拷进容器元素。

## 验证
- cCsv.cls 单模块 0 error（原 4 错全清，裁剪缺依赖 exit=1 正常）。
- 回归：Dictionary.cls（090ad 路径）、cIni.cls（090e PropertyGet 默认成员链写路径）0 error，判定扩展无破坏。

## 备注
- Variant 数组元素后期绑定用 vb6_ComGetProp（PropertyGet 读）；若目标对象（cJson 工程类）无 IDispatch，运行期行为受限——但 cJson.Root 内部返回 Dictionary（外部 COM，Dispatch 支持），编译正确性为本轮目标。
- 后续若遇 Variant 数组元素上带参方法调用（d(0).Foo(x)），下游 IndexOrCallExpr 会消费 comObjExpr_ marker（既有机制）。

## 残留（下轮候选）
1. C3 崩溃 bug（阻塞全量回归，bisect 方案见 HANDOFF）
2. cAliyunCaptcha `With .ReturnJson()` 字段函数化
3. cHttpServerResponse VBMAN.Version 参数形状 / `->socket`
4. cCollection prop_get_Item x1、C2440 SafeArray* ↔ vb6_VARIANT 各族
5. C2440 各类方向 30+（每类 1-7 处）
