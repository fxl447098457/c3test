# 33 - P20-36 Menu控件属性实现（cgen架构改造makeCtrlHwndArg）

> 日期: 2026-07-01
> 关联: P20-36, 020-P20遗留项任务清单

## 问题

Menu控件没有HWND，但cgen属性读写系统统一使用 `vb6_hwnd_XXX` 单参数模式。
Menu的RTL函数（`vb6_GetMenuCaption`等）需要 `(HMENU hmenu, int menuId)` 双参数。

## 解决方案

### 1. 架构改造：`makeCtrlHwndArg()` 方法

新增 `CCodeGen::makeCtrlHwndArg(ctrlNameLower, ctrlType)` 方法：
- Menu控件 → `(void*)GetMenu((HWND)vb6_hwnd_Form1), 1001`（HMENU+menuId双参数）
- 其他控件 → `vb6_hwnd_XXX`（保持原有行为）

### 2. 属性映射：Menu case

在 `getControlPropReadFn` / `getControlPropWriteFn` 中添加 `FrmControlType::Menu` 分支：
- Caption → vb6_GetMenuCaption / vb6_SetMenuCaption
- Checked → vb6_GetMenuChecked / vb6_SetMenuChecked
- Enabled → vb6_GetMenuEnabled / vb6_SetMenuEnabled
- Visible → vb6_GetMenuVisible / vb6_SetMenuVisible

### 3. 属性访问站点改造

将硬编码 `vb6_hwnd_` 替换为 `makeCtrlHwndArg()` 调用：
- **非数组控件属性读** (IdentifierExpr MemberAccessExpr)
- **默认属性读** (内置Object变量)
- **非数组控件属性写** (AssignmentStmt)
- **默认属性写** (AssignmentStmt)
- **Let控件属性写** (LetStmt)

### 4. With块Menu支持

- With块属性读/写/Let写增加 `FrmControlType::Menu` 分支
- Menu With对象存储小写控件名（供makeCtrlHwndArg使用）
- 跳过HWND tempVar赋值（Menu无HWND，生成 `int tempVar = 0;` 占位）

### 5. 菜单ID收集

在 `emitFormFramework` 开头添加 DFS 遍历，收集菜单名→menuId映射到 `knownMenuIds_`：
- 起始ID=1000，与 `emitMenuItem`/`emitMenuClickDispatch` 一致
- 分隔线(-)不分配ID
- 子菜单容器不分配ID，仅叶子节点分配
- 递归逻辑与 `emitMenuClickDispatch` 完全镜像

## 修改文件

| 文件 | 修改内容 |
|------|---------|
| `src/backend/cgen.hpp` | 新增 `knownMenuIds_`, `knownMenuFormHwnd_`, `makeCtrlHwndArg()` 声明 |
| `src/backend/cgen.cpp` | Menu属性映射 + makeCtrlHwndArg实现 + 5处属性访问改造 + With块Menu支持 + 菜单ID收集 + `#include <functional>` |

## 验证

- 构建成功
- 74/74 回归测试零失败
