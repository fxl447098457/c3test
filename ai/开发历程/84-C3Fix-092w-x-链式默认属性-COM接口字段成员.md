# 84 - C3Fix 092w/092x：链式默认属性访问与 COM 接口字段成员访问

> 日期：2026-09-11
> 基线：无窗体 125 模块 bisect **11 → 8**（提交 `ac69fc0` / `e0df780`）
> 结论：**无窗体 bisect 面已收敛** —— 剩余 8 个错误全部为非代码缺陷（窗体假阳性 7 + 配置缺口 1）。

## 背景

延续 092r–v（16 → 11）之后的收敛阶段。残留 11 个错误中：
- 7 个是**窗体假阳性**（cLogs×5、cLayer×2，`.frm` 模块不在 bisect 编译集）；
- 2 个是**真实代码缺陷**（Demo_Database 506、cHttpServer 821）；
- 1 个是**配置缺口**（cHttpServerResponse 452，外部 ActiveX 模块 `Common` 未入 vbp）。

本轮清掉 2 个真实缺陷。

## 092w：链式默认属性访问 `X(a)(b)`（Demo_Database 506）

### 现象

`Demo_Database.bas` 392 行：

```vb
Debug.Print "       更新后的分数: " & TestDB.Rows(1)("score")
```

生成的 C：

```c
vb6_BSTR_Concat(vb6_BSTR_FromStr(L"       更新后的分数: "),
    vb6_cCollection_prop_get_Item(TestDB->Rows  /* class var .Rows field */,
                                  vb6_VariantFromValue(1),
                                  vb6_BSTR_FromStr(L"score")))
```

报 **C2197 实参太多**（签名 2 参，传了 3）+ **C2440**（Variant → BSTR）。

### 根因

`TestDB.Rows(1)("score")` 的 AST 是**嵌套 IndexOrCallExpr**：外层 `("score")` 的 callee 是内层 `TestDB.Rows(1)`。

`cgen_expr.cpp` 的 `visit(IndexOrCallExpr)` 中，当 callee 被拆成「完整调用文本」时（`callee.back() == ')'`），走 Fix 086 的链式默认属性分支：

```cpp
bool innerIsChainedObj = false;
if (funcPart == "vb6_ComCall" || ... ) innerIsChainedObj = true;
else {
    size_t gp = funcPart.find("_prop_get_");
    if (gp != npos) {
        Symbol* propSym86 = symTab_.lookupModuleByKind(propName, PropertyGet);
        if (propSym86 && propSym86->params.empty()) innerIsChainedObj = true;   // ← 仅无参 prop_get
    }
}
```

`prop_get_Item` 有声明形参 `(KeyOrIndex)` → `params.empty()` 为假 → **落入「合并」路径**，把外层 `("score")` 直接拼到内层调用尾部 → 3 个实参。

设计假设是「内层无参 prop_get 只发了 this，外层实参才是它的参数」。但对**内层已自带实参**的 `Rows(1)` 而言，该假设不成立：`classMethodObjArg` 已是 `TestDB->Rows, vb6_VariantFromValue(1)`（this + 1 实参）。

### 修法（cgen_expr.cpp）

1. **触发条件扩展**：`_prop_get_` 分支中，若 `classMethodObjArg` 顶层段数 ≥ 2（this + ≥1 实参）→ 判定内层调用已自足，外层索引改走返回对象的默认成员 `Item` 后期绑定。
2. **返回类型适配**：链式输出 `vb6_ComCall(innerObj, L"Item", ...)` 的 `obj` 形参是 `void*`，而内层 `prop_get_Item` 返回 `vb6_VARIANT` —— 若 `funcPart` 在 `variantReturnFuncs_` 中（driver 预扫描的 Variant 返回函数集合），先包 `vb6_VariantToObjectVal(innerObj)`，避免 Variant 直传 void* 触发 C2440。
3. **BSTR 上下文提取**：`wrapToBSTR` 新增 `vb6_VariantFromComResult(` 顶层前缀识别 → `vb6_VariantToString(...)`。**必须放在 `vb6_BSTR` 子串检查之前**，因为内层实参可能含 `vb6_BSTR_FromStr`。

结果（与既有 `Row("id")` 形态一致）：

```c
vb6_VariantToString(vb6_VariantFromComResult(
    vb6_ComCall(vb6_VariantToObjectVal(
        vb6_cCollection_prop_get_Item(TestDB->Rows, vb6_VariantFromValue(1))),
        L"Item", (void*[]){vb6_ComPackBSTR(vb6_BSTR_FromStr(L"score"))}, 1)))
```

### 验证

bisect 11 → 9，无回归（C2197/C2440 同族清零）。

## 092x：COM 接口字段链式成员访问（cHttpServer 821）

### 现象

`cHttpServer.cls` 809 行：

```vb
If Request.Header.Exists("Cookie") = True Then
```

生成：

```c
if ((Request->Header->Exists(vb6_BSTR_FromStr(L"Cookie")) == (-1)))
```

报 **C2037**（`vb6_ComIface_IDictionary` 左侧不是结构/联合）。

对照同文件 822 行 `Request.Header("Cookie")` 生成正确：

```c
vb6_ComCall((void*)Request->Header  /* class var .Header field */, L"Item", ...)
```

以及 `cHttpServerRequest.cls` 内 `Header.Exists(...)`（隐式 me）也正确生成 `vb6_ComCallInt(me->Header, L"Exists", ...)`。

### 根因

`Header As Dictionary` 是 COM 接口字段。driver 预扫描时：

- **COM 接口字段**记入 `classTypedFieldMap_[类][字段] = "COM:<Interface>"`；
- 而 `classVoidFieldMap_` 不含（真正 `void*` 字段才进）。

内层 MAE 的值上下文字段访问（`knownClassVars_` 回退分支）**只查 `classVoidFieldMap_`** 决定是否追加 `voidptr` 标记，于是 `Request.Header` 输出为 `Request->Header  /* class var .Header field */`（**无** `voidptr`）。

外层 `.Exists` 走到 Fix 088b「typed 类对象字段链」：`inferClassTypeOfExpr(Request.Header)` 命中 `COM:` 前缀 → 返回空串（设计上「COM 字段交给 COM 处理」）→ 但代码没有据此切 COM，而是落入兜底 `objExpr + "->" + cIdent(memberName)` → `Request->Header->Exists`。

### 修法（cgen_expr.cpp）

内层 MAE 追加 `voidptr` 标记时**同时**查询 `classTypedFieldMap_` 的 `COM:` 前缀条目，视为 COM 指针字段：

```cpp
if (!isVoidPtr && classTypedFieldMap_) {
    auto itT = classTypedFieldMap_->find(itClassVar->second);
    if (itT != end) {
        auto itF = itT->second.find(memLower);          // 兼查 m_ 前缀
        if (itF != end && itF->second.compare(0, 4, "COM:") == 0) isVoidPtr = true;
    }
}
```

→ 外层 MAE Fix 023 识别 `voidptr` 标记 → 切 COM dispatch → `vb6_ComCall(Request->Header, L"Exists", ...)`。

### 验证

bisect 9 → 8，无回归。

**重要判读**：生成的 `vb6_ComCall(...) == (-1)`（指针与 -1 比较）是**既有架构形态**，非本修复引入 —— 在 `cSSE.c` 68/110/120/184 行的 `me->MapHSocketToUsers->RootItem  /* ... voidptr */ .Exists(...)` 已是完全相同写法并长期编译通过。故本修复只负责让它进入 COM dispatch 路径（消除 C2037），不改变该比较形态。

## 收尾与教训

- **教训 ①**：Fix 086 的「内层无参 prop_get」假设过窄 —— 判断「内层调用是否自足」应看**实参个数**（顶层段数 ≥ 2），而非属性的声明形参数。
- **教训 ②**：同一语义（COM 字段）在两侧用了**两张表**（`classVoidFieldMap_` 用于标记、`classTypedFieldMap_` 的 `COM:` 前缀用于类型），漏查一张即静默退化为结构体访问。凡「字段是否 COM」的判断，两张表都要看。
- **教训 ③**：判断某生成形态是否「既有可接受」时，直接在**生成产物目录**（temp `C3C\<pid>\*.c`，非仓库内、不受 .gitignore 限制）用 `search_content` 搜同类形态，比反复读生成器代码更快确认边界。092w/092x 均靠此法确认与既有行为一致后即收敛，未盲目扩大改动面。

## 结果

- 无窗体 bisect 基线 **11 → 8**；
- 剩余 8 = cLogs×5 + cLayer×2（窗体假阳性）+ cHttpServerResponse 452（`Common` 外部模块配置缺口）；
- **无窗体 bisect 面已收敛**，无残留真实代码缺陷。
