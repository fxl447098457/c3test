# C3Fix - COM 类函数返回成员访问 + If Variant 条件 (090ag)

**日期**: 2026-09-07
**里程碑**: 090ag
**回归**: 103* → 100*（ToolsJsonVba 3 错单模块清除；全量被 C3 崩溃阻塞）
**提交**: 368d0ee

## 现象

ToolsJsonVba.bas 生成 C 3 错：

### 1) C2039/C2037 x2（ParseObject：`json_ParseObject.Item(json_Key) = v`）
```c
vb6_ret_json_ParseObject.Item(json_Key) = vb6_VariantFromObject(...);  /* 错 */
```
- `json_ParseObject As Dictionary`（Scripting.Dictionary 项目外 COM）→ 函数返回
  C 类型 `vb6_ComIface_IDictionary*`（非 void*）。
- MAE visit 的 089c2 分支条件只匹配 `currentReturnCType_ == "void*"`
  （As Collection/As Object → void*），vb6_ComIface_* 类型落入 else 的 UDT
  fallback → 生成 `.Item` 结构字段访问 → C2037（vb6_ComIface_IDictionary 前向
  声明不完整，无 Item 成员）。
- 同一函数的 393 行 `json_ParseObject.CompareMode = TextCompare` 走的是
  FnRetObj COM SetProp 前置分支（LHS 纯 MAE）→ 正确（不受影响）。差别只在
  Item 是带 Key 索引的默认成员（LHS 为 IC 包 MAE → emitExpr 路径）。

### 2) C2083（ConvertToJson：`If JsonValue Then`）
```c
if (JsonValue) { ... }  /* vb6_VARIANT struct 不能直接 bool */
```
- JsonValue ByVal As Variant 参数（Variant 值）作 If 条件 → 无 bool 转换。
- IfStmt 原处理只有 isComMarker_ → resolveComValue("Int")，对裸 Variant 标识符
  不做转换。

## 修复

### 089c2 扩展（cgen_expr.cpp visit(MemberAccessExpr)）
```cpp
} else if (currentReturnCType_ == "void*"
           || currentReturnCType_.find("vb6_ComIface_") != std::string::npos) {
    ...
    comObjExpr_ = "(void*)" + currentReturnVar_;
```
- As Dictionary 等外部 COM 类返回的函数体内 `FuncName.xxx` 走 COM dispatch。
- comObjExpr_ 统一 void* 形态（下游 P25 链写/ComCall 消费 IDispatch 语义）。

### IfStmt Variant 条件（cgen_stmt.cpp visit(IfStmt)）
```cpp
if (!isComMarker_) {
    bool condIsVariant = cExprIsVariant(lastExpr_);
    if (!condIsVariant && condition 是 IdentifierExpr && knownVariantVars_) ...
    if (condIsVariant && !hasPrefix("vb6_VariantToBool("))
        lastExpr_ = "vb6_VariantToBool(" + lastExpr_ + ")";
}
```
- VB6 If Variant 真值判定：Empty/0/False → 假。

## 验证
- ToolsJsonVba.bas 单模块 3 → 0 error。
- cCsv.cls 回归 0 error（090ae/090af 不受影响）。
- cZipArchive/cJson 单编错误均为裁剪缺依赖假错（vb6_type_ZipVfsType/FILETIME
  类型定义在依赖模块；ToolsList 模块缺失致 RsToCollection 返回类型未知）——
  全量基线两簇 0 错，非本次改动引入。

## 教训
- 函数返回类型 C 映射有多个形态：内置 COM（void*）、项目外 COM 类
  （vb6_ComIface_*）、工程类（vb6_cls_*，走 088d）、UDT。089c2 只覆盖 void*；
  修 090ag 时把 vb6_ComIface_* 也并入 COM dispatch。后续若遇其它 COM 形态
  （如 vb6_ComIface 指针的其它返回路径）同样并入口。
- 单模块 fix 裁剪对依赖重的模块（cZipArchive/cJson/cLang）会误报，判错须核对
  全量基线；ToolsJsonVba 相对独立（仅引 vb6_Replace 等 rtl）可作干净验证面。

## 残留（下轮候选）
1. C3 崩溃 bug（阻塞全量回归，bisect 方案见 HANDOFF）
2. cToolsArray ParamArray/Variant 数组元素族（6 错，含 PA_Set 缺失、IsMissing、
   Extend(...)= 属性 Let LHS、UBound(OutVars) 误包 VariantToSafeArray1D）
3. cLang/cHttpServerResponse 等依赖重模块（需多模块 fix 才准）
4. C2440 各类方向 30+（每类 1-7 处）
