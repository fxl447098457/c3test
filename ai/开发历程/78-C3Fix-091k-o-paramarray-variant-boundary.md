# C3Fix - ParamArray/Variant 参数边界 / Variant 目标赋值 / 属性 Let 与命名实参适配 (091k-o)


**日期**: 2026-09-10
**里程碑**: 091k-o
**回归**: 无窗体 125 模块 bisect 52 → **42**（每步单独验证；中途两次大回归已定位并修正）
**提交**: e97a90d(091k) / 02205f5(091l+m) / ad62a09(091n) / 96d123a(091o)

## 现象

52 个残余错误里，按生成代码又分出 5 簇，共同点是**"C 侧类型载体已确定，但 cgen 仍按 VB 的 Variant 语义做提取/未做打包"**：

```c
/* 1. ParamArray */
vb6_PA_UBound(vb6_VariantToSafeArray1D(OutVars))        /* C2440: SAFEARRAY*→vb6_VARIANT */
vb6_IsMissing(vb6_PA_GetLong(OutVars, i))               /* warning: 语义错 (RTL 形参 SAFEARRAY*) */
vb6_VariantArrayGet(&Vars, i)                           /* C4047: vb6_VARIANT** vs vb6_VARIANT* */

/* 2. Variant 目标赋值 */
EnumLevelNames = _arr_0;                                /* C2440: SafeArray1D*→vb6_VARIANT */

/* 3. 属性 Let */
vb6_cWinsock_prop_let_UserData(oClient, baBuffer);      /* C2440: SafeArray1D*→vb6_VARIANT */
vb6_cDialog_prop_let_Filter(Rs, vb6_VariantFromComResult(...)); /* C2440: vb6_VARIANT→BSTR */

/* 4. 命名实参 */
vb6_cTlsSocket_Accept((*oListenSocket), &(me->m_oSocket), vb6_BSTR_Empty(), 0, bUseTls, 0, 0, 1);
                                                        /* C2440: int16_t→vb6_VARIANT (UseTls 是
                                                           Optional ByVal Variant) */
```

## 修复

### 091k ParamArray / ByRef Variant 参数边界（52→48）
1. 通用实参类型转换：实参是当前过程的 ParamArray 参数 → **跳过**数组提取（`SAFEARRAY*` 已是载体）。
   - 顺带说明 `vb6_PA_UBound` 改名分支（Fix 084x）本身生效，但改名后仍带被包装的 `args[0]`。
2. `vb6_VariantArrayGet(&v, i)`：ByRef Variant 形参的 C 类型已是 `vb6_VARIANT*`（cgen_util
   参数映射 ByRef→指针），再取址会传二级指针 → 改为裸名。
3. `IsMissing`：
   - RTL `int32_t vb6_IsMissing(SAFEARRAY*)` 是 **ParamArray 专用**（psa==NULL 表示未传实参），
     不是 VBA 的 `IsMissing(Optional Variant)`；
   - 生成侧：ParamArray 名 → `vb6_IsMissing(pa)`；其它复合表达式（数组元素 / ParamArray 元素）
     → 常量 `(0)`（VB6 中非 Optional 实参的 IsMissing 恒 False）；
   - 运行时参数表项由 `vb6_VARIANT*` 修正为 `SAFEARRAY*`（与 RTL 一致）。

### 091l/091m Variant 目标赋值（48→46）
- 091l：赋值路径新增"Variant 目标 ← 具体类型值 → `vb6_VariantFromValue(value)`"（`_Generic`
  宏按实类型选 ctor：标量 / BSTR / SafeArray1D* / 指针）。
- 091m：该判定依赖 `knownVariantVars_`，但它在每个过程开始时 `clear()`，模块级
  `Dim EnumLevelNames As Variant` 因而不在集合内 → 新增 `moduleVariantVars_`：
  模块级声明时登记、三处过程开始回灌。
  **教训**：首版未排除类模块/窗体成员 → 方法体内裸名生成 → C2065 ×7 回归（+4 总数），
  加 `!isClassModule_` 后归零。

### 091n 属性 Let 值参（46→44）
`obj.Prop = value` 的 `prop_let_` 调用此前**完全不打包**。改按 **091a 的写方向参数表**
（`findClassMemberWriteParams`，含类归属校验）取末参类型：
- `Variant` 形参 + **数组载体**值 → `packLetValueArg`（ByRef/ByVal 各自形态）；
- `BSTR` 形参 + Variant 值 → `wrapToBSTR`。

两次大回归（46→82）说明这条路上"形参类型来源"极不可靠：
- 裸 `propLetSym->params` 会被 `lookupModuleByKind` 的**全局同名命中**污染（拿到他类属性的
  Variant 参数表）→ `Rs.Status = 200` 之类被误打包成 `vb6_VariantFromValue(1)`；
- 加 `Vb6Type::Unknown` 兜底同样灾难；
- 最终把打包限定为"值形态确为数组载体"（`vb6_SafeArray1D*` / `_arr_N` / `vb6_Split(` /
  knownArrays_ / knownByteArrayVars_ 命中），既修好 cWinsock 1949/1965，又不碰
  COM 后期绑定属性（`Dictionary.CompareMode = 1`）。

### 091o 命名实参 ByVal 适配（44→42）
`Name:=expr` 的展开路径（named → 位置实参 + Optional 占位 + `_has_` 标志）只调用了
`applyByRef`，漏掉 ByVal 形参的类型适配 → 补 `ByVal Variant → vb6_VariantFromValue`
（类对象实参仍直传指针，同 Fix 084d）。

## 验证
- 每项 `build.bat` + `_tmp_bisect.ps1 -N 125`：52 → 48 → 46 → 44 → 42，逐项无回归；
- 两次中间回归（→82）都用"看新错误形态"定性（`vb6_VARIANT → int32_t` 大量出现在
  cHttpServer/cDataBase/cJson = COM/后期绑定属性被误打包），再收窄条件。

## 教训
- **"符号表说是 Variant"不等于"C 形参是 Variant"**：COM 后期绑定、跨模块同名属性、
  `Vb6Type::Unknown` 三类都会让参数表失真；涉及打包/提取的判定要么用带类归属校验的
  写/读方向表，要么用**值形态**（数组载体/COM 结果/标量）来约束。
- **VB 语义与 RTL 语义要分开看**：`IsMissing`（VB6 Optional 形参）与
  `vb6_IsMissing`（ParamArray 是否整体缺省）是两回事，同名不同义。
- **模块级变量集合的生命周期**：凡在过程开始时 `clear()` 的集合，都要像
  `moduleNewVars_`/`classUdtMembers_` 那样考虑"模块级回灌"，且必须排除类成员。
- 命名实参、可选参数填充、`ParamArray`、属性 Let 各有一条**独立的实参生成路径**，
  通用参数适配（ByRef/ByVal/类型转换）要在每条路径上都补齐，否则错误会以"零散几处"的
  形式长期残留。

## 残留（42）与下一步
详见 `C3_FIX_HANDOFF.md`：ForEach COM 源（cLang 26）、物化临时未登记 knownArrays_（cPLI 56）、
类 Variant 字段 ← COM 结果（cWinsock 172 / Demo 857/859）、COM 属性 Let 的 BSTR 提取
（cToolsList 25）、Debug 强转调用参数量（pvSubClass 543/544，C2198 族）。
