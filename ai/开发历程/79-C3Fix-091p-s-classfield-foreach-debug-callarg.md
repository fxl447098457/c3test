# C3Fix - 类字段 Variant / ForEach COM 源 / Debug 兜底 / ByVal Variant 收 Object 实参 (091p-s)


**日期**: 2026-09-10
**里程碑**: 091p-s
**回归**: 无窗体 125 模块 bisect 42 → **36**（每步单独验证，四次均无回归）
**提交**: ac78b3c(091p) / 36d83a3(091q) / 1c8daf7(091r) / db139c9(091s)

## 现象与修复

### 091p 类字段的 Variant 判定（42→41）
```c
void vb6_cWinsock_prop_set_UserData(vb6_cls_cWinsock* me, vb6_VARIANT Value) {
    me->m_vUserData = vb6_VariantToObjectVal(Value);   /* C2440: void* → vb6_VARIANT */
}
```
VB 源是 `Private m_vUserData As Variant` + `Property Set UserData(ByVal Value As Variant): Set m_vUserData = Value`，
语义上是 **Variant 容器存对象引用**，应生成 `me->m_vUserData = vb6_VariantFromValue(Value);`。

根因与 091m 同源：`knownVariantVars_` 在每个过程开始时 `clear()`，类字段名不在其中 →
`Set` 语句的 `targetIsVariant` 判定失败 → 走"typed 指针 ← Variant 值"分支做 `VariantToObjectVal` 提取。

091m 的回灌只覆盖**标准模块级变量**（类字段回灌裸名会产生 C2065）。本项改为：
- 新增 `classVariantFields_`：类模块/窗体字段声明时登记（`currentProc_==nullptr && isClassModule_`）；
- `Set` 判定中，仅当 `target` 以 `me->` 开头时查该集合（前缀明确，不污染裸名集合）。

### 091q ForEach 的 COM 集合源（41→40）
```c
void* _fe_enum_0 = vb6_ForEach_Init(
    vb6_VariantToObjectVal(vb6_ComCall(me->LangInfo, L"Item", (void*[]){...}, 1)));  /* C2440 */
```
`vb6_ComCall/vb6_ComCallObject` 返回值**就是对象指针**（void*），而 `isDefinitelyVariantExpr`
把"COM Item 调用"视为 Variant → 落到 `VariantToObjectVal` 分支二次提取。
修复：在 `visit(ForEachStmt)` 的 COM 分支加 `collIsObjPtr091q`（`vb6_ComCall(`/`vb6_ComCallObject(` 前缀），
两个提取分支同时排除。

### 091r Debug.Print 的 Variant 变量兜底（40→38）
```c
x = vb6_VariantFromStackVARIANT(&_fe_var_21);       /* For Each 循环变量 x As Variant */
vb6_DebugWriteLong((int32_t)(x));                   /* C2440 + C2198: DebugWriteLong 参数太少 */
```
Debug.Print 的参数类型判定只有 `cExprIsVariant`（C 表达式前缀级），**不认"Variant 变量名"**。
新增 `isVariantVal091r` lambda：`cExprIsVariant` ∨ `knownVariantVars_` ∨ `classVariantFields_`
（统一处理 `me->` 前缀、`(*param)` 解引用、生成器附加的 `/* ... */` 注释后缀），
命中即用 `vb6_DebugWriteBSTR(vb6_VariantToString(x))`。

### 091s ByVal Variant 形参收 Object 实参（38→36）
```c
vb6_ToolsJsonVba_ConvertToJson(Json, ...)   /* Json As Object (void*), 形参 ByVal JsonValue As Variant */
                                            /* C2440: void* → vb6_VARIANT */
```
Fix 084d 为避免"符号表把类指针形参错记为 Variant"而设的跳过条件过宽：
`!inferClassTypeOfExpr(arg).empty()` 对 **Object/COM 变量**也为真 → 不打包。两步收窄：
1. 只有 `symTab_.lookup(类名)->kind == SymbolKind::Class`（项目类）才跳过；
2. 若实参是 `knownObjectVars_/knownTypedComVars_` 命中的 Object/COM 变量（`inferClassTypeOfExpr`
   可能返回内建 "Object" 类名导致误判），则**强制打包** `vb6_VariantFromValue(arg)`。

## 教训
- **"名字集合在过程开始时被 clear()"是本轮反复出现的坑**：091l→091m（模块级）、091p（类字段）。
  凡是"变量名 → 类型/语义"的判定集合，都要明确回答"过程切换后它还可见吗"。类字段因为访问形态
  固定为 `me->field`，最适合单独成集合而不是回灌裸名。
- **同一语义有多个表达载体**：`vb6_ComCall`（对象指针）vs `vb6_ComGetProp`（Variant）；
  判断"能不能再提取"要看**载体**而不是"这是不是 COM 调用"。
- **类型判定要有"名字级"兜底**：C 表达式前缀匹配（`cExprIsVariant`）覆盖不了裸变量名，
  引入变量集合 + 统一的"剥前缀/剥注释"预处理，比在每个调用点重复写解析逻辑可靠。
- **宽跳过条件是债务**：084d 的"类对象实参跳过打包"救了当时代码，但把 Object/COM 变量一起豁免了；
  修复方式是**把条件收窄到语义精确的集合**（项目类）并给出替代路径，而不是再加一层特例。

## 残留（36）与下一步
见 `C3_FIX_HANDOFF.md`：跨模块方法调用漏传 me（C2198×3，cLayer/cLogs/cHttpServerResponse）、
窗体全局对象名未解析（C2065×5，cLogs 的 FLogs）、Variant→具体类型提取（C2440×6，
cDialog/cAesCBC/cToolsList/cHttpClient/Demo_Database）、BSTR 拼接误用指针算术（cToolsHttp 280）、
RTL 签名不匹配（ToolsTlsThunks 1780/5468）。
