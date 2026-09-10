# C3Fix - 属性写方向签名 / 常量类型 / ForEach 数组源 / BSTR 提取 (091a-i)


**日期**: 2026-09-10
**里程碑**: 091a-i
**回归**: 无窗体 125 模块 bisect 65 → **52**（每步单独验证，无回归）
**提交**: aeaa95f(091a) / 05793ac(091c) / ae72bf2(091d) / d8f11aa(091h-i) / 另 091e-g 合并提交

## 现象（起点 65 错）

同一批 C2440"参数/初始化/赋值方向不匹配"里，按生成代码聚成 4 簇：

### 1) 属性 Let 值参不打包（Demo.c 766/767/768）
```c
vb6_Demo_prop_let_Item(me, json, key, double);   /* 应字段式打包 */
```
`memberParams` 读优先序为 Get > Func > Sub > Let > Set。当模块同时存在 Get（带 key 参数）
与 Let 时，跨模块 storageKey 冲突（`item$pl`）导致 Phase A 找不到本类符号 → 回退到 Get
的 `[key]` 参数表 → 属性 Let 的"末参才是 value"规则失效 → 不打包。

### 2) ParamArray 元素赋值（cToolsArray.c 158/167）
```c
vb6_PA_GetLong(OutVars, i) = <Variant>;   /* C2106 取函数作左值 + C2440 Variant→具体类型 */
```
`OutVars(i) = value` 未识别 `vb6_PA_Get*` 目标形态。

### 3) 常量类型被当 Variant（cToolsSystem.c 11/13、cDialog.c 36→532）
```c
const vb6_VARIANT SW_SHOWNORMAL = 1;                       /* C2440 初始化 */
#define BIF_USENEWUI ((vb6_VariantToLong(64) | vb6_VariantToLong(16)))  /* C2440 */
```
- 局部 `Const`（LocalDeclStmt 分支）此前被"简化"为 Variant；
- cgen 用 `mapTypeRef(asType)`（空→Variant），与符号表推断无关；
- 模块级常量表达式因符号类型 Variant 被按 Variant 语义包装操作数。

### 4) ForEach 数组源 / BSTR 提取（ToolsTlsThunks 754/2913/3182、cTlsSocket 200、Demo_Database 310/506/678）
```c
vb6_ForEach_Init(vb6_VariantToObjectVal(vb6_Split(...)));                 /* C2440 */
vb6_LenB(vb6_VariantFromComResult(vb6_ComGetProp(...)));                  /* C2440 */
vb6_BSTR_Concat(L"...", vb6_cDataBase_LastInsertId(TestDB));              /* C2440 */
```
- `For Each` 源只有 IdentifierExpr 才走数组路径，`Split(...)`/`Array(...)` 落 COM 路径；
- `bstrTopPrefixes` 把 `vb6_VariantFromComResult(` 误当"已 BSTR"（该函数实返回 vb6_VARIANT）；
- `wrapToBSTR()` 的"其他 vb6_ 函数假定为 BSTR"直通分支未识别项目 Variant 返回函数。

## 修复

### 091a 属性写方向签名表（65→63）
- `Symbol` 新增 `memberLetParams` / `memberSetParams`（Let/Set 各自唯一，Pass1/Pass2
  无条件填充；driver.cpp 跨模块拷贝）。
- 新函数 `findClassMemberWriteParams(className, member, isSet, out)`：
  Phase A 模块作用域 Let/Set 符号 → Phase B Class 符号写方向表。
- 090w 的 C/D2 打包改用它（不再依赖被 Get 遮蔽的 `memberParams`）。

### 091c ParamArray 元素赋值（63→61）
- 赋值路径新增 `vb6_PA_Get*` 目标识别：改写为
  `vb6_PA_SetLong/Double/BSTR(args, <按类型提取的 value>)`（Variant 值 →
  `vb6_VariantToLong/ToDouble/ToString`）。

### 091d 常量类型推断 + 整型折叠（61→58）
- `LocalDeclStmt` 的 ConstDecl 直接复用 `registerConstant`（完整字面量/一元负号推导，
  define 到局部作用域）；
- cgen_stmt 局部 ConstDecl 无 `As` 时按字面量定 C 类型（Integer/Long→int32_t、
  Single/Double→double、String→BSTR、Boolean→VBABOOL）；
- cgen_decl 模块级 ConstDecl 生成前先 `tryEvalConstInt` 折叠整型表达式 →
  `#define BIF_USENEWUI (80)`。

### 091e-g ForEach 数组源（58→55）
- `rtParamTypeUsable = i >= calleeParams.size() || calleeParams[i].type 是
  Variant/Empty`（放宽运行时参数类型表触发条件）+ 表补
  `{"vb6_StrConv", {"BSTR","int32_t","int32_t"}}`（本项单独验证无变化，保留）。
- ForEach 源为 `Split/Filter`（元素 BSTR）或 `Array(...)`（vb6_ArrayCreate 建
  **Variant 数组**，元素 vb6_VARIANT）时走数组迭代路径；表达式集合先物化到
  `_fe_arrN`，避免 LBound/UBound/`VB6_SA_AT` 重复求值。

### 091h-i BSTR 提取（55→52）
- 删除 `bstrTopPrefixes` 中的 `"vb6_VariantFromComResult("`（返回 vb6_VARIANT，
  vb6rtl.h:918）；
- `wrapToBSTR()` 直通分支前增加 `variantReturnFuncs_` 检查（含
  `vb6_<cls>_prop_get_<name>`）→ 命中则 `vb6_VariantToString(...)`。

### 未采用（回退）091b
对象属性赋值路径值参打包：首版按 `propLetSym->params.back()` 无条件打包 → +36 C2440
（`lookupModuleByKind` 全局同名属性命中错类）；改按 `objClass` 精确查后回归消失，但
ByVal Variant 场景（`UserData Let(ByVal Value As Variant) ← baBuffer`）仍不生效：
`inferExprType` 与符号表均拿不到局部数组位；且枚举类型（`CompareMode As CompareMethod`）
符号表解析为 Variant 而 cgen 生成 int32_t 形参 → 打包即 C2440。已回退并记录。

## 验证
- 每项单独构建 + `_tmp_bisect.ps1 -N 125`（无窗体 125 模块 FIX_bisect.vbp）：
  65 → 63 → 61 → 58 → 55 → 52，逐项无回归（错误总数单调下降或在预期内）。
- 目标文件逐个核对生成行（cToolsSystem/cDialog/Demo/ToolsTlsThunks/cTlsSocket/
  Demo_Database）。

## 教训
- **BSTR 与 VARIANT 的判定必须按 RTL 真实返回类型核对**（`vb6_VariantFromComResult`
  返回 vb6_VARIANT，不是 BSTR）。字符串级前缀表（`bstrTopPrefixes`/
  `variantPrefixes`）是历史补丁堆叠点，新增/删除条目都要跑 bisect 看回归。
- **"假定为 BSTR"这类 fallback 直通**是 BSTR 簇错误的根源；补 `variantReturnFuncs_`
  这类"权威集合"检查比继续堆叠函数名前缀更可持续。
- ForEach 源走数组路径时**必须物化表达式**（否则每次 LBound/UBound/取元素都会重新
  调用 Split/Array，既重复分配又语义错误）。
- 局部 `Const`/局部数组的类型信息在符号表里不可靠（被简化/丢失），涉及打包决策时
  不能只信 `inferExprType`/`symTab_.lookup`。

## 残留（下轮候选，bisect 52）
1. C2198×7 / C2039×6 / C2065×5 等非 C2440 族未细分。
2. **形参为按值 Variant 的运行时函数**（表项 `"vb6_VARIANT"`）实参为具体类型时未包装：
   `vb6_VariantToObjectVal(vb6_ComCall(...))`（cLang 26）、`vb6_Join/PA_UBound(
   vb6_VariantToSafeArray1D(...))`（cPLI 56 / cToolsArray 147）、cWinsock 1604/1949/1965、
   pvSubClass 543/544、cDelay 74。注意 `vb6_CLngV/CIntV` 有 5993-6010 反向剥离后处理。
3. **赋值路径目标类型识别**：ToolsLogs 44（`EnumLevelNames = _arr_0`）、cWinsock 172、
   cHttpClient 399/418、cLang 144、Demo 671、cTlsSocket 4093。
4. **UDT 嵌套字段**（cDialog 393 `me->mData.mstrFileName = <Variant>`）。
5. cAesCBC 25 / cToolsList 25 / cToolsArray 98/107（`vb6_IsMissing` 形参是 SAFEARRAY*，
   表项写成 `vb6_VARIANT*`，需一并核对）。
