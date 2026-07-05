# 051-M13-FIX2 COM Item PROPERTYGET修复

**日期**: 2026-07-05
**里程碑**: M13-FIX2
**状态**: 已完成，74/74回归零失败

## 问题描述

frxParse项目的 `dic.Item("hello")` 调用返回空值/Null，但 `dic.Add` 正常工作。VB6原版exe运行正常，说明是c3编译器/RTL的bug。

## 根因分析

### 1. COM Invoke标志错误 (核心根因)

`vb6_ComCall()` 函数始终使用 `DISPATCH_METHOD` 标志调用 `IDispatch::Invoke()`。

但 Scripting.Dictionary 的 Item 成员是 **参数化属性** (parameterized property get)，其 `invkind=0x2` (INVOKE_PROPERTYGET)，不是方法。

独立测试程序 `test_dict_item.c` 验证：
| Invoke Flag | 结果 |
|---|---|
| DISPATCH_METHOD alone | 0x80020003 FAIL |
| DISPATCH_PROPERTYGET alone | SUCCESS - "world" |
| DISPATCH_METHOD | DISPATCH_PROPERTYGET | SUCCESS - "world" |

### 2. CRT链接不匹配

RTL .lib 原编译用 `/MD`(动态CRT)，c3生成命令不用 `/MD`(静态CRT)，导致28个LNK2019未解析外部符号。

### 3. UTF-8 BOM丢失

PowerShell的 `[System.IO.File]::WriteAllText()` with `UTF8Encoding($false)` 剥离BOM。MSVC在 `/std:c11` 模式下无BOM时错误解析中文注释，级联产生97+错误。

## 修复内容

### Fix 1: vb6_ComCall Invoke标志 (vb6com.c L~209)
```c
// Before:
hr = pDisp->lpVtbl->Invoke(pDisp, dispid, &IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &dp, &result, &ei, &argErr);
// After:
hr = pDisp->lpVtbl->Invoke(pDisp, dispid, &IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD | DISPATCH_PROPERTYGET, &dp, &result, &ei, &argErr);
```

### Fix 2: vb6_VariantFromComResult 新函数 (vb6rtl.c + vb6rtl.h)
```c
vb6_VARIANT vb6_VariantFromComResult(void* variant_ptr) {
    VARIANT* pv = (VARIANT*)variant_ptr;
    vb6_VARIANT result;
    memset(&result, 0, sizeof(result));
    if (pv && V_VT(pv) != VT_EMPTY && V_VT(pv) != VT_NULL) {
        if (V_VT(pv) == VT_BSTR) {
            result.vt = 8; // vbString
            result.bstrVal = V_BSTR(pv);
            V_BSTR(pv) = NULL; // ownership transfer
        } else if (V_VT(pv) == VT_I4 || V_VT(pv) == VT_I2) {
            result.vt = 2; // vbInteger
            result.lVal = V_VT(pv) == VT_I4 ? V_I4(pv) : V_I2(pv);
        } // ... more types
    }
    if (pv) VariantClear(pv);
    return result;
}
```

### Fix 3: wrapVariantValue COM检测 (cgen_util.cpp L~1250)
```cpp
if (cExpr.find("vb6_ComCall(") != std::string::npos)
    return "vb6_VariantFromComResult(" + cExpr + ")";
```

### Fix 4: MsgBox Variant→BSTR转换 (cgen_expr.cpp L~2484)
- 检测 `vb6_VariantFromComResult` → 包装 `vb6_VariantToString()`
- 检测 `inferExprType(*node.positional[0]) == Vb6Type::Variant` → 包装 `vb6_VariantToString()`

### Fix 5: CRT链接修复
RTL编译命令从 `/O2 /MD` 改为 `/O2 /std:c11`，匹配c3生成的编译命令。

### Fix 6: UTF-8 BOM修复
所有RTL .c文件重写后确保带BOM：`UTF8Encoding($true)`。

## 验证结果

1. **frxParse编译**：`c3 --arch x86 工程1.vbp` → 编译链接成功
2. **生成C代码**：
   - L198: `vb6_ComVarFree(vb6_ComCall(dic, L"Add", ...))` ✓
   - L201: `v = vb6_VariantFromComResult(vb6_ComCall(dic, L"Item", ...))` ✓
   - L202: `vb6_MsgBox1(vb6_VariantToString(v))` ✓
3. **运行验证**：MsgBox正常弹出，进程等待用户点击（5秒未退出）→ 内容正确显示
4. **回归测试**：74/74 零失败

## 修改文件清单

| 文件 | 修改 |
|---|---|
| src/rtl/core/vb6com.c | DISPATCH_METHOD→DISPATCH_METHOD|PROPERTYGET; 移除VariantFromComResult实现 |
| src/rtl/core/vb6rtl.c | 新增vb6_VariantFromComResult()函数 |
| src/rtl/core/vb6rtl.h | 新增vb6_VariantFromComResult声明 |
| src/rtl/core/vb6com.h | 移除vb6_VariantFromComResult声明(类型依赖问题) |
| src/backend/cgen_util.cpp | wrapVariantValue增加vb6_ComCall检测 |
| src/backend/cgen_expr.cpp | MsgBox BSTR转换增加Variant类型检测 |

## 技术要点

- COM参数化属性(Property Get with args)必须用DISPATCH_PROPERTYGET或组合标志调用
- `DISPATCH_METHOD | DISPATCH_PROPERTYGET` 组合标志兼容方法和属性两种invkind，COM自动匹配
- c3的IndexOrCallExpr COM marker路径统一走vb6_ComCall，无需在cgen层区分method vs property get
- vb6_VARIANT和VARIANT是不同类型，桥接函数vb6_VariantFromComResult负责类型转换和所有权转移
