# C3 编译器错误修复 — 任务交接文档

> 用途：新会话恢复上下文用。新开会话后直接说「读取 C3_FIX_HANDOFF.md 并继续修复」。
> 更新日期：2026-09-02（第 5 节 m_uData 已完成修复验证：588 → 552）

## 1. 项目与目标

- **C3**：VB6→C 转换器（工作区 `c:/Users/vi/Desktop/c3.vb6.pro`）
- **目标**：减少 `vbman/dist/c3-error.log` 中 MSVC 编译错误数
- **进度**：777 → 588 → **552**（最新统计口径为 `: error C` 行数）
- **当前状态**：C2102（`&` 要求左值）/ C2099（初始化值不是常量）已清零；m_uData With 分类已修复（ToolsTlsThunks 74 → 51）

## 2. 构建 / 验证工作流

1. `scripts/build.bat` — 重编 C3.exe
2. `vbman/build_vbman.bat` — 用新 C3.exe 重新生成 VBMAN 的 C 代码并触发 MSVC 编译
   - C3 生成成功时打印新临时目录（如 `113776838989600`），exit 1 是警告所致，属正常
   - C3 失败时自动写 `vbman/dist/c3-error.log`
3. `scripts/_tmp_count.ps1` — 统计错误总数
4. 辅助脚本（按需重写）：`scripts/_tmp_grep.ps1`（模式统计）、`scripts/_tmp_peek.ps1`（看生成代码）
5. **注意**：PowerShell 内联 `$_` 会被转义，复杂逻辑必须写进 .ps1 脚本文件再执行

## 3. 错误演进史

777 → 774 → 748 → 723（VBA 子集常量）→ 700（VBA 全量 + 枚举）→ 638（ReDim/Erase + 枚举）→ 594（C2102 常量取址修复）→ 589 → 588（C2099 静态初始化清零）

## 4. 已完成的修复（Fix 084 系列）

### cgen_expr.cpp
- **Fix 084x**（~1600 行）：优先级 3 `isVarName` 判定补充 cgen 集合检查（knownUdtVars_/knownClassVars_/knownNewVars_/knownObjectVars_/knownTypedComVars_/knownIfaceVars_），修复 `oCallback` 局部类变量被误判为模块名
- **Fix 084z-2**（IdentifierExpr）：`inferClassTypeOfExpr` 支持 PropertyGet 符号返回类实例（如 `pvSocket() As cTlsSocket`）
- **Fix 084z-3**：Fix 033 分支 className 回退优先用 `inferClassTypeOfExpr`
- **Fix 084o-5**（LHS）：函数名引用返回对象的成员访问 → `vb6_ret_Func->Field`
- **Fix 084z-4**（RHS）：对称读取侧（`NewItem.RootItem` → `vb6_ret_NewItem->RootItem`）
- **Fix 084y-6/084y-7**：VBA 模块成员——常量表内联数值（vbCr/vbLf/vbTab/vbCrLf/vbNullString/vbTrue/vbFalse/VarType 数值），函数生成裸 `vb6_<name>`（去 `$` 后缀）；Fix 033 回退补 `symTab_.lookup(fnName)` 全表查询拿 Optional 参数表
- **Fix 084y-8**（~1760 行）：M22 fallback 前加 EnumType 分支——枚举类型.成员有常量值则内联数值，否则 `vb6_enum_<Enum>_<member>`
- **Fix 084aa 系列**：
  - 位置实参 / 具名实参 ByRef：常量（`isConstIdent` 命中 #define 宏）不可取址 → 按形参类型用 `wrapConstArgForByRef` 生成 `(&(TYPE){CONST})` 复合字面量（Variant 按常量自身类型选字段：VT_BSTR/VT_R8/VT_BOOL/VT_UI1/VT_I4）
  - `VarPtr(函数调用)`：调用结果非左值 → `(intptr_t)&(void*){call}` 复合字面量包装（如 `VarPtr(.Glob(0))` 的 COM 属性调用）
  - VarCmp 比较路径（~1104-1186 行）：`vb6_VarCmpLong<fn>(&left, right)` 的左右操作数左值判断排除常量宏；Variant vs Variant 的 `isLvalue` lambda 排除常量

### cgen_stmt.cpp
- **Fix 084y**（~495 行）：P6.7 Property Let/Set 前加 `objClassMatchesProp` 校验（对象类与属性 sourceModule 不一致时跳过属性路径）
- **Fix 084y-2**（~1164 行）：P6.8 同类匹配校验
- **Fix 084o-5**：`vb6_ret_Func->Field`（类返回值成员访问）
- **Fix 084y-5**：ReDimStmt/EraseStmt 改用 `resolveArrayTargetIdent`（With 块成员 → `_vb6_with_N->Field`；ByRef UDT/数组/Variant 参数 → `(*obj).Field`；UDT 变量 → `obj.Field`）
- **Fix 084aa**：静态局部变量初始化必须是编译期常量——`vb6_VariantEmpty()` → `{0}`，`vb6_BSTR_Empty()` → `NULL`

### cgen_base.cpp / cgen.hpp
- `resolveArrayTargetIdent`：ReDim/Erase 目标解析（cIdent 之后）
- `lookupConstSym` / `isStringConstIdent` / `isConstIdent` / `constIdentType` / `wrapConstArgForByRef`（ByRef 常量包装）
- `inferClassTypeOfExpr` MemberAccessExpr 分支字段表未命中时回退 `getClassMethodReturnType`（属性 Get 返回类）

### cgen_decl.cpp
- **Fix 084aa**：模块级标量 Variant 初始化 `vb6_VariantEmpty()` → `{0}`（既有 BSTR → NULL 处理之外）

## 5. 已完成的修复：m_uData 模块级 UDT 变量（Fix 010n 扩展）

### 症状（vbman/dist/ToolsTlsThunks.c，修复前）
1. **C2064 x24**：`m_uData.Pfn(2)` — UDT 固定数组字段被当函数调用（VB6 `Pfn(2)` 应为 C `Pfn[2]`）
2. **C2224 x37**：`_vb6_with_15->LocalCertificates.Item(lIdx)`、`(*uInput).Stack.Remove(1)` — `.Item`/`.Add`/`.Remove` 左侧非结构体（**独立簇，见第 9 节**）
3. **C2440 x13**：`vb6_SafeArray1D* → vb6_VARIANT`、`(void*)m_uData` UDT 值强转

### 根因（已实证，非推测）
`With m_uData`（模块级 UDT 变量）被误分类为 **COM 对象** → 生成 `(void*)m_uData` 强转 + 所有字段访问变成 COM 调用 → C2440/C2224/C2064 连锁错误。
**实证链**（临时调试输出，C3_DEBUG_UDT 环境变量）：
- `[UDTREG] var=m_uData type=UcsCryptoData sym=0x... kind=8 cType=vb6_type_UcsCryptoData` → **注册代码确实执行**，`knownUdtVars_["m_udata"]` 正常写入
- `[WITHUDT] obj=m_udata inKnown=0 tempType=void*` → **但 With 分类时查不到**！

**真正原因**：`knownUdtVars_` 在**每个过程开头被 `clear()`**（M22-fix，cgen_decl.cpp 三处：visit(FunctionDecl)/visit(SubDecl)/visit(PropertyDecl)，位于 67/293/1252 行附近），clear 后仅从 `classUdtMembers_` 恢复——而 `classUdtMembers_` **只对类模块填充**（cgen_base.cpp generate() 开头 `if (isClassModule_)` 分支）。**普通 .bas 模块的模块级 UDT 变量（如 m_uData）在进入过程后永久丢失**，导致过程内 `With m_uData` 分类失败。

### 修复（对称扩展 classUdtMembers_ 机制）
1. **cgen.hpp**：新增成员 `std::unordered_map<std::string, std::string> moduleUdtMembers_;`（小写 var 名 → UDT C 标识符）
2. **cgen_base.cpp** generate() 开头：
   - `moduleUdtMembers_.clear()`
   - `if (!isClassModule_)` 分支：扫描 `module.declarations` 的 VariableDecl（`asType` 为 `SimpleTypeRef` 且 `lookupDotted` 命中 `UserDefinedType`），写入 `moduleUdtMembers_[lower] = "vb6_type_" + cIdent(name)`
3. **cgen_decl.cpp** 三处过程开头恢复处（classUdtMembers_ 恢复行后）各加：
   `knownUdtVars_.insert(moduleUdtMembers_.begin(), moduleUdtMembers_.end());`

### 验证结果（修复前后对比）
- ToolsTlsThunks.c：`vb6_type_UcsCryptoData* _vb6_with_125 = &(m_uData)`（修复前 `void* _vb6_with_125 = (void*)m_uData`）
- `.hRandomProv` → `_vb6_with_125->hRandomProv`（修复前 `vb6_ComGetIntProp(...)`）
- `m_uData.Pfn[2]`（修复前 `m_uData.Pfn(2)` 函数调用）
- **错误数：588 → 552**；ToolsTlsThunks：74 → 51（C2064 x24 清零、C2440 13→3 均为其他根因）

### 注意事项（踩坑记录）
- **增量构建可能产生陈旧对象 → 运行时 0xC0000409 崩溃**（症状：任何模式跑 VBMAN.vbp 都崩溃，`--dump-symbols` 也在 `=== End Symbol Table ===` 后崩）。本次通过 `cmake --build .build --clean-first` 全量重建解决。**今后改 cgen.hpp 后若运行崩溃，优先怀疑陈旧对象，先 clean 构建**

## 6. 剩余错误分布（552，最新确认 2026-09-02）

按错误码（总数 552）：
- C2440 x122、C2065 x88、C2198 x82、C2224 x66、C2039 x48、C2197 x38、C2088 x32、C2106 x27、C2223 x13、C2186 x7、C2045 x5、C2129 x4、C2037 x3、C2101 x3、C2064 x3、C2083 x2、C2063 x2、C2172 x2、C2296/C2171/C2110/C2059/C2166 各 1
- 已清零：C2102、C2099（以及 ToolsTlsThunks 的 C2064 x24）
- ToolsTlsThunks.c 剩余 51：C2224 x37（COM 集合簇，见第 9 节）+ C2039 x4（MessBuffer_Data 字段缺失）+ C2198 x3 + C2440 x3（SafeArray 簇）+ C2065 x2 + C2101 x1 + C2186 x1

## 7. 关键文件地图

| 文件 | 职责 |
|---|---|
| `src/backend/cgen_expr.cpp` | 表达式生成：IndexOrCallExpr、M22 fallback、ByRef 实参、VarCmp、VarPtr、枚举分支 |
| `src/backend/cgen_stmt.cpp` | 语句生成：With 分类、属性 Let/Set、ReDim/Erase、静态局部 |
| `src/backend/cgen_decl.cpp` | 声明生成：模块级变量 + knownUdtVars_ 注册（903-912） |
| `src/backend/cgen_base.cpp` | 辅助：lookupConstSym/wrapConstArgForByRef/resolveArrayTargetIdent/inferClassTypeOfExpr |
| `src/backend/cgen_util.cpp` | inferClassTypeOfExpr、inferUdtTypeOfExpr（2076+） |
| `src/backend/cgen.hpp` | 声明 |
| `src/driver/driver.cpp` | 跨模块注入（1094-1128，仅 Public） |
| `src/semantics/symbol_table.cpp` | lookup/lookupModule 作用域链 |
| `vbman/src/Socket/mdTlsThunks.bas` | 当前错误源文件（VB6 侧） |
| `vbman/dist/c3-error.log` | MSVC 错误日志（统计源） |

## 8. 会话纪律

- 每次修复后必须走完整工作流（build.bat → build_vbman.bat → _tmp_count.ps1）验证错误数变化
- 不用内联 PowerShell `$_`，写 .ps1 脚本
- 修复按错误簇批量处理，每轮结束后更新本文档第 5/6 节
- **改 cgen.hpp 后运行崩溃（0xC0000409）先怀疑陈旧对象 → `cmake --build .build --clean-first` 全量重建**

## 9. 下一步调查：ToolsTlsThunks C2224 x37（COM 集合簇）+ SafeArray 簇

### 症状（生成代码）
1. **C2224 x37**：`_vb6_with_15->LocalCertificates.Item(lIdx)`、`(*uInput).Stack.Remove(1)` — `.Item`/`.Remove` 左侧非结构体
2. **C2039 x4**：`"MessBuffer_Data": 不是 "vb6_type_UcsTlsContext" 的成员`（1786 行）
3. **C2198 x3 / C2440 x3**：`vb6_SafeArrayDestroy1D(...)` 参数太少、`vb6_SafeArray1D* → vb6_VARIANT`（758 行）

### 已确认的根因（C2224）
- VB6 侧：`UcsTlsContext` UDT 内字段 `LocalCertificates As Collection`、`Stack As Collection`（**UDT 内嵌 COM 对象字段**）
- C 侧：UDT 结构体中该字段生成 `void*`；访问 `.Item(...)` 时被当**结构体字段+函数调用**，而非 **COM 方法调用**
- C3 已有 COM 调用生成（`vb6_ComGetIntProp`/`vb6_ComCall*`，用于局部 COM 变量与 With 块 COM 对象），但 **UDT 内嵌 void* 字段上的方法调用未走 COM 路径**

### 建议方向（未实施）
- 在 MemberAccessExpr/IndexOrCallExpr 生成时，若接收者表达式推断为 `void*`（COM 指针，如 UDT 内嵌 Collection/类字段），成员访问走 COM 调用路径（`vb6_ComGetIntProp(obj, L"Item")` 等）
- 需先梳理 cgen_expr.cpp 中 COM 调用判定条件（knownObjectVars_/knownTypedComVars_ 等），确定「UDT 字段类型为 void*」的识别点
- C2039/C2198/C2440（SafeArray 簇）独立，涉及 `MessBuffer_Data` 字段缺失与 SafeArray 参数包装，可另起一轮
