---
AIGC:
  ContentProducer: '001191110102MAD55U9H0F10002'
  ContentPropagator: '001191110102MAD55U9H0F10002'
  Label: '1'
  ProduceID: 'ea92ecdf-a648-4d07-8314-592468dd28e0'
  PropagateID: 'ea92ecdf-a648-4d07-8314-592468dd28e0'
  ReservedCode1: '1a4af5b8-8e5f-440d-bf86-472b88e88434'
  ReservedCode2: '1a4af5b8-8e5f-440d-bf86-472b88e88434'
---

---
AIGC:
  ContentProducer: '001191110102MAD55U9H0F10002'
  ContentPropagator: '001191110102MAD55U9H0F10002'
  Label: '1'
  ProduceID: '93dc214b-81ba-40ce-9241-669e948746ae'
  PropagateID: '93dc214b-81ba-40ce-9241-669e948746ae'
  ReservedCode1: '2ff1d3b2-b793-4a5b-a57e-3a6d71ee53ec'
  ReservedCode2: '2ff1d3b2-b793-4a5b-a57e-3a6d71ee53ec'
---

---
AIGC:
  ContentProducer: '001191110102MAD55U9H0F10002'
  ContentPropagator: '001191110102MAD55U9H0F10002'
  Label: '1'
  ProduceID: '98bae649-9ebd-4b57-95c9-ed5a2598c553'
  PropagateID: '98bae649-9ebd-4b57-95c9-ed5a2598c553'
  ReservedCode1: 'e087ffd8-e003-405e-a254-20917827de66'
  ReservedCode2: 'e087ffd8-e003-405e-a254-20917827de66'
---

# P24-05 补充修复: TypeLib路径去重 + ComGlobalNs冲突容错

日期: 2026-07-07
阶段: P24-05 (frxParse OLEAUT32崩溃修复 — 收尾)
关联: 57-P24-05-OLEAUT32-CrashFix-BSTR-TypeConfusion.md

## 问题背景

P24-05 的 Bug 2 (TypeLib import失败) 修复在上一轮被 revert, 因为引入了新错误:
```
error VB3002: 重复声明: 'vbman'
```

## 根因分析

### 调试过程

1. **重新应用 loadByClsid 修复** — 先按 TypeLib ID 直接查找 HKCR\TypeLib\{guid}, 失败再走 CLSID 反查
2. **编译 c3 → 编译 frxParse** — 确认复现 "重复声明: vbman" 错误
3. **符号表调试** — 在 symbol_table.cpp define() 中加 fprintf, 发现冲突类型:
   ```
   DEFINE CONFLICT: new=ComGlobalNs existing=ComGlobalNs name=vbman
   ```
   冲突双方都是 ComGlobalNs, 不是之前预估的 ComModule vs ComGlobalNs
4. **driver.cpp 调试** — 在 Loop 4 (ComGlobalNs 注册) 加 fprintf, 发现 sGlobal coclass 被遍历两次:
   ```
   LOOP4 CC: name=sGlobal isGlobalNs=1 hasDefIface=1 numMembers=8
   LOOP4 CC: name=sGlobal isGlobalNs=1 hasDefIface=1 numMembers=8
   ```
5. **loadByPath 路径调试** — 发现同一 VBMAN.dll 被加载了两次:
   - `D:\code\vi\vbmanlib\vbman\dist\DLL\VBMAN.dll` (通过 loadByProgId / Reference 行)
   - `C:\VBMANLIB\_bin\DLL\VBMAN.dll` (通过 loadByClsid / Object 行 TypeLib ID 查注册表)
   - 两条路径指向同一 DLL (junction 目录), 缓存比较 tlbPath 字符串不匹配 → 双重加载

### 根因链

```
VBP文件 Reference=*G#...#VBMAN.dll → loadByProgId → findTypeLibPathForProgId → 路径A
VBP文件 Object=...#VBMAN.dll → loadByClsid(新修复) → HKCR\TypeLib\{guid} → 路径B
路径A != 路径B (同DLL的不同路径) → 缓存未命中 → VBMAN TypeLib 加载两次
→ cachedResults() 包含两个相同结果 → Loop3 注册 ComModule("vbman") 两次
→ Loop4 注册 ComGlobalNs("vbman") 两次 → 符号表 define() 冲突
```

## 修复内容 (2个Bug)

### Bug 8: TypeLib 同 DLL 不同路径导致缓存未命中

**文件**: `src/com/typelib_parser.cpp` loadByPath()
**修复**: 在 loadByPath() 入口处用 GetLongPathNameW 规范化路径 + 转小写, 用于缓存比较
- 在 TypeLibResult 结构体中新增 canonPath 字段 (规范化路径)
- 缓存比较从 `cached->tlbPath == tlbPath` 改为 `cached->canonPath == canonPath`

### Bug 9: symbol_table ComGlobalNs 未加入冲突容错

**文件**: `src/semantics/symbol_table.cpp` define()
**修复**: 将 ComGlobalNs 加入冲突解决逻辑的两个路径:
1. Line 119: `isBuiltin && (ComClass || ComInterface || ComModule || ComGlobalNs)` → 已有同名则 skip
2. Line 128-129: existing 是 COM 类型包括 ComGlobalNs, 且 new 不是 COM 类型 → erase 允许覆盖

这是防御性修复: 即使主修复(Bug 8)解决了去重问题, 符号表也应正确处理 ComGlobalNs 冲突。

## 验证结果

- frxParse 编译: ✅ 通过 (之前报 "重复声明: vbman")
- frxParse 运行: ✅ 窗口标题 "Power by vbman - v1.0.457" 正常
- 回归测试: ✅ 76/76 零失败

## 修改文件清单

| 文件 | 改动 |
|------|------|
| src/com/typelib_parser.cpp | loadByClsid: 先按TypeLib ID查再按CLSID反查; loadByPath: GetLongPathNameW规范路径+小写缓存比较 |
| src/com/typelib_parser.hpp | TypeLibResult 新增 canonPath 字段 |
| src/semantics/symbol_table.cpp | define(): ComGlobalNs 加入 isBuiltin skip + existing erase 逻辑 |
| ai/004-进度表.md | P24-05 行更新为9个Bug |

## P24-05 完整修复列表 (9个Bug)

| # | Bug | 文件 | 影响 |
|---|-----|------|------|
| B1 | ActiveX控件ProgID错误→CLSID直用 | cgen_form.cpp | 编译期 |
| B2 | TypeLib import: loadByClsid先按TypeLib ID查 | typelib_parser.cpp | 编译期 |
| B3 | CLSID C初始化格式 | cgen_form.cpp | 编译期 |
| B4 | vb6_ComPackObject缺AddRef→双重释放 | vb6com.c | 运行时 |
| B5 | vb6_ComSetProp VARIANT泄漏 | vb6com.c | 运行时 |
| B6 | ComCall返回值未释放 | cgen_form.cpp | 运行时 |
| B7 | wrapToBSTR/wrapVariantValue BSTR→VARIANT*类型混淆 | cgen_expr.cpp, cgen_util.cpp | 运行时(崩溃) |
| B8 | TypeLib同DLL不同路径→缓存未命中→双重注册 | typelib_parser.cpp | 编译期 |
| B9 | ComGlobalNs未加入符号表冲突容错 | symbol_table.cpp | 编译期 |

> AI生成

> AI生成

> AI生成