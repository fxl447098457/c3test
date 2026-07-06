# P24-05: ActiveX DLL回归测试 + For Each COM修复

**日期**: 2026-07-06
**状态**: 已完成
**Commit**: 待提交

## 概要

为P24-01~P24-04的COM优化修复添加回归测试锁定，过程中发现并修复了两个For Each COM集合相关的编译器bug。

## 新增测试

### test_p24.bas — P24-01/03 COM回归测试
- **P24-01**: COM返回值在字符串拼接上下文中的VARIANT→BSTR解包
  - `"Path: " & fso.GetAbsolutePathName(".")`
  - `s = fso.GetAbsolutePathName("C:")`
  - `"Folder: " & fso.GetFolder("C:\Windows").Name`
- **P24-03**: For Each COM集合枚举（IEnumVARIANT）
  - `For Each d In drives` — 预赋值集合变量
  - `For Each d In fso.Drives` — 内联COM属性访问
  - 循环体内访问 `d.IsReady` COM属性

### test_vbman/ — P24-04 VB_GlobalNameSpace编译测试
- `test_vbman.vbp`: 引用VBMAN.dll TypeLib
- `Module1.bas`: `VBMAN.Version()` + `CreateObject("VBMANLIB.cVBMAN").Version()`
- 编译测试（VBMAN.dll为32位，x64环境中仅编译不运行）

### run_tests.ps1 增强
- `Test-Run`/`Test-Vbp` 新增可选 `$Arch` 参数（支持 `--arch x86`）
- 新增"P24 COM Optimization Tests"测试分区

## 修复的编译器Bug

### Bug #1: For Each COM marker未物化
**症状**: `For Each d In fso.Drives` 生成 `vb6_ForEach_Init(fso)` 而非 `vb6_ForEach_Init(vb6_ComGetObjectProp(fso, L"Drives"))`

**根因**: MemberAccessExpr处理COM对象时设置 `isComMarker_=true`，但 `lastExpr_` 仅保持基础对象名。For Each codegen只读 `lastExpr_`，没有消费COM marker。

**修复**: 在 `emitExpr(*node.collection)` 后检查 `isComMarker_`，调用 `resolveComValue("Object")` 物化COM属性调用。

### Bug #2: For Each Object类型循环变量赋值错误
**症状**: `Dim d As Object` 的 `For Each d In drives` 生成 `d = vb6_VariantFromStackVARIANT(&_fe_var)` — `vb6_VARIANT` 类型无法赋值给 `void*`

**根因**: For Each codegen统一使用 `vb6_VariantFromStackVARIANT` (返回 `vb6_VARIANT`)，未区分循环变量类型。Object类型应用 `vb6_ComUnpackObject` (接受原生 `VARIANT*`，返回 `void*`/IDispatch*)。

**修复**: 循环变量赋值改为类型感知：Object/TypedCom → `vb6_ComUnpackObject`，其他 → `vb6_VariantFromStackVARIANT`。

## 修改文件

| 文件 | 修改内容 |
|------|---------|
| `src/backend/cgen_stmt.cpp` | For Each COM marker物化 + 循环变量类型感知赋值 |
| `tests/test_p24.bas` | 新增: P24-01/03 COM回归测试 |
| `tests/test_vbman/Module1.bas` | 新增: P24-04 VBMAN GlobalNameSpace测试 |
| `tests/test_vbman/test_vbman.vbp` | 新增: 引用VBMAN.dll的VBP工程 |
| `tests/run_tests.ps1` | Test-Run/Test-Vbp新增Arch参数 + P24测试分区 |

## 测试结果

**76/76 全部通过** (原74个 + 新增2个)
- test_p24: PASS (运行时验证FSO COM调用+For Each枚举)
- test_vbman: PASS (编译验证VBMAN GlobalNameSpace)

## 已知限制

1. c3的 `buildVcvarsPrefix` 在x64 MSVC环境中不追加x86 vcvarsall — `VCINSTALLDIR` 已设置时直接返回空字符串。x86编译需在x86环境中运行。
2. VBMAN测试为compile-only，未验证运行时行为（需x86环境）。
