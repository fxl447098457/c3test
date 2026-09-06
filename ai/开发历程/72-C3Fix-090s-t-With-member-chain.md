# C3Fix - With 块成员链解析 (090s/090t: 嵌套类字段推断/COM 字段分类/Optional 补齐)

**日期**: 2026-09-06
**里程碑**: 090s/090t
**回归**: 全量 vbman 142 → 132（C2440 79→73）；Demo 簇 21 → 15
**提交**: 126d9ac（代码）、24d13dd（HANDOFF + vbman/dist/c3-error.log）

## 问题背景

Demo.bas（综合演示模块，引用 30+ 工程类）编译错误集中在 With 块成员链：5 类根因 6 个错误行级：

```
Demo.c(99)  C2039: .Item("rollNo") = v — With Http.RequestDataQuery (As New Dictionary
           = COM void* 字段) 被当 vb6_cls_Dictionary* 类实例 → 结构体字段调用
(104)       C2172: .Root("data")("coatingWeight") 嵌套 ComCall 对象参数传值
(142)       C2198: .Router.Reg "Test", New dHttpSvr — 实参全丢
(144)       C2198: .Start — 4 个 Optional 参未补
(502)       C2198: .CBC.Encode("fdsfds") — 实参全丢
```

## 根因与修复（090s 三处协同 + 090t）

### 1. inferClassTypeOfExpr(WithMemberExpr) 返回字段类（cgen_util.cpp）

`.Router.Reg`：With 目标 cHttpServer，`.Router` As cHttpServerRouter（typed 项目类字段）。
推断曾返回 With 目标类（cHttpServer）→ 外层 findClassMemberCallParams(cHttpServer, "Reg")
解析失败 → 用户实参全丢，只剩 this（C2198）。修复：memberName 命中
classTypedFieldMap_[With类] 时返回**字段类**（与 MAE 分支对齐）；COM:/void* 字段返回空；
未命中字段表维持原 With 类（避免误伤属性场景）。

### 2. WithStmt MAE 目标按宿主类字段表分类（cgen_stmt.cpp WithStmt）

`With Http.RequestDataQuery`：member 不在 knownClassVars_ → memSym 全局 lookup
捡到 Dictionary COM 符号 → ClassInstance + vb6_cls_Dictionary*（tempType）→
`.Item(k)=v` 生成 `_vb6_with_1->Item(...)=...`（C2039，Dictionary struct 无 Item）。
修复：MAE 目标先查宿主类（inferClassTypeOfExpr(memExpr.object)）字段表：
- classVoidFieldMap_ 命中 → WithObjKind::COMObject（dispatch）
- classTypedFieldMap_ 命中 → COM: 前缀 → COMObject；项目类 → ClassInstance + className=字段类
memSym 兜底加 `withInfo.kind == WithObjKind::Unknown` 保护（不再覆盖已解析分类）。
COMObject With 内 `.Item(k)=v` 自动走既有 COM marker → vb6_ComSetPropArg ✓。

### 3. visit(WithMemberExpr) asCallCallee_ 协议化（cgen_expr.cpp）

With 内无括号类方法调用（`.Start`）：此前生成完整 `funcName(tempVar)` → CallStmt
bare-call 分支因 callExpr 含 '(' 跳过参数补齐 → 声明带 Optional 的方法只传 this
（C2198）。修复：asCallCallee_ 时交付 `pendingChainObj_ = "(void*)"+tempVar` + 裸函数名
（同 MAE Fix 015/088b 协议）；CallStmt bare-call 链新增 WithMemberExpr 形参解析分支
（withObjectInfoStack_.back() + findClassMemberCallParams）→ Optional padding →
`vb6_cHttpServer_Start((void*)_vb6_with_3, &(int32_t){80}, &(BSTR){0}, 0, 0)`。

### 连带修复（推断类修正自动生效）

- `.CBC.Encode("fdsfds")`（字段链方法带参）：外层 MAE 按字段类解析 → 实参补齐 ✓
- `.Root("data")("coatingWeight")` 嵌套 COM 链读：Fix 086 嵌套对象参数处理
  （ComCall 对象参数直接取 prop_get 调用）自动正确 ✓

## 验证

- 全量 VBMAN：142 → 132（Demo 21 → 15；Demo 中 99/104/142/144/502 五类错误行级清零）
- 修复形态验证（Demo.c）：COM 字段 With 声明 `void* _vb6_with_1 = (void*)Http->RequestDataQuery`；
  `.Item(k)=v` → `vb6_ComSetPropArg(_vb6_with_1, L"Item", (void*[]){...}, vb6_ComPackBSTR(...))`

## 备注（Demo residual 15，下轮专项）

1. `(513)` C2106：`With ini.Section("App")` 嵌套 With（属性返回类实例）→ void* With 目标
   `.Item(k)=v` 误生成 vb6_cTimers_Item(...)=（全局符号撞名 cTimers.Item）
2. `(534)(550)(600)(601)` C2440：MsgBox 类方法 String 返回被双包 VariantToString
   （cRegedit.FindFirst As String → BSTR 当 VARIANT）；UDT 数组元素字段 BSTR 写
3. `(681)` C2440/C2198：Debug.Print 链
4. `(697)` C2166：左值指定 const
5. `(721)` C2198+C2039：`.Rs` 实参（cDataBase COM 字段传参）Decode 参数太少
6. `(778)(798)(869)(871)` C2440：double/int → VARIANT*（字面量参数打包、跨模块 ConvertToJson）
