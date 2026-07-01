# 039-M22-isVarName2-DeclareANSI-MsgBox空参数

> 日期: 2026-07-01
> 里程碑: M22 实战VB6项目编译+运行时修复（续）
> 回归: 74/74 零失败

---

## 1. isVarName2 作用域修复

### 问题
cgen_expr.cpp 的 MemberAccessExpr (优先级4) 使用 `isVarName2` 判断表达式是否为变量名，但 `isVarName2` 仅通过 `symTab_.lookup()` 查找——语义分析后 `current_` 指向模块作用域，过程级局部变量查找不到。导致：
- UDT 变量 `p.X` 被误判为 `Module1.X` → 生成 `vb6_p_X` 而非 `p.X`
- 类变量 `c.Method` → 错误的跨模块调用
- 接口变量 `s.Method` → 同上

### 修复 (三叉)

| 文件 | 修改 |
|------|------|
| cgen_expr.cpp | 扩展 `isVarName2` 检查所有 cgen 跟踪集合 (knownUdtVars_, knownClassVars_, knownNewVars_, knownObjectVars_, knownTypedComVars_, knownIfaceVars_) |
| cgen_stmt.cpp | 赋值侧 MemberAccess (p.X = 100) 同样扩展 isVarName 检查 |
| cgen_decl.cpp | 过程入口注册 UDT/Class/Iface/BSTR/Double/Long 参数类型；过程中注册 UDT 局部变量 |

### 关键回归教训
初始方案对所有跟踪集合执行 `.clear()`，导致 test_events 和 M7Test 回归：
- WithEvents 变量 (`Dim WithEvents btn As Button`) 在模块级注册到 `knownClassVars_`
- 过程入口 `.clear()` 删除了这些模块级条目
- **修复**: 只 clear `knownUdtVars_`（map 类型，旧条目与新变量名冲突），set 类型不清空

### Commit
`f255821`, 4 files changed, 70/74→74/74

---

## 2. CStr(BSTR) 类型匹配优化

### 问题
`vb6_CStr(name)` 中 `name` 是 BSTR 变量时，`vb6_CStr` 期望 `vb6_VARIANT` 参数，类型不匹配。

### 根因
函数参数的 BSTR 类型未注册到 `knownBstrVars_`。

### 修复
1. cgen_decl.cpp: 过程入口注册 BSTR/Double/Long 参数到类型跟踪集合
2. cgen_expr.cpp: CStr(BSTR) 优化 — 当参数已经是 BSTR 变量时，直接传递不包装

---

## 3. Declare ANSI API 无泄露

### 背景
VB6 的 Declare 语句声明 Windows API 函数，A 版本（如 `GetWindowTextA`、`WritePrivateProfileStringA`）的 `ByVal String` 参数需要 BSTR→ANSI 转换。

### 初始实现（有泄露）
```c
SomeFuncA(vb6_BSTR_ToANSI(str1), vb6_BSTR_ToANSI(str2), otherArg);
// ↑ 两个 malloc 分配的 char* 永远不会被释放
```

### 最终实现（无泄露）

**RTL 层** (vb6rtl.h):
```c
static inline char* vb6_BSTR_ToANSI(BSTR bstr);  // WideCharToMultiByte(CP_ACP)
static inline void vb6_FreeANSI(char* ansi);      // free()
```

**Cgen 层** — 临时变量 + 统一释放:
1. `cgen_expr.cpp`: 遇到 Declare ANSI 调用的 ByVal String 参数，先 emit 临时变量声明:
   ```c
   char* _ansi_0 = vb6_BSTR_ToANSI(str1);
   char* _ansi_1 = vb6_BSTR_ToANSI(str2);
   ```
   记录到 `ansiTempsToFree_` 向量，用 `_ansi_N` 替代内联表达式
2. `cgen_stmt.cpp`: `emitStmtList` 每条语句后统一释放:
   ```c
   SomeFuncA(_ansi_0, _ansi_1, otherArg);
   vb6_FreeANSI(_ansi_0);
   vb6_FreeANSI(_ansi_1);
   ```
3. `cgen_decl.cpp`: 过程 return 前兜底释放（安全网）

| 文件 | 新增 |
|------|------|
| cgen.hpp | `ansiTempsToFree_` (vector\<string\>) + `ansiCounter_` (int) |
| cgen_expr.cpp | 临时变量模式替代内联 |
| cgen_stmt.cpp | emitStmtList 统一 FreeANSI |
| cgen_decl.cpp | 过程入口重置 + return 前兜底释放 |
| cgen_decl.cpp | Declare ANSI 检测（Alias/函数名以'A'结尾+前字符小写） |

---

## 4. MsgBox 空参数 / Optional 参数省略

### 问题
`MsgBox "hi", , "title"` — 连续逗号省略 buttons 参数导致崩溃。

### 根因
parser 创建占位符 `LiteralExpr(Long, "0")` 时，union 的 `longValue` 未显式设置：
```cpp
LiteralExpr(loc, LiteralKind::Long, "0")  // 构造函数只设 intValue(0)
// longValue 的高32位是未初始化垃圾 → 生成 1846835937280L 而非 0L
```

### 修复

| 文件 | 修改 |
|------|------|
| parser_stmt.cpp | MsgBox 语句形式占位符: 显式设置 `placeholder->longValue = 0` |
| parser_expr.cpp | 通用函数调用参数列表: 添加空参数检测（Comma/RightParen → 占位符） |

通用处理确保 `AnyFunc(arg1, , arg3)` 也能正确解析。

---

## 5. IsMissing() — 待实现

用户指出 VB6 的 `IsMissing()` 函数用于检测 Optional 参数是否被省略：
```vb6
Sub Test(Optional x As Variant)
    If IsMissing(x) Then Debug.Print "not provided"
End Sub
```

### 实现方案（待做）
- cgen 为每个 Optional 参数生成额外的 `bool _has_<param>` 标志
- 调用点省略 Optional 参数时传 `false`，提供时传 `true`
- `IsMissing(paramName)` → `!_has_<param>`
- Variant 类型 Optional: 替代方案用 VT_ERROR/DISP_E_PARAMNOTFOUND 哨兵值

### 后续任务

已记入进度表 P20-36（004-进度表.md），下一周期执行。

---

## 文件变更汇总

| 文件 | 变更 |
|------|------|
| src/backend/cgen.hpp | +ansiTempsToFree_, +ansiCounter_, +knownDeclareAnsi_ |
| src/backend/cgen_expr.cpp | isVarName2扩展, CStr(BSTR)优化, ANSI临时变量模式 |
| src/backend/cgen_stmt.cpp | 赋值侧isVarName扩展, emitStmtList统一FreeANSI, UDT局部变量注册 |
| src/backend/cgen_decl.cpp | 参数类型注册, ANSI检测, ansiTempsToFree_清理 |
| src/rtl/core/vb6rtl.h | +vb6_BSTR_ToANSI, +vb6_FreeANSI |
| src/parser/parser_stmt.cpp | MsgBox占位符longValue=0 |
| src/parser/parser_expr.cpp | 通用空参数处理 |
| ai/004-进度表.md | M22描述更新 |

## 回归测试

74/74 PASS, 零回归

## Commit

(待合并上一轮 f255821 后一起提交)
