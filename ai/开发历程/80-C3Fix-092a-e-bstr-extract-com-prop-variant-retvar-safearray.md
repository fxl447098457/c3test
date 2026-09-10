# C3Fix - BSTR 提取顶层判定 / COM 属性写 / Variant 返回变量 / ByRef 数组形参 (092a-e)


**日期**: 2026-09-10
**里程碑**: 092a-e
**回归**: 无窗体 125 模块 bisect 36 → **32**（每步单独验证，四次均无回归）
**提交**: d5806d8(092a) / 821e0ea(092b) / 02e49a3(092c) / 8a8eac2(092e)

## 现象与修复

### 092a 子串判断 vs 顶层判断（36→35）
```c
vb6_BSTR_Assign(&me->JsCode,
    vb6_StrConv(vb6_LoadResData(vb6_BSTR_FromStr(L"AES.CBC"), vb6_BSTR_FromStr(L"JSCRIPT")), 64, 0));
/* C2440: 函数: 无法从 vb6_VARIANT 转换为 BSTR */
```
`paramBase == String` 的提取分支用 `argVal.find("vb6_BSTR") == npos` 判定"是否已是 BSTR"。
`vb6_LoadResData(vb6_BSTR_FromStr(..), ..)` 的**内部实参**含该子串 → 整体被当作 BSTR → 跳过
`vb6_VariantToString` 提取。改为与 5080 处同源的**顶层前缀**白名单（只看表达式开头）。

**教训**：`find()` 子串判定在"函数调用实参本身也是该类型表达式"时必然误判；类型判定要看**表达式顶层**。

### 092b COM 属性写被误判为类属性写（35→34）
```c
vb6_cDialog_prop_let_Filter(Rs, vb6_VariantFromComResult(vb6_ComGetProp(...)));  /* C2440 */
```
VB 源 `Rs.Filter = ...`（`Rs As Object`，ADODB Recordset）。`lookupModuleByKind("Filter", PropertyLet)`
全局命中 `cDialog.Filter`；`inferClassTypeOfExpr(Rs)` 为空 → Fix 084g-2 的
"`!objClass.empty() || propLetSym->isExternal`" 直接放行 → 生成类属性调用，既类型不符又丢失 COM 后期绑定语义。
修复：`objClass.empty()` 时若对象是 `knownObjectVars_` / `knownTypedComVars_` 变量 → 跳过属性路径（交回 COM 写）。

**教训**：全局符号查找（`lookupModuleByKind`）必须配合"对象究竟是不是该类实例"的判定；
"类型推断不出来"不等于"就是匹配的那个类"。

### 092c 返回变量不是"Variant 表达式"（34→33）
```c
vb6_VARIANT vb6_ret_ShowOpen = ...;
...
me->mData.mstrFileName = vb6_ret_ShowOpen;   /* C2440: vb6_VARIANT → BSTR */
```
`cExprIsVariant` 只看函数调用前缀，`knownVariantVars_` 只装 **VB 名**；`vb6_ret_<Proc>` 这种
"生成器内部返回变量"两边都不覆盖。加上 `value == currentReturnVar_ && currentReturnCType_ == "vb6_VARIANT"`
判定后走既有"按目标类型提取"逻辑（UDT 字段 String → `vb6_VariantToString`）。

**教训**：生成器自造的表达式形态（返回变量、临时变量）需要单独的识别入口，不能指望源码级判定命中。

### 092e ByRef 数组形参的复合字面量类型（33→32）
```c
BSTR vb6_cToolsUtf8_Decode(vb6_cls_cToolsUtf8* me, vb6_SafeArray1D** Utf);   /* 声明 */
vb6_cToolsUtf8_Decode(ToolsUtf8, (&(uint8_t*){vb6_VariantFromComResult(...)})); /* 调用 → C2440 */
```
Fix 078 rev2 已把 ByRef 数组形参的 C 类型定为 `vb6_SafeArray1D**`，但调用点的复合字面量类型仍取
`mapType(Byte|Array)` = `uint8_t*`。统一按 `vb6_SafeArray1D**` 生成，并补 Variant 提取映射。

**教训**：形参 C 类型的"声明侧规则"（Fix 078）改动后，必须回归检查**调用侧所有生成点**
（复合字面量、提取函数映射、ByRef 取址），否则会出现"声明与调用不一致"的编译错误。

## 本轮副产物
- 判定 **cLogs/cLayer 7 个 C2065/C2198 为无窗体 bisect 假阳性**：`FLogs`/`FLayer` 是 `.frm` 窗体
  （`vb6_FLogs_Visible` 的定义在窗体模块），bisect 编译集不含窗体 → 不应计入修复目标。
- 收敛出"赋值右值 Variant → 目标具体类型"的提取矩阵（cgen_stmt 1160-1230）：
  数组元素（`VB6_SA_AT`）× BSTR/Long/Double、UDT 字段（`inferUdtFieldVb6Type`）、
  返回变量（092c）；下一步可补 `currentReturnCType_` 的 SafeArray1D*/void*/double 分支（cHttpClient 399 等）。

## 残留（32）与下一步
见 `C3_FIX_HANDOFF.md`：C2039×6（成员不存在：IDictionary::Exists、cClientCallback::socket、
cHttpClient::ReturnJson、UcsTlsContext::MessBuffer_Data、SAFEARRAY::data/lBound）、
C2198×6（多为假阳性 + ToolsTlsThunks 1780 / cToolsHttp 280）、C2440×8（cTlsSocket 4093、
cHttpServer 374、cHttpClient 399、cLang 144、cPLI 56、Demo_Database 506、pvSubClass 543/544）、
零散 C2101/C2106/C2110/C2171/C2186。
