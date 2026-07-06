# P24-04: VB_GlobalNameSpace — frxParse编译专题

> 日期: 2026-07-06
> 里程碑: P24 COM优化专题
> 基线: 74/74 回归零失败
> 结果: 74/74 回归零失败, frxParse成功编译为253KB x86 exe

## 背景

frxParse demo项目使用VBMAN ActiveX DLL的`VBMAN.Version()`函数。该调用依赖VB6的VB_GlobalNameSpace特性: ActiveX DLL中标记了`VB_GlobalNameSpace = True`的coclass (sGlobal) 的Public方法被提升到全局作用域, 使`VBMAN.Version()`等价于`sGlobal.VBMAN().Version()`。

## 修复清单

### Bug #1: Variant变量COM晚绑定 (前期已完成)
- **现象**: `For Each b In ImageList1.ListImages: dic.Add CStr(b.Index), b.Picture` 编译失败
- **根因**: 三个子问题: (1) knownVariantVars_未在MemberAccessExpr中检查 (2) isComMarker_状态污染 (3) comPackExpr缺少Variant分支
- **修复文件**: cgen_expr.cpp, cgen_util.cpp, vb6rtl.h/c

### Bug #2: VBP Reference路径解析回退 (前期已完成)
- **现象**: `loadByClsid`对VBMAN失败(未regsvr32注册)
- **根因**: VBP Reference行含path字段但未被用作loadByPath回退
- **修复文件**: driver.cpp

### Bug #3: VBP Reference 5字段格式 (前期已完成)
- **现象**: VBMAN.dll TypeLib加载后coclasses=0
- **根因**: VBP Reference行实际是5字段(`*\G{GUID}#ver#lcid#path#desc`), parser期望6字段
- **修复文件**: vbp_parser.cpp

### Bug #4: VB_GlobalNameSpace — 核心突破 (本轮完成)

#### 发现过程
- VBMAN.dll TypeLib解析成功(93 coclasses, 113 interfaces)
- `sGlobal` coclass: flags=0x000b (FDEFAULT|FCANCREATE|FPREDECLID), 默认接口`_sGlobal`
- `_sGlobal` dispatch接口: func[7]=`VBMAN` memid=1610809344 invkind=1
- VB_GlobalNameSpace在TypeLib中无特殊标记, 需启发式检测

#### 检测启发式
- coclass名含"Global" (不区分大小写)
- 或: 默认接口有方法名与TypeLib项目名匹配
- 检测到后设`isGlobalNamespace=true`

#### 实现架构

**TypeLib层 (typelib_parser.hpp/.cpp)**:
- `ComCoClassInfo::isGlobalNamespace` 标志
- `TypeLibResult::typeLibProjectName` 存储TypeLib项目名
- `parseTypeLib()`末尾: 链接coclass→defaultInterface → GlobalNameSpace检测
- 清理了8个debug fprintf语句

**符号层 (symbol_table.hpp)**:
- `SymbolKind::ComGlobalNs` 新符号类别
- `Symbol::comGlobalNsMethodName` 存储提升的方法名

**驱动层 (driver.cpp)**:
- GlobalNameSpace coclass的默认接口custom方法注册为独立ComGlobalNs符号
- 每个promoted方法一个符号, 复用comClsidStr/comProgId/comMethods等字段

**语义层 (semantic_analyzer.cpp)**:
- resolveTypeRef: ComGlobalNs → Vb6Type::Object
- MemberAccessExpr: ComGlobalNs对象 → Vb6Type::Variant (晚绑定)

**代码生成层 (cgen_expr.cpp)**:
- MemberAccessExpr: ComGlobalNs → 生成`vb6_ComCallObject(vb6_CreateObject(L"ProgId"), L"Method", NULL, 0)`作为comObjExpr_, 设置isComMarker_供后续消费
- IdentifierExpr: ComGlobalNs独立使用 → 直接生成COM调用表达式

#### 生成的C代码 (frxParse示例)
```c
// VB6: Me.Caption = "Power by vbman - " & VBMAN.Version()
vb6_ComCallObject(
    vb6_CreateObject(L"VBMANLIB.sGlobal"),  // 创建sGlobal单例
    L"VBMAN", NULL, 0)                       // 调用VBMAN() → 返回cVBMAN
→ vb6_ComCall(..., L"Version", NULL, 0)      // 在cVBMAN上调用.Version()
```

## 修改文件清单

| 文件 | 改动 |
|------|------|
| src/com/typelib_parser.hpp | +isGlobalNamespace, +typeLibProjectName |
| src/com/typelib_parser.cpp | +GlobalNameSpace检测后处理, +coclass→iface链接, -8个debug fprintf |
| src/semantics/symbol_table.hpp | +SymbolKind::ComGlobalNs, +comGlobalNsMethodName |
| src/driver/driver.cpp | +GlobalNameSpace promoted方法注册循环 |
| src/semantics/semantic_analyzer.cpp | +ComGlobalNs在resolveTypeRef/MemberAccessExpr |
| src/backend/cgen_expr.cpp | +ComGlobalNs在MemberAccessExpr/IdentifierExpr |

## 关键设计决策

1. **ComGlobalNs而非扩展ComModule**: promoted方法在语义上是函数调用(返回COM对象), 不是模块级函数命名空间, 独立SymbolKind更清晰
2. **复用comClsidStr/comProgId/comMethods**: 避免为ComGlobalNs新增重复字段
3. **用vb6_CreateObject而非新增RTL函数**: sGlobal的ProgID可通过ProgIDFromCLSID获取, 无需额外的vb6_ComCreateByClsid
4. **检测启发式: 名含Global或方法名匹配TLB名**: 覆盖sGlobal和典型VB6命名模式

## 已知限制

- 每次调用VBMAN()都重新CreateObject, 非真正的PredeclaredId单例行为 — 功能正确但效率可优化
- TYPEFLAG_FAPPOBJECT(0x0100)未用于检测 — sGlobal实际不设此标志, 但其他库可能使用
- 链式调用如`VBMAN.Version().Length`未被测试 — 但现有COM marker链式机制应能支持
