# C3Fix - Variant 边界修正批 (090u/v/w/x/y/aa/ab/ac: 132 → 109)

**日期**: 2026-09-07
**里程碑**: 090u/v/w/x/y/aa/ab/ac
**回归**: 全量 vbman 132 → 109（C2440 73→46）
**提交**: 5273415（090v）、29035e2（090w/090ab）、51e57e2（090aa/x/u/ac）、dfaa29e（Demo）、b8ebae5（error log）

## 问题背景

132 阶段剩余错误从「cZipArchive/Demo 大簇」转为散点。本批根因共同指向
**Variant 判定过宽 / 类指针·UDT 字段漏分支 / App 属性缺入口**，每类 1-6 个错误：

```
Dictionary.c(110/162)  C2197 prop_let_key 参数太多（遗留，下批）
cTlsSocket.c(200)      C2440 函数 vb6_VARIANT → BSTR（MsgBox 二次包裹族）
cTlsSocket.c(4093)     C2440 初始化 vb6_VARIANT → void*（For Each 类元素族）
COMSTAT.fBitFields     C2440 int32 → vb6_VARIANT（UDT 字段误判 Variant 族）
(有些模块)             C2059/C2198 (void*)0.HelpFile（App.HelpFile 无入口族）
```

## 根因与修复

### 1. App.HelpFile 无入口 → `(void*)0.HelpFile`（090ac）

`App.HelpFile`（vbman 中多处用于错误/事件描述拼接）不在 MemberAccessExpr 的 App
分支 → 落到 `(void*)0.HelpFile`（nullptr 解引用非法语法）→ C2059/C2198。
修复：
- vb6rtl.c/h：新增 `vb6_App_HelpFile()` 返回空串 BSTR（产物无 App COM 对象，VB6
  语义为 EXE 名.hlp，空串可编译且等价于无帮助文件）
- cgen_expr.cpp：MemberAccessExpr App 分支（`helpfile` → `vb6_App_HelpFile()`）、
  wrapToBSTR 防二次装箱、IndexOrCallExpr BSTR 前缀表登记
- cgen_stmt.cpp：CallStmt/PrintStmt/WriteStmt 的 isBstrExpr 前缀表同步登记

### 2. 模块级 As New 变量被局部同名冲刷（090v）

`knownNewVars_` 在 Sub/Function/Property 入口被无条件 `clear()`（Fix 010o 清局部），
模块级 `Dim X As New cls` 的注册随之丢失 → 后续函数内成员访问漏 cast 或误注入。
且若某函数内出现同名局部变量（`Dim A As New cCollection` vs 别函数 `Const A`），
残留注册会让 Const A 被注入 auto-instantiate → C2166。
修复：新增 `moduleNewVars_`（模块/类级 As New 持久表），VariableDecl 模块级注册时
双写；函数入口 `knownNewVars_.clear()` 后从 moduleNewVars_ 整体恢复。

### 3. UDT 字段符号不可解析 → 误判 Variant（090ab）

`inferExprType` 对「UDT 已确认但字段未找到」曾返回 Variant（084o-2 为防误匹配模块
同名符号而禁回退模块表）。跨模块 `Public Type` 在符号表不可达时（COMSTAT 定义于
另一 .bas），字段永远「未找到」→ 全部字段被 `isDefinitelyVariantExpr` 判 Variant →
int32 字段实参套 `vb6_VariantToLong(int32)` → C2440。
修复：该路径与新增的「UDT C 类型已知但符号不可解析」兜底均返回 `Unknown`
（Unknown 不会触发 Variant 包装，回退到普通类型路径）。

### 4. For Each 项目类元素走 Variant 分支（090aa）

`For Each oClient In colTimedOut`（oClient As cTlsSocket 项目类）元素提取：
knownClassVars_ 未查 → 落入 Variant 分支 `oClient = vb6_VariantFromStackVARIANT(...)`
→ C2440（vb6_VARIANT → void* 初始化）。
修复：ForEachStmt 元素判定补 `knownClassVars_` 分支，
`(vb6_cls_X*)vb6_ComUnpackObject(&feVar)`（VARIANT 的 IDispatch* → 类指针）。

### 5. MsgBox / Debug.Print 对已转换表达式二次包裹（090u/090x）

- 090u（cgen_expr ~5875）：BSTR 形参打包段对 Variant 实参转 `vb6_VariantToString(...)`
  后，外层 MsgBox 专用逻辑又按「Variant 实参」再包一层 →
  `vb6_VariantToString(vb6_VariantToString(x))`？不——是先转成了 BSTR 结果表达式，
  再包 `vb6_VariantToString(BSTR)` → C2440（BSTR → vb6_VARIANT 入参）。
  修复：`firstArg` 已以 `vb6_VariantToString(` 开头时不再包裹（保持一层）。
- 090x（cgen_stmt Debug.Print）：Variant 表达式落入数值分支
  `vb6_DebugWriteLong((int32_t)(x))` → C2440。修复：Variant 表达式 →
  `vb6_DebugWriteBSTR(vb6_VariantToString(x))`（运行时按字符串输出，贴合 Debug 语义）。

### 6. prop_let_ Variant 末参裸拼标量（090w）

`tryRewriteCOMLvalue` Pattern C/D2 改写（`Set/Let obj.Prop = value` →
`vb6_cX_prop_let_Prop(me, args..., value)`）时，若末形参 `As Variant`
（cJson.Item Dat As Variant ByRef / cCsv.Value ByVal Dat As Variant），裸拼
double/int/BSTR 值 → C2440。
修复：改写目标是 prop_let_ 且非 Set 时，经 findClassMemberCallParams 取末形参类型，
Variant 则打包（ByVal → `vb6_VariantFromValue(v)`；ByRef → `(&(vb6_VARIANT){...})`）。

### 连带

- 090y：With 目标 void*（COM 方法返回对象）兜底语义注释厘清，TODO(090z) 待还原
  ClassInstance 定位崩溃（0xC0000409）。
- Demo.bas：`Users.Decode .Rs` → `Users.Decode (.Rs)`（消解析歧义）。

## 验证

- 全量 VBMAN：132 → 109；C2440 73 → 46；分布转为 64 组、每簇 1-7 个。
- 修复形态（示例生成）：
  - `(vb6_cls_cTlsSocket*)vb6_ComUnpackObject(&_fe_item)`（For Each）
  - `vb6_cCsv_prop_let_Value(me, (&(vb6_VARIANT){vb6_VariantFromValue(vb6_Now())}))`（090w ByRef）
  - `vb6_DebugWriteBSTR(vb6_VariantToString(x))`（Debug.Print Variant）

## 残留（下批候选）

1. **Dictionary.c(110/162) C2197**：`vb6_Dictionary_prop_let_key(me, k, v, vb6_BSTR_Empty())`
   —— Key(old)= 写路径多传一个 BSTR 垫片实参（tryRewriteCOMLvalue 090w 附近疑点）
2. C2440 vb6_SafeArray1D* → vb6_VARIANT x6（数组实参按 BSTR/Variant 形参错装箱）
3. C2440 void* → vb6_VARIANT x5 + = vb6_VARIANT → SafeArray* x3（对象/数组返回赋 Variant）
4. 调用参数太少 18 处（cDataBase_ExecParam/QueryParam、cHttpClient_Inst_OnError 事件族）
5. C2039/C2037 成员族（MessBuffer_Data、IDictionary.Item/Exists、ReturnJson、socket、Value）
