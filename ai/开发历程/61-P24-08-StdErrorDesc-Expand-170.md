---
AIGC:
  ContentProducer: '001191110102MAD55U9H0F10002'
  ContentPropagator: '001191110102MAD55U9H0F10002'
  Label: '1'
  ProduceID: '9a77ac4f-9684-40ae-b5eb-7cba1d91fa82'
  PropagateID: '9a77ac4f-9684-40ae-b5eb-7cba1d91fa82'
  ReservedCode1: '60d4e16f-e92a-460a-b6c6-0973a6e40424'
  ReservedCode2: '60d4e16f-e92a-460a-b6c6-0973a6e40424'
---

# P24-08 Phase4: VB6标准错误号映射表扩展 (60→170+)

日期: 2026-07-08
阶段: P24-08 Phase4 (COM错误传播 - 错误描述表扩展)
关联: 60-P24-08-COM-ErrorDesc-i18n-GUI-MessageBox.md

## 背景

P24-08 Phase3 实现了 `vb6_StdErrorDesc()` 五级 fallback 错误描述，但硬编码的 switch-case 只有 60 个条目（ErrNum 5~746），覆盖不全，且存在错误号映射错误。

## 数据源分析

从 `MSVBVM60.DLL` STRINGTABLE 逆向提取所有 VB6 标准错误描述：

### DLL STRINGTABLE ID 规则（关键发现）

1. **经典运行时错误 (ErrNum 0~100)**: STRINGTABLE ID = 10000 + DLL序列号
   - 序列号 ≠ 错误号！必须按文本内容交叉匹配
   - 例: DLL seq 0 → ErrNum 3 ("Return without GoSub")

2. **窗体/控件/扩展错误 (ErrNum 310+)**: STRINGTABLE ID = 错误号（直接对应）
   - 例: ID 1321 = VB6 Err 1321 = "Invalid file format"

3. **DLL 仅英文**: 切换语言 ID 无中文变体，中文描述需自行翻译

### 提取方法

1. 编写 C 提取工具 (`extract_strings.c`) — 扫描 ID 1~10000
2. 编写第二版 (`extract_strings2.c`) — 扫描 ID 10000~51000
3. 结果: ~312 条唯一错误描述（经典 127 条 + 窗体/控件 185 条）

### 错误号修正

| 旧错误号 | 旧描述 | 新错误号 | 新描述（DLL验证） |
|---------|--------|---------|-----------------|
| 735 | 无法将文件保存到TEMP | 485 | 无效的图片类型 |
| 744 | 找不到搜索文本 | 486 | 不能从指定文件加载图片 |
| 746 | 替换内容过长 | 487 | 不能将图片保存为指定格式 |
| 321 | 无效的文件格式 | 310 | 无效的文件格式（321对应另一个上下文） |

## 实现内容

### vb6_StdErrorDesc 扩展 (60 → 170+ 条目)

按分类组织，覆盖 ErrNum 3~1728：

- **经典运行时错误** (3~94): 28 条 — Return without GoSub、Overflow、Type mismatch 等
- **扩展运行时错误** (97~99): 3 条 — Friend过程、私有对象引用、断开连接
- **资源文件错误** (310~331): 13 条 — Invalid file format、资源文件格式等
- **组件注册错误** (335~345): 11 条 — 注册表访问、ActiveX注册、控件数组
- **窗体/对象加载错误** (360~374): 13 条 — 对象加载/卸载、License、版本兼容
- **属性错误** (380~399): 15 条 — 属性值、只读、运行时限制
- **模态窗体错误** (400~406): 6 条 — 模态显示约束
- **对象权限错误** (419~424): 6 条 — 对象引用、属性查找
- **COM/ActiveX错误** (429~463): 21 条 — ActiveX创建、自动化、参数
- **图片/打印机错误** (481~490): 10 条 — 图片格式、打印
- **剪贴板错误** (520~521): 2 条
- **DDE错误** (1260~1298): 39 条 — DDE通信完整覆盖
- **文件名/资源错误** (1320~1326): 7 条
- **组件/控件扩展错误** (1335~1728): ~20 条 — 控件集合、自动化

### Bug修复

- **vb6_CreateObject 重复调用**: `vb6_RaiseError(429, desc429)` 在 CoCreateInstance 失败路径出现两次，删除重复行

## 修改文件

| 文件 | 修改内容 |
|------|---------|
| `src/rtl/core/vb6com.c` | vb6_StdErrorDesc 从 60 扩展到 170+ 条目，修正 3 个错误号，修复重复 RaiseError |
| `src/rtl/lib/vb6rtl.lib` | x64 重建 |
| `src/rtl/lib/vb6rtl_dll.lib` | x64 重建 |
| `src/rtl/lib/vb6rtl_gui.lib` | x64 重建 |
| `src/rtl/lib/x86/vb6rtl.lib` | x86 重建 |
| `src/rtl/lib/x86/vb6rtl_dll.lib` | x86 重建 |
| `src/rtl/lib/x86/vb6rtl_gui.lib` | x86 重建 |
| `src/driver/c3rtl.rc` | touch 强制 RC 重嵌入 |

## 测试结果

76/76 回归测试全部通过，零失败。

## 后续改进方向

当前方案是 switch-case 硬编码，优势是零依赖、启动快。可考虑的改进：
- **STRINGTABLE 资源嵌入 c3.exe**: 与现有 RC 资源嵌入架构一致，支持多语言，最接近 VB6 原版做法
- **错误表完整性**: 当前 ~170 条已覆盖绝大多数常见场景，如需完整可后续补充

> AI生成