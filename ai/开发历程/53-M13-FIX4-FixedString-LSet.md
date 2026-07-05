---
AIGC:
  ContentProducer: '001191110102MAD55U9H0F10002'
  ContentPropagator: '001191110102MAD55U9H0F10002'
  Label: '1'
  ProduceID: 'b2b69a25-4aac-4b79-a4b5-f360a47dccf2'
  PropagateID: 'b2b69a25-4aac-4b79-a4b5-f360a47dccf2'
  ReservedCode1: '7bca1796-ea86-4fee-a702-7496f6400e0f'
  ReservedCode2: '7bca1796-ea86-4fee-a702-7496f6400e0f'
---

# M13-FIX4: 定长字符串 String*N + LSet 修复

**日期**: 2026-07-05
**里程碑**: M13 (兼容性收尾)
**回归测试**: 74/74 零失败

## 问题

frxParse demo 运行时 List1 只显示时间，没有"等等"字样。

VB6 源码:
``vb
Dim a As String * 10
LSet a = "等等"
List1.AddItem a & Now
``

## 根因分析

两个 bug 协同导致:

### Bug1: String*N 初始化为 BSTR NULL

cgen_stmt.cpp line 2440-2441:
``cpp
} else if (var.asType && var.asType->kind == ASTNodeKind::FixedStringTypeRef) {
    initVal = "NULL";  // Fixed-length string: BSTR initially NULL
``

生成代码: BSTR a = NULL;

但在 VB6 中, Dim a As String * 10 创建长度10的定长字符串缓冲区, 初始内容为10个空格。

### Bug2: LSet 使用 SysStringLen(NULL) = 0

cgen_stmt.cpp line 163:
``cpp
c_.emitLine("vb6_BSTR_Assign(&" + tgtC + ", vb6_LSet(" + valExpr + ", SysStringLen(" + tgtC + ")));");
``

生成: b6_BSTR_Assign(&a, vb6_LSet(source, SysStringLen(a)));
因为 a=NULL, SysStringLen(NULL)=0, vb6_LSet 返回空字符串。

## 修复方案

### 1. RTL: vb6_BSTR_FixedSTR(int32_t len) (vb6rtl.h)

新增 inline 函数, 创建 len 个空格的 BSTR:
``c
static inline BSTR vb6_BSTR_FixedSTR(int32_t len) {
    if (len <= 0) return vb6_BSTR_Empty();
    BSTR bstr = SysAllocStringLen(NULL, len);
    if (bstr) { for (int32_t i = 0; i < len; i++) bstr[i] = L' '; }
    return bstr;
}
``

### 2. 初始化修改 (cgen_stmt.cpp ~L2448)

``cpp
} else if (var.asType && var.asType->kind == ASTNodeKind::FixedStringTypeRef) {
    auto& fs = static_cast<FixedStringTypeRef&>(*var.asType);
    emitExpr(*fs.length);
    std::string fsLen = lastExpr_;
    initVal = "vb6_BSTR_FixedSTR(" + fsLen + ")";
    knownFixedStringLen_[fsLower] = fsLen;
``

### 3. LSet/RSet 使用固定长度 (cgen_stmt.cpp ~L162)

``cpp
std::string strLen;
auto fsIt = knownFixedStringLen_.find(tgtLower);
if (fsIt != knownFixedStringLen_.end()) {
    strLen = fsIt->second;
} else {
    strLen = "SysStringLen(" + tgtC + ")";
}
``

### 4. 变量跟踪 (cgen.hpp + cgen_decl.cpp)

- cgen.hpp: std::unordered_map<std::string, std::string> knownFixedStringLen_;
- cgen_decl.cpp: 3处过程入口 clear + VariableDecl 注册定长字符串

## 生成代码对比

**修复前:**
``c
BSTR a = NULL;  // String * 10 → NULL, SysStringLen=0
vb6_BSTR_Assign(&a, vb6_LSet(vb6_BSTR_FromStr(L"等等"), SysStringLen(a)));  // 长度0!
``

**修复后:**
``c
BSTR a = vb6_BSTR_FixedSTR(10);  // String * 10 → 10个空格的BSTR
vb6_BSTR_Assign(&a, vb6_LSet(vb6_BSTR_FromStr(L"等等"), 10));  // 使用固定长度10
``

## 验证

- frxParse List1 正确显示"等等"+时间 ✓
- 74/74 回归测试零失败 ✓

> AI生成