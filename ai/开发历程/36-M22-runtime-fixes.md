# M22: 6个运行时问题修复 + pathToUtf8编码修复

> 日期: 2026-07-01
> 提交: 3e5034e
> 里程碑: M22 实战VB6项目编译+运行时修复

## 背景

M22前阶段(f84a31b)完成了realVBP多模块项目的编译链接零错误, 但编译生成的EXE运行时发现6个问题。

## 修复清单

### Issue 1: EXE中文文件名乱码

**现象**: form demo编译时 `/Fe"D:\vb6pro\dist\demos\form/????1.exe"`, 链接器报LNK1104找不到文件

**根因**: `driver.cpp`中`projectBaseName_`来自`std::filesystem::path::string()`, Windows上返回ACP(GBK)编码, 但`utf8ToAcp()`假设输入是UTF-8。GBK字节被误当UTF-8 → `MultiByteToWideChar(CP_UTF8, ...)`失败 → 产生`????`

**修复**: 在`driver.cpp`添加`pathToUtf8()`辅助函数, 通过`.wstring()`获取UTF-16后再转UTF-8, 确保`projectBaseName_`和`outputDir`始终是UTF-8编码。同时将所有`path.stem().string()`和`absPath.string()`替换为`pathToUtf8()`调用。

**影响文件**: `src/driver/driver.cpp`

### Issue 2: 窗体屏幕居中无效 (StartUpPosition=2)

**现象**: 窗体启动位置不在屏幕中央

**根因**: `cgen_form.cpp`的Show函数将startupPos==2的情况直接用CW_USEDEFAULT

**修复**: 当`startupPos == 2`时, 生成`(GetSystemMetrics(SM_CXSCREEN) - clientWidth) / 2`之类的居中坐标, 覆盖3种窗体类型(MDIForm/MDIChild/normal)

**影响文件**: `src/backend/cgen_form.cpp`

### Issue 3: 菜单点击无响应

**现象**: "测试菜单"点击后不弹出MsgBox

**根因**: `emitMenuClickDispatch`只遍历`menuCtrl.children`, 顶层叶节点菜单(如只有Caption没有子项的Mtest)没有生成WM_COMMAND处理代码

**修复**: 增加`menuCtrl.children.empty()`分支 — 当菜单无子项时, 直接为菜单控件生成Click处理

**影响文件**: `src/backend/cgen_form.cpp`

### Issue 4: Now()显示原始日期数值

**现象**: `Print Now`输出"46204.7"而非"2026/7/1 17:12:26"

**根因**: `vb6_Format()`中`vb6_vtDate` case fall-through到`vb6_vtDouble`, 用`%g`打印OLE日期原始数值

**修复**: 分离`vb6_vtDate`分支, 使用`VariantTimeToSystemTime` + `GetDateFormatW`/`GetTimeFormatW`进行本地化日期时间格式化

**影响文件**: `src/rtl/core/vb6rtl.c`

### Issue 5: Dim As New自动实例化失效

**现象**: `Dim a As New Class1`后Form_Load中`a.Add 3, 5`不生效, 标题未显示结果

**根因**: 
1. `symTab_.lookup("a")`返回NULL — 语义分析结束后`current_`指向模块作用域, 无法向下查找过程作用域中的局部变量
2. 工厂函数命名不一致 — cgen生成`vb6_New_Class1()`但Class1.c定义`vb6_cls_Class1_New()`

**修复**: 
1. 在`cgen_expr.cpp`的IdentifierExpr中添加`foundSym == null`回退检查`knownNewVars_`/`knownClassVars_`
2. 修正3处工厂函数命名从`vb6_New_X`到`vb6_cls_X_New`

**影响文件**: `src/backend/cgen_expr.cpp`

### Issue 6: Option1 Click的Print不输出到窗体

**现象**: `Option1_Click`中`Print Now`无输出

**根因**: 解析器将`Print Now`等同于`Print #filenumber, expr` — Now被当作fileNumber, outputList为空

**修复**: 
1. `ast.hpp`添加`bool isFormPrint = false`到PrintStmt
2. `parser_stmt.cpp`区分`Print expr`(无#)和`Print #n, expr`
3. `cgen_stmt.cpp`对isFormPrint生成`vb6_Form_Print(formHwnd, bstrExpr)`
4. `vb6forms.c/h`实现`vb6_Form_Print()`使用TextOutW + CurrentX/CurrentY跟踪
5. 非BSTR表达式使用`wrapToBSTR()`包装, 如`vb6_Now()`→`vb6_CStrDate(vb6_Now())`

**影响文件**: `src/ast/ast.hpp`, `src/parser/parser_stmt.cpp`, `src/backend/cgen_stmt.cpp`, `src/backend/cgen.hpp`, `src/backend/cgen_form.cpp`, `src/rtl/core/vb6forms.c`, `src/rtl/core/vb6forms.h`

## 关键架构洞察

**符号表作用域问题**: 语义分析结束后, `SymbolTable::current_`指向模块作用域。过程局部变量在子作用域中, `Scope::lookup()`只向上搜索不向下, 导致cgen阶段`symTab_.lookup()`找不到过程局部变量。cgen必须使用自己的跟踪map(`knownNewVars_`, `knownClassVars_`等)。

**Windows路径编码问题**: `std::filesystem::path::string()`在Windows上返回ACP编码, 但编译器内部统一使用UTF-8。所有从`path::string()`获取的字符串如需参与内部处理, 必须通过`pathToUtf8()`转换。

## 测试结果

- 70/74回归通过
- 4个预先存在的失败(test_compat/M6Test/test_implements/M7Test)非本次引入
- 两个demo项目编译链接成功:
  - `dist/demos/realVBP/dist/工程1.exe` (187KB)
  - `dist/demos/form/工程1.exe` (141KB)


## 居中计算修复 (commit cec97a9)

### 问题: cgen_form.cpp 居中代码混合缇/像素+客户区/窗口尺寸

之前的居中修复在 cgen 层面计算:
```c
(GetSystemMetrics(SM_CXSCREEN) - clientWidth) / 2
```

两个致命错误:
1. `clientWidth` 是缇(twips), `GetSystemMetrics` 返回像素(pixels) — 单位混用
2. `clientWidth` 是客户区尺寸, 不含标题栏和边框 — 实际窗口比这大, 导致窗口中心点在(0,0)

### 修复方案: 居中计算移到 RTL 层

cgen 只传 `-1, -1` 标志表示 CenterScreen, RTL 的 `vb6_CreateFormWindow` 在内部:
1. 先 `AdjustWindowRectEx` 得到窗口实际尺寸(winW, winH, 含非客户区)
2. `(GetSystemMetrics(SM_CXSCREEN) - winW) / 2` 正确计算像素坐标
3. 同理修复 `CreateMDIFormWindow` 和 `CreateMDIChildWindow`

### 顺带修复: MsgBox 空参数

`MsgBox "hi", , "title"` 中间省略的 buttons 参数现在解析为 `LiteralExpr(Long, "0")`, 不再崩溃。

回归测试: 70/74 通过, 零新增回归。
