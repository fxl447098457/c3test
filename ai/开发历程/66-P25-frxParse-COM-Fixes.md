# P25 - frxParse端到端运行修复（COM属性/枚举/比较）

> 日期: 2026-07-10
> Git: a840fbd → a6a733d → 9611154 → 376a9b0 → 5985fd8
> 状态: 全部完成, 82/82回归零失败

## 背景

frxParse是C3编译器的核心demo项目，包含MSComctlLib.ImageList(47图) + Scripting.Dictionary + VBMAN ActiveX等高级COM用法。此前编译能通过但运行时有4个COM代码生成Bug，导致功能异常或程序挂起。

## P25 主commit: COM参数化属性PUT/MsgBox Variant转换/CStr COM适配/resolveComMarkerForPack

4处编译错误修复:
1. `dic.Item("hello") = "world"` → `vb6_ComSetPropArg()` (新增RTL函数，参数化属性PUT)
2. MsgBox参数传递: `vb6_MsgBox(args[0], ...)` 而非 `vb6_MsgBox(argList, ...)` (args[0]是BSTR)
3. `CStr(b.Index)` → `vb6_CStr(vb6_VariantFromComResult(vb6_ComGetProp(...)))` (COM Variant转String)
4. COM arg pack 6处resolveComMarkerForPack处理

## P25-fix: 后期绑定COM属性访问统一返回VARIANT

- 症状: `dic.Add CStr(b.Index), b.Picture` 生成垃圾key导致重复key错误
- 根因: late-bound COM var property access全部走GetStringProp，对integer属性(ListImage.Index)返回垃圾BSTR
- 修复: `vb6_VariantFromComResult(vb6_ComGetProp(obj, L"Prop"))` 返回VARIANT
- 改3处(cgen_expr.cpp L2042/2045/2048) + 1处(L1098 MemberAccessExpr for Variant vars)

## P25-fix2: ForEach_Next跳过VT_EMPTY/VT_NULL/VT_ERROR

- 症状: ImageList.ListImages._NewEnum返回49项(47真实+2 VT_EMPTY占位符), VB6静默跳过
- 修复: vb6_ForEach_Next改为for(;;)循环，遇到VT_EMPTY/VT_NULL/VT_ERROR则continue跳过

## P25-fix3: COM getter rvalue在比较中取地址非法

- 症状: `If i > ImageList1.ListImages.Count Then i = 1` 生成 `&vb6_ComGetIntProp(...)` — 取rvalue地址非法
- 修复: 检测COM getter结果的推断类型，修正为实际返回类型(Long/Double/String/Object)走C比较运算符
- 对纯VARIANT rvalue(如vb6_VariantFromComResult)，生成临时变量 `_vcmp_N` 再取 `&`
- 新增 `vcmpCounter_` 成员到 cgen.hpp

## P25-fix4: ForEach Count安全上限—修复IEnumVARIANT无限循环

### 根因（关键发现）

通过添加fprintf调试日志到vb6_ForEach_Next，发现关键事实:

```
ForEach_Next call#1 hr=0x00000000 fetched=1 vt=0x0009   (VT_DISPATCH)
ForEach_Next call#2 hr=0x00000000 fetched=1 vt=0x0009
...（100万+次，永不停止）
```

**MSComctlLib.ImageList的IEnumVARIANT实现有致命Bug**: `Next()` 永远返回 `S_OK + fetched=1 + VT_DISPATCH`，枚举游标从不前进，永远不会返回S_FALSE。

这导致P25-fix2引入的for(;;)跳空循环变成死循环——因为每次Next()都"成功"返回VT_DISPATCH项(非VT_EMPTY)，循环永不终止。

### 修复方案

模仿VB6原生运行时行为: VB6通过ICollection::get_Count获取总项数来限制枚举范围。

- `vb6_ForEach_Init`: 除了获取_NewEnum接口，还读取集合的Count属性(通过GetIDsOfNames("Count") + Invoke(DISPATCH_PROPERTYGET))
- 包装结构体 `vb6_ForEachState`: 存储 `{IEnumVARIANT* pEnum; int32_t totalCount; int32_t fetchedCount;}`
- `vb6_ForEach_Next`: 当 `fetchedCount >= totalCount` 时强制返回0，截断无限枚举
- `vb6_ForEach_Release`: 释放包装结构体 + 释放IEnumVARIANT

### 隔离测试过程

1. 注释整个Form_Load → 窗体正常显示 "Running"
2. 恢复Form_Load但注释VBMAN → 仍然 "Not Responding"
3. 注释For Each循环 → 窗体正常
4. 恢复For Each但注释dic.Add → 仍然挂死 → 确认是For Each本身
5. 加fprintf日志 → 发现Next()永不终止，1,043,504次调用后才手动kill

### 验证结果

- frxParse完整运行: 窗体正常显示, 标题 "Power by vbman - v1.0.458"
- Timer动画正常(Picture2每秒切换ImageList图片)
- Dictionary正确填充47个entry
- 82/82回归测试零失败

## 修改文件

| 文件 | 修改内容 |
|------|---------|
| src/backend/cgen_expr.cpp | MsgBox BSTR转换/resolveComMarkerForPack/CStr COM适配/late-bound VARIANT返回/COM getter rvalue比较 |
| src/backend/cgen_stmt.cpp | 参数化COM属性赋值vb6_ComSetPropArg + knownTypedComVars_ |
| src/backend/cgen_util.cpp | resolveComMarkerForPack()实现 |
| src/backend/cgen.hpp | resolveComMarkerForPack声明 + vcmpCounter_成员 |
| src/rtl/core/vb6com.c | vb6_ComSetPropArg + vb6_ForEachState结构体 + Count安全上限 + 跳空逻辑 |
| src/rtl/core/vb6com.h | vb6_ComSetPropArg声明 |
| src/rtl/core/vb6rtl.h | vb6_ComSetPropArg声明 |
| src/rtl/lib/vb6rtl.lib | x64重建 |
| src/rtl/lib/x86/vb6rtl.lib | x86重建 |

## 经验教训

1. **COM组件的IEnumVARIANT不一定可靠**: MSComctl.OCX是1998年的组件，其IEnumVARIANT实现有Bug
2. **VB6运行时有防御性设计**: 用Count做枚举上限是VB6的长期策略，我们也必须这么做
3. **调试RTL死循环**: fprintf写文件比OutputDebugString更可靠，不会因为消息循环阻塞而看不到输出
4. **换行符/编码要特别注意**: 连续多次用PowerShell编辑GBK编码的VB6 .frm文件，需要小心行号偏移和重复行
