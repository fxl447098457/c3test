# P24-05 frxParse运行时OLEAUT32崩溃修复

> 日期: 2026-07-07
> 里程碑: P24-05
> 基线: 76/76回归零失败 → 76/76回归零失败

## 问题

frxParse编译成功但运行时崩溃，退出码`0xC000041D`(STATUS_FATAL_APP_EXIT)，OLEAUT32.dll ACCESS_VIOLATION at offset 0x00015f9c。

## 根因分析

### 二分定位（11个测试 A-K）

| 测试 | ImageList 47图 | VBMAN COM链 | Timer | 结果 |
|------|---------------|-------------|-------|------|
| A | ❌ | ❌ | ❌ | ✅ 通过 |
| B | 创建null | ✅ | ✅ | ❌ 崩溃 |
| C | ✅ | ❌ | ✅ | ✅ 通过 |
| D | CreateObject only | - | - | ✅ 通过 |
| E | +ComCallObject | - | - | ✅ 通过 |
| F | +ComCall(手动清理) | - | - | ✅ 通过 |
| G | 原始嵌套一行 | - | - | ❌ 崩溃 |
| H | 分步+MessageBox | - | - | ✅ 通过 |
| I | 分步+VariantFromComResult | - | - | ✅ 通过 |
| J | 分步+SetControlText | - | - | ✅ 通过 |
| K | BSTR传给VariantFromComResult | - | - | ❌ 崩溃 |

**结论**: 崩溃不在47张ImageList图片加载，而在`Me.Caption = "Power by vbman - " & VBMAN.Version()`的BSTR/Variant类型混淆。

### Bug 7 — wrapToBSTR/wrapVariantValue 子串匹配导致 BSTR→VARIANT* 类型混淆

**现象**: 生成的C代码为
```c
vb6_SetControlText(hwnd, vb6_VariantToString(vb6_VariantFromComResult(vb6_BSTR_Concat(...))));
```

`vb6_BSTR_Concat`返回BSTR(wchar_t*)，但被传给`vb6_VariantFromComResult`(期望VARIANT*)，OLEAUT32读取BSTR内存当VARIANT字段，`VariantClear`释放垃圾指针→ACCESS_VIOLATION。

**根因**: `wrapToBSTR()`和`wrapVariantValue()`用`find("vb6_ComCall(") != npos`做子串匹配，匹配到了嵌套深层的`vb6_ComCall`，但表达式顶层已经是`vb6_BSTR_Concat`(BSTR类型)。

**修复**: 改为`find("vb6_ComCall(") == 0`(前缀匹配)，只匹配顶层调用。

## 修复清单（7个Bug）

| Bug | 文件 | 行 | 描述 | 修复 |
|-----|------|-----|------|------|
| B1 | cgen_form.cpp | 1264 | ActiveX控件ProgID错误(CoClass名≠ProgID) | CLSID直用替代ProgID查找 |
| B2 | typelib_parser.cpp | 97 | TypeLib import对Object=GUID失败 | loadByClsid先查TypeLib ID再查CoClass CLSID |
| B3 | cgen_form.cpp | 1280 | CLSID C初始化格式错误 | GUID字符串→C init格式转换 |
| B4 | vb6com.c | 460 | vb6_ComPackObject缺AddRef→双重Release | AddRef后VariantClear+ReleaseObject正确配对 |
| B5 | vb6com.c | 331 | vb6_ComSetProp VARIANT泄漏 | 所有return路径添加free(value_void) |
| B6 | cgen_form.cpp | 1363 | ComCall返回VARIANT*未释放 | vb6_ComVarFree((void*)vb6_ComCall(...)) |
| B7 | cgen_expr.cpp:613, cgen_util.cpp:1267 | - | find()!=npos匹配嵌套COM调用→类型混淆 | 改为find()==0前缀匹配 |

## 未解决的次要问题

- **typelib_parser.cpp的loadByClsid修复导致"重复声明: vbman"**: 新实现能正确找到VBMAN.dll的TypeLib并导出其符号，但`vbman`(VBMAN.dll中的枚举/常量)与VB6代码中的变量重名。暂时回退此修复，后续需要在符号注册时允许COM符号被用户变量覆盖。

## 修改文件

- src/backend/cgen_expr.cpp — wrapToBSTR: find()!=npos → find()==0
- src/backend/cgen_util.cpp — wrapVariantValue: find()!=npos → find()==0
- src/backend/cgen_form.cpp — CLSID直用 + ComVarFree
- src/rtl/core/vb6com.c — AddRef + free(value_void)
