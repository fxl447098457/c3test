# 050 - COM ProgID修复 + cgen_expr运算符优先级bug

> 日期: 2026-07-04
> 状态: 已完成
> 验证: frxParse dic.Count=11 弹出正确

## 问题描述

frxParse demo编译运行后，点击 Command1 按钮，MsgBox dic.Count 弹出 0 而非 11。

## 根因分析

### 根因1: TypeLib解析器 ProgID fallback 不精确

`typelib_parser.cpp` L526: `cc->progId = name;` 将 coclass 短名（如 `"Dictionary"`）作为 ProgID，
但 Windows 注册表中 `CLSIDFromProgID("Dictionary")` 失败（0x800401F3），
正确的 ProgID 应为 `"Scripting.Dictionary"`。

### 根因2: cgen_expr.cpp C++ 运算符优先级 bug

三处 `vb6_NewObject` 自动实例化守卫中，修复根因1时引入了 C++ 运算符优先级 bug：

```cpp
// 错误: + 优先级高于 ?:
newExpr = "(void*)vb6_NewObject(L\"" + itCom->second->comProgId.empty() ? itCom->second->name : itCom->second->comProgId + "\")";
```

C++ 解析为：`("前缀" + empty()) ? name : (progId + "\")")`  → 始终返回短名且缺少函数包装。

## 修复方案

### 修复1: typelib_parser.cpp — ProgIDFromCLSID API

用 Windows `ProgIDFromCLSID` API 从 CLSID 反查注册表获取真实 ProgID，fallback 保留短名：

```cpp
CLSID clsid = pTypeAttr->guid;
LPOLESTR progIdW = nullptr;
HRESULT progHr = ProgIDFromCLSID(clsid, &progIdW);
if (SUCCEEDED(progHr) && progIdW) {
    // 宽字符→窄字符串
    cc->progId = progIdStr;
    CoTaskMemFree(progIdW);
} else {
    cc->progId = name;  // fallback
}
```

### 修复2: cgen_expr.cpp — 提取 _progId 局部变量

三处（L280/L298/L320）统一改为先计算 progId 再拼接：

```cpp
const std::string& _progId = itCom->second->comProgId.empty() ? itCom->second->name : itCom->second->comProgId;
newExpr = "(void*)vb6_NewObject(L\"" + _progId + "\")";
```

## 修改文件

| 文件 | 修改内容 |
|------|---------|
| src/com/typelib_parser.cpp | L522-539: ProgIDFromCLSID反查替代name fallback |
| src/backend/cgen_expr.cpp | L280/L298/L320: 三处运算符优先级bug修复 |

## 验证结果

- frxParse `--arch x86` 编译通过
- 生成的 C 代码: `vb6_NewObject(L"Scripting.Dictionary")` (正确)
- 运行: MsgBox 弹出 11 (正确，0-10共11个条目)
