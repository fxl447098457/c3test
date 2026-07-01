# M22 Issue A/B/C: 三项运行时深度修复

> 日期: 2026-07-01
> Commit: 62e0df5
> 回归: 70/74 (4个前M22遗留UDT/Implements失败)

## 背景与问题

上一轮(M22)修复了6个运行时问题并提交(3e5034e, cec97a9)，但仍有3个问题未完全解决：
- 窗体标题不显示(Me.Caption在Form_Load中设置无效)
- EXE文件名中文乱码(工程1.exe→宸ョ▼1.exe)
- Print到窗体不自动换行

代码修复已在上一轮写入文件但尚未构建测试提交。

## Issue A: 窗体标题不显示

### 根因分析
`CreateFormWindow()` 内部调用 `CreateWindowExA()`，该API触发 `WM_CREATE` 消息，从而执行 `Form_Load()`。但此时 `CreateWindowExA()` 尚未返回，`vb6_hwnd_Form1` 仍为 NULL。

`Form_Load()` 中 `Me.Caption = myName` 被翻译为 `vb6_SetControlText(vb6_hwnd_Form1, myName)`，而 `vb6_hwnd_Form1 == NULL` 导致 `SetWindowTextA` 无效。

### 修复方案
在 `cgen_form.cpp` 的 `emitFormFramework` 中，在调用 `Form_Load()` 之前提前赋值 HWND：

```cpp
c_.emitLine("vb6_hwnd_" + cIdent(formName) + " = (void*)hwnd;  /* early assign for Form_Load */");
```

`WM_CREATE` 处理函数的 `hwnd` 参数就是窗口句柄，在 `CreateWindowExA` 返回前已经有效。

### 修改文件
- `src/backend/cgen_form.cpp` — 添加 early HWND assign

## Issue B: EXE文件名中文乱码

### 根因分析
两层问题叠加：

**层1: CreateProcess vs std::system**
`std::system()` 通过CRT将 `char*` 转为宽字符串，CRT使用当前locale/codepage（可能是ACP=GBK）。
如果命令行中包含UTF-8编码的中文路径，CRT会把UTF-8字节当GBK解释，产生乱码。

**层2: filesystem::path双重编码**
`project.exeName` 的值来自VBP文件，VBP解析器已将其从GBK转为UTF-8。
但 `std::filesystem::path(project.exeName)` 用 `const char*` 构造，MSVC的filesystem实现会按ACP重新解释字节。
UTF-8字节"工程"(E5 B7 A5 E7 A8 8B)被当作GBK解码，得到"宸ョ▼"，这是双重编码。

form demo没有 `ExeName32` 字段，走 `vbpPath`（来自argv，ACP编码），所以不受层2影响。

### 修复方案

**层1修复**: `msvc_driver.cpp` 中 `executeCommand()` 改用 `CreateProcessW()`。
自己控制UTF-8→UTF-16转换(MultiByteToWideChar CP_UTF8)，不依赖CRT的codepage解释。
移除 `utf8ToAcp()` 函数和 `/Fe` 路径的ACP转换。

```cpp
// UTF-8 → UTF-16 → cmd.exe /c <command>
std::wstring wcmd = utf8ToWide(cmd);
std::wstring fullCmd = L"cmd.exe /c " + wcmd;
CreateProcessW(nullptr, &mutableCmd[0], ...);
```

**层2修复**: `driver.cpp` 新增 `utf8ToWide()` 辅助函数。
对于VBP解析器输出的UTF-8字符串，先转为 `wstring` 再构造 `filesystem::path`：

```cpp
// 旧: filesystem::path(project.exeName)  // exeName是UTF-8, 被当作ACP → 双重编码
// 新: filesystem::path(utf8ToWide(project.exeName))  // UTF-8→UTF-16→path, 正确
```

### 修改文件
- `src/backend/msvc_driver.cpp` — CreateProcessW替换std::system, 移除utf8ToAcp
- `src/driver/driver.cpp` — 新增utf8ToWide(), 修复project.exeName路径构造

## Issue C: Print到窗体不自动换行

### 根因分析
VB6行为: 每个 `Print` 语句执行后自动换行(CurrentY移到下一行)。
之前的 `vb6_Form_Print` 只更新 CurrentX，不改变 CurrentY，导致多次 Print 的文字叠在同一行。

### 修复方案
`vb6forms.c` 的 `vb6_Form_Print` 中，绘制文本后：
```c
vb6_SetCurrentY(hwnd, currentY + (float)tm.tmHeight);
vb6_SetCurrentX(hwnd, 0.0f);
```

### 修改文件
- `src/rtl/core/vb6forms.c` — Print后自动换行
- RTL .lib重建: vb6rtl.lib, vb6rtl_gui.lib, vb6rtl_dll.lib

## 构建与验证

1. RTL .lib重建: `.temp/build_rtl_libs2.bat` 成功
2. c3rtl.rc touch触发RC重编译
3. 增量构建c3.exe成功
4. form demo: `工程1.exe` 文件名正确, 141KB
5. realVBP demo: `工程1.exe`（在dist/子目录）文件名正确, 187KB
6. 回归测试: 70/74通过

## 遗留问题

- 4个UDT/Implements回归测试仍为FAIL（M22之前遗留）
- `resolvePath()` 中 `filesystem::path(relativePath)` 对UTF-8字符串存在同样的双重编码潜在风险（当前demo路径为ASCII不受影响）
