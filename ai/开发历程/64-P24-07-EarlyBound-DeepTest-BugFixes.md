# P24-07: 早期绑定COM深度测试 - 5个Bug修复

**日期**: 2026-07-09
**状态**: done
**回归**: 78/78 零失败

## 背景

P24-07的目标是把C3编译器的COM早期绑定能力从"基本能编译"推进到"常见VB6 COM代码编译运行正确"。上轮只测了FolderExists/FileExists两个简单方法，存在大量未覆盖场景。

## 发现并修复的Bug

### Bug 5: ReadAll崩溃 — vb6_ComGetProp缺少DISPATCH_METHOD
- **现象**: `ts.ReadAll` 调用后程序崩溃
- **根因**: `vb6_ComGetProp()` 只使用 `DISPATCH_PROPERTYGET`，但ReadAll是INVOKE_FUNC方法，COM服务器需要`DISPATCH_METHOD`标志
- **修复**: `vb6com.c` line 549: `DISPATCH_PROPERTYGET` → `(DISPATCH_METHOD | DISPATCH_PROPERTYGET)`
- **RTL lib**: 重新编译x86和x64的vb6rtl.lib

### Bug 6: BSTR字符串比较 — 指针比较而非内容比较
- **现象**: `If bp = "C:\Windows\System32" Then` 总是False
- **根因**: `cgen_expr.cpp` BinaryExpr对BSTR生成`(left == right)`，这是指针比较
- **修复**: 在`mapBinaryOp()`分发前添加字符串比较块：当任一操作数为Vb6Type::String时，生成`vb6_StrCmp(left, right) op 0`
- **注意**: BinaryOp枚举名是Eq/Neq/Lt/Gt/Le/Ge（不是Equal/NotEqual）

### resolveComValue()重写 — 利用前期绑定签名推断返回类型
- **现象**: `Not fso.FileExists(path)` 生成 `~BSTR`（非法C），因为resolveComValue()只用unpackType参数默认"BSTR"
- **修复**: 重写`cgen_util.cpp:resolveComValue()`，优先检查`isEarlyBoundCom_ && earlyBoundSym_`，利用TypeLib签名的returnType选择正确解包函数；未知类型（如UserDefinedType）回退到unpackType参数

### TKIND_ALIAS解析 — DriveType属性类型推断
- **现象**: `drv.DriveType` 被TypeLib解析为Vb6Type::UserDefinedType(36)，因为VT_USERDEFINED引用TKIND_ALIAS类型
- **根因**: `mapTypeDesc()` 未处理TKIND_ALIAS (typekind=6)
- **修复**: 在VT_USERDEFINED分支的if-else链中增加TKIND_ALIAS处理：递归调用`mapTypeDesc(&pRefAttr->tdescAlias, pRefTI)`解析别名底层类型
- **效果**: DriveType现在正确映射为Vb6Type::Long，生成`vb6_ComGetIntProp(drv, L"DriveType")`

### Bug 4: Not运算符对COM结果 — ~void*非法C
- **现象**: `Not fso.FileExists("path")` 生成 `~vb6_ComCall(fso, ...)`，`~void*`是非法C
- **根因**: `IndexOrCallExpr`中，有参数的前期绑定方法(如FileExists)直接生成`vb6_ComCall()`返回void*，没有根据签名returnType选择类型化调用函数
- **修复**: 在`IndexOrCallExpr`的前期绑定分支中，增加有参数方法的类型化COM调用分发：根据签名的returnType选择`vb6_ComCallInt/BSTR/Double/Object`
- **效果**: `FileExists("path")` → `vb6_ComCallInt(fso, L"FileExists", ...)`，返回int32_t，`~int32_t`合法

## 误删typelib_parser.cpp修复

清理诊断代码时误删了5个含`diag_`的行：
- 构造函数 `TypeLibParser(Diagnostics& diag) : diag_(diag) {}`
- 3个 `diag_.warn(...)` 调用

通过`git checkout -- src/com/typelib_parser.cpp`恢复。

## 测试结果

### test_earlybound2.bas (10/10)
1. FolderExists ✓
2. FileExists ✓
3. BuildPath (2参数方法, String返回) ✓
4. GetExtensionName ✓
5. GetFileName ✓
6. GetAbsolutePathName ✓
7. DriveType (Long属性, 曾为UserDefinedType) ✓
8. IsReady (Boolean属性) ✓
9. CreateTextFile + WriteLine + Close ✓
10. ReadAll (方法调用, 曾崩溃) ✓

### test_not_com.bas (3/3)
1. Not fso.FileExists(不存在的文件) → True ✓
2. Not fso.FolderExists(不存在的目录) → True ✓
3. 整体PASS ✓

### 回归测试 78/78

新增2个测试到回归套件，全部通过零失败。

## 修改的文件

| 文件 | 修改内容 |
|------|---------|
| src/rtl/core/vb6com.c | vb6_ComGetProp加DISPATCH_METHOD |
| src/rtl/core/vb6rtl.h | vb6_ComPackBool声明 |
| src/rtl/lib/vb6rtl.lib (x64) | 重新编译 |
| src/rtl/lib/x86/vb6rtl.lib | 重新编译 |
| src/backend/cgen_expr.cpp | BSTR比较+有参数方法类型化COM调用 |
| src/backend/cgen_util.cpp | resolveComValue()重写 |
| src/com/typelib_parser.cpp | TKIND_ALIAS解析 |
| tests/test_earlybound2.bas | 新增深度测试 |
| tests/test_not_com.bas | 新增Not COM结果测试 |
| tests/run_tests.ps1 | 新增2个测试到回归套件 |

## 遗留项

- Bug 2: Variant数值比较 (`vb6_VARIANT > 0` 是无效C) — 需要vb6_VariantToLong/Double
- P24-10: COM默认属性 `obj(0)` → `_Item/Item`
- P24-11: COM可选参数/ByRef参数
- P24-09: 多接口Implements
- P24-06: 外部COM WithEvents
