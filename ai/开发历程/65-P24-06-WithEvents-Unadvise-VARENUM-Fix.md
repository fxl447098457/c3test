# P24-06 WithEvents Unadvise 修复 + VARENUM C5286/C5287 警告修复

日期: 2026-07-09

## 背景

P24 COM 优化专题进入收尾阶段。P24-10/11/12 + Bug2 已于上一轮提交 (f52a745)，但 P24-06 (WithEvents Unadvise) 和 VARENUM 警告尚未处理。

## P24-06: WithEvents 连接点泄漏修复

### 问题

当 WithEvents COM 变量被重新赋值 (`Set obj = New X`) 或置空 (`Set obj = Nothing`) 时，旧的 IConnectionPoint::Advise 连接未被断开，导致连接点泄漏。VB6 语义要求每次 Set 赋值前先 Unadvise 旧连接。

### 修复

在 `src/backend/cgen_stmt.cpp` 中，WithEvents 变量的 Set 赋值代码生成逻辑，在 `vb6_ComAdvise` 调用之前插入:

```c
if (cookie != 0) {
    vb6_ComUnadvise(pUnk, &diid, sink, &cookie);
    cookie = 0;
}
```

即: 先检查 cookie 是否非零(有旧连接)，如果有则先 Unadvise 断开旧连接，再 Advise 建立新连接。

### 影响范围

WithEvents 三种路径均受益:
1. 内部 VB6 类事件 (P6.5)
2. 窗体控件事件 (P16)
3. 外部 COM 对象事件 (P13.23)

## VARENUM C5286/C5287 警告修复

### 问题

MSVC /W3 下，`vb6rtl.c` 中 Variant 比较函数的 `switch(v->vt)` 和 `v->vt == VT_BSTR` 触发 C5286 (VARENUM→vb6_vartype 隐式转换) 和 C5287 (不同枚举类型比较) 警告，共 13 处。

### 修复

在 `src/rtl/core/vb6rtl.c` 中:
- `switch (v->vt)` → `switch ((vb6_vartype)v->vt)`
- `case VT_I2:` → `case (vb6_vartype)VT_I2:` (6 个 case)
- `v->vt == VT_BSTR` → `v->vt == (vb6_vartype)VT_BSTR`
- `vb.vt = VT_I4` → `vb.vt = (vb6_vartype)VT_I4` (6 个 VarCmpLong 函数)

修复后 MSVC 编译零警告零错误。

### RTL 重建

修改 `vb6rtl.c` 后重新编译 x64/x86 obj 和 lib，touch c3rtl.rc 强制资源重嵌入，rebuild c3.exe。

## 验证

- 82/82 回归测试零失败
- MSVC 编译 vb6rtl.c 零警告

## 变更文件

| 文件 | 变更 |
|------|------|
| src/backend/cgen_stmt.cpp | WithEvents Set 赋值前插入 Unadvise 检查 |
| src/rtl/core/vb6rtl.c | VARENUM→vb6_vartype 显式转换 (13处) |
| src/rtl/lib/vb6rtl.lib (x64+x86) | 重建 |
| ai/004-进度表.md | P24-06/Bug2 状态更新 |
