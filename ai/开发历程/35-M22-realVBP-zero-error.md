# M22 里程碑：realVBP 零错误编译

**日期**: 2026-07-01
**Commit**: f84a31b
**结果**: dist/demos/realVBP (Form1.frm + Module1.bas + Class1.cls) 编译零C级错误，生成工程1.exe

## 背景

M22 目标是让 C3 编译器能编译真正的 VB6 多模块项目。先有 form demo (单模块窗体) 编译通过，但 realVBP (3模块项目) 还有多个 C 级编译错误。

## 修复清单

### Fix 1: wrapToBSTR 中 vb6_Now() 误判为 BSTR

**问题**: `Text1.Text = Now` 生成 `vb6_SetControlText(hwnd, vb6_Now())`，但 `vb6_Now()` 返回 `double`，`SetControlText` 参数2需要 `BSTR`（指针类型），报 C2172 "实参不是指针"。

**根因**: `wrapToBSTR()` 函数中：
1. Line 512 有 `if (expr.find("vb6_Now") != npos) return expr;` — early return 把 `vb6_Now()` 当作已返回 BSTR 的函数
2. 即使删除了 early return，`vb6_Now()` 以 `vb6_` 开头，走进 "其他vb6_函数假定为BSTR" 分支直接返回

**修复**:
- 删除 `vb6_Now` 的 early-return
- 在 `vb6_` 前缀分支中加入 Date 函数检测（`vb6_Now/Date/Time` → `vb6_CStrDate()`）
- 加入更多数值函数检测（`vb6_Timer/Rnd/Sqr/Sgn/Fix/Int/Val` → `vb6_CStrLong()`）

**文件**: `src/backend/cgen_expr.cpp`

### Fix 2: 类模块 .c 文件缺少标准模块 #include

**问题**: `Class1.c` 使用 `myName` 变量但未 `#include "Module1.h"`，报 C2065 "myName: 未声明的标识符"。

**根因**: `cgen_base.cpp` 中跨模块 #include 策略有缺陷：
- 标准模块引用标准模块 → `.c`（正确）
- 类模块引用类模块 → `.h`（正确）
- **类模块引用标准模块** → 漏掉了！`if (!isClassModule_)` 直接跳过了类模块

**修复**: 去掉 `!isClassModule_` 限制，改为所有模块的 `.c` 都 include 引用的标准模块头文件。

**文件**: `src/backend/cgen_base.cpp`

### Fix 3: VB6 函数名引用 = 返回值变量

**问题**: `Module1.myName = Add`（Class1.cls 中）生成 `vb6_BSTR_Assign(&myName, vb6_Class1_Add)`，传入了函数指针而非返回值变量，报 C4047/C4024 类型不匹配。

**根因**: VB6 语义中，在函数体内引用自身函数名等同于引用返回值变量（如 `Add = 1` 设置返回值，`Module1.myName = Add` 引用返回值）。但 IdentifierExpr 的早期检查（line 158-166）总是返回函数过程名。

**修复**: 
- IdentifierExpr 中检测当前函数名引用：
  - **非调用callee上下文** → 返回 `currentReturnVar_`（`vb6_ret_Add`）
  - **IndexOrCallExpr的callee上下文** → 返回函数名（`vb6_Class1_Add`，供递归调用）
- 新增 `asCallCallee_` 布尔成员变量作为上下文标志
- 在 `IndexOrCallExpr::emitExpr(*node.callee)` 前后设置/重置此标志

**递归调用关键区分**: `Factorial = n * Factorial(n-1)` 中，`Factorial(n-1)` 的 `Factorial` 是 IndexOrCallExpr 的 callee，需要是函数名；而 `Factorial = ...` 和 `myName = Add` 中的 `Add` 是纯 IdentifierExpr，需要是返回值变量。

**文件**: `src/backend/cgen_expr.cpp`, `src/backend/cgen.hpp`

## 其他已应用修复（前轮完成）

- RTL typed CStr overloads: `vb6_CStrLong/Dbl/Bool/Byte/Date`
- VARIANT 复合字面量: `{.vt=VT_xxx, .field=value}` 指定初始化器
- 控件属性 BSTR 包装: `SetControlText`/`SetMenuCaption` 写入时 wrapToBSTR
- CallStmt void return: `callExpr=="0"` 时 early return
- 跨模块变量: `externalModules_` 判断 + 符号表 BSTR 类型检查
- Me.Property BSTR 包装
- 控件默认属性 BSTR 包装
- escapeWideCString + AppendMenuW（中文菜单支持）
- GBK→UTF-8 编码转换（SourceBuffer::readAndConvertToUtf8）

## 回归测试

70/74 通过。4 个失败为前 M22 遗留问题：
- `test_compat`: UDT 成员访问（`vb6_p_X` 等未声明标识符）
- `M6Test`: 测试框架配置问题（手动编译实际通过）
- `test_implements`: Implements 接口查找问题
- `M7Test`: UDT 成员访问（`vb6_c_Radius`）

## 生成代码验证

### Class1.c（修复后）
```c
#include "Class1.h"
#include "Module1.h"  // Fix 2: 之前缺少

BSTR vb6_Class1_Add(vb6_cls_Class1* me, vb6_VARIANT* a, vb6_VARIANT* b) {
    BSTR vb6_ret_Add = vb6_BSTR_Empty();
    vb6_BSTR_Assign(&vb6_ret_Add, vb6_BSTR_Concat(vb6_CStr((*a)), vb6_CStr((*b))));
    vb6_BSTR_Assign(&myName, vb6_ret_Add);  // Fix 3: 之前是 vb6_Class1_Add 函数指针
    return vb6_ret_Add;
}
```

### Form1.c（修复后）
```c
static void vb6_Command1_Click(void) {
    vb6_SetControlText(vb6_hwnd_Text1, vb6_CStrDate(vb6_Now()));  // Fix 1: 之前直接传 vb6_Now()
    vb6_SetControlText(vb6_hwnd_Label1, vb6_GetControlText(vb6_hwnd_Text1));
}
```

## 修改文件汇总

| 文件 | 修改内容 |
|------|---------|
| `src/backend/cgen.hpp` | +`externalModules_`, +`asCallCallee_`, +`escapeWideCString()`, +`wrapToBSTR()` |
| `src/backend/cgen_base.cpp` | `externalModules_` 设置, 类模块.c include标准模块, `escapeWideCString()`, AppendMenuW |
| `src/backend/cgen_expr.cpp` | `wrapToBSTR()`, vb6_Now/Date/Date函数处理, asCallCallee_ callee上下文, 跨模块变量, VARIANT复合字面量 |
| `src/backend/cgen_stmt.cpp` | CallStmt void return, Me.Property BSTR, 控件默认prop BSTR, 跨模块BSTR赋值, With块BSTR |
| `src/backend/cgen_form.cpp` | escapeWideCString + AppendMenuW |
| `src/rtl/core/vb6rtl.c` | +typed CStr overloads (Long/Dbl/Bool/Byte/Date) |
| `src/rtl/core/vb6rtl.h` | +typed CStr overload 声明 |
| `src/common/source_manager.hpp/cpp` | +readAndConvertToUtf8() GBK→UTF-8 |
| `src/project/vbp_parser.cpp` | 使用readAndConvertToUtf8() |
| `src/project/frm_parser.cpp` | 使用readAndConvertToUtf8() |
| `src/driver/driver.cpp` | .frm编译流程重构, 避免双读 |
