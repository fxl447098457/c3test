# 048 - P20-37注册表函数 + P20-36 IsMissing + P21-18 LoadPicture增强 + P21-25内置常量补全

**日期**: 2026-07-02
**提交**: 待提交
**测试**: 74/74 全部通过

## 本轮完成任务

### P20-37: 注册表4函数 (SaveSetting/GetSetting/DeleteSetting/GetAllSettings)

**修改文件**:
- `src/rtl/core/vb6rtl.h` — 添加4个注册表函数声明 + vb6_LoadPictureEx声明 + vb6_VARIANT union添加parray成员
- `src/rtl/core/vb6rtl.c` — 完整实现:
  - `vb6_RegBuildKey()` — 构建VB6注册表路径 (HKCU\Software\VB and VBA Program Settings\)
  - `vb6_SaveSetting()` — RegCreateKeyExW + RegSetValueExW
  - `vb6_GetSetting()` — RegOpenKeyExW + RegQueryValueExW, 支持默认值
  - `vb6_DeleteSetting()` — 支持删除单个值(RegDeleteValueW)或整个section(RegDeleteKeyW)
  - `vb6_GetAllSettings()` — RegEnumValueW枚举,返回1D Variant数组
  - `vb6_LoadPictureEx()` — OleLoadPicturePath支持所有图片格式
  - 添加 `#include <olectl.h>` (IPicture/OleLoadPicturePath)
- `src/backend/cgen_expr.cpp` — 添加builtinFuncs映射 + GetSetting可选默认值padding
- `src/backend/msvc_driver.cpp` — 控制台和GUI链接行添加advapi32.lib
- `src/semantics/semantic_analyzer.cpp` — 添加4个注册表函数的semantic注册

**问题修复**:
1. 原始impl_p20_37.py脚本中vb6rtl.c的插入标记不存在(`// CreateObject(progId)`)，改用文件末尾追加
2. GetAllSettings使用了不存在的常量(vb6_vtArray/VB6_SA_VARIANT)，改用vb6_SafeArrayCreate1D+vb6_ArraySetVariant
3. vb6_VARIANT union缺少parray成员，添加 `struct vb6_SafeArray1D* parray`
4. 链接器缺少advapi32.lib，在msvc_driver.cpp的控制台和GUI链接行添加

### P20-36: IsMissing()函数实现

**核心策略**: 为每个Optional参数生成 `int _has_<paramname>` 标志参数

**修改文件**:
- `src/backend/cgen_decl.cpp` — `makeParamList()` 中为每个Optional参数追加 `int _has_<name>` 到C参数列表
- `src/backend/cgen_expr.cpp` — 三处修改:
  1. **IsMissing拦截**: 在函数调用路径入口处( callee映射前)拦截`IsMissing(arg)`,若arg是当前过程的Optional参数则生成`(!_has_arg)`
  2. **通用Optional padding**: padding后追加_has_标志(传入=1, padding=0)
  3. **ParamArray路径padding**: 同样追加_has_标志
  4. **ByRef Variant padding修复**: Variant/Object类型的compound literal用`{0}`而非`{funcCall()}`

**功能测试**:
```
Case1: hello (no opt)     ' Optional未传 → IsMissing=True
Case2: hello + world      ' Optional传入 → IsMissing=False
Case3: test (no opt)      ' Optional未传 → IsMissing=True
```

### P21-18: LoadPicture增强

**修改文件**:
- `src/backend/cgen_expr.cpp` — builtinFuncs映射从`vb6_LoadPictureFromFile`改为`vb6_LoadPictureEx`
- `vb6_LoadPictureEx()` 使用OleLoadPicturePath，支持ICO/CUR/WMF/EMF/GIF/JPG/PNG

### P21-25: 内置常量补全 (159个)

**修改文件**:
- `src/semantics/semantic_analyzer.cpp` — 添加159个内置常量:
  - 比较常量: vbBinaryCompare, vbTextCompare, vbDatabaseCompare (3个)
  - StrConv常量: vbUpperCase, vbLowerCase, vbProperCase, vbWide, vbNarrow等 (9个)
  - 文件属性常量: vbNormal, vbReadOnly, vbHidden, vbSystem, vbVolume, vbDirectory, vbArchive, vbAlias (8个)
  - 文件模式常量: vbInput, vbOutput, vbRandom, vbAppend, vbBinary (5个)
  - VarType扩展: vbDecimal, vbUserDefinedType, vbArray, vbDataObject (4个)
  - MsgBox扩展: vbMsgBoxSetForeground, vbMsgBoxRight, vbMsgBoxRtlReading, vbDefaultButton5 (4个)
  - 星期常量: vbSunday~vbSaturday + vbUseSystemDayOfWeek (8个)
  - FirstWeekOfYear: vbFirstJan1, vbFirstFourDays, vbFirstFullWeek (3个)
  - Calendar: vbCalGreg, vbCalHijri (2个)
  - QueryClose: vbFormControlMenu, vbFormCode, vbAppWindows等 (6个)
  - 修饰键常量: vbShiftMask, vbCtrlMask, vbAltMask (3个)
  - 鼠标按钮: vbLeftButton, vbRightButton, vbMiddleButton (3个)
  - 按键码常量: vbKey0~vbKey9, vbKeyA~vbKeyZ, vbKeyF1~vbKeyF16, vbKeyNumpad0~9等 (100+个)
  - 其他: vbObjectError, vbNullString (2个)

### P2.5/P19-6: 状态清理

- P2.5(默认属性解析): 已由P17.1实现，标记为已完成
- P19-6(COM For Each): 已由P20-02实现，标记为已完成

## 构建记录

1. RTL libs重建 (build_rtl_libs2.bat) — 成功
2. Touch c3rtl.rc — 强制RC重编译
3. C3.exe增量构建 — 成功
4. 回归测试 74/74 — 全部通过

## 修改文件清单

| 文件 | 修改内容 |
|------|----------|
| src/rtl/core/vb6rtl.h | 注册表函数声明 + LoadPictureEx声明 + parray成员 |
| src/rtl/core/vb6rtl.c | 注册表实现 + LoadPictureEx实现 + olectl.h |
| src/backend/cgen_expr.cpp | 注册表映射 + GetSetting padding + IsMissing拦截 + _has_标志 + ByRef Variant修复 + LoadPictureEx映射 |
| src/backend/cgen_decl.cpp | makeParamList添加_has_标志 |
| src/backend/msvc_driver.cpp | 链接行添加advapi32.lib |
| src/semantics/semantic_analyzer.cpp | 注册表函数注册 + 159个内置常量 |
| ai/004-进度表.md | P20-36/37, P21-18/25, P2.5, P19-6状态更新 |
