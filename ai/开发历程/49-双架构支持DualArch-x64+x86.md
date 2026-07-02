# M13 双架构支持 (Dual-Architecture x64 + x86)

日期: 2026-07-03

## 背景

MSCOMCTL.OCX (ImageList/ListView/TreeView等) 只有32位版本, x64进程调用 CoCreateInstance 时返回 REGDB_E_CLASSNOTREG (0x80040154)。c3的核心价值是x64编译(VB6原生没有), 但32位COM组件兼容性是刚需, 因此需要支持双架构输出。

## 实现方案

c3编译器本身保持64位, 通过 `--arch x86|x64` 参数选择目标二进制架构:
- 默认 x64 (核心差异化价值)
- `--arch x86` 用于需要加载32位COM组件的兼容场景

### 架构决策

1. **不自动检测** — 不做 `.c3config` 或 VBP 分析, 用户显式指定 `--arch`
2. **RTL双套.lib** — x64和x86各一套预编译静态库, 嵌入c3.exe资源按需提取
3. **Headers arch-neutral** — .h文件不需要区分架构, 只有.lib区分
4. **vcvarsall动态选择** — x86目标调用 `vcvarsall.bat x86`, x64调用 `x64`

## 修改文件清单

| 文件 | 修改内容 |
|------|---------|
| `src/driver/c3rtl.rc` | 新增x86 .lib RCDATA资源 (IDs 120-122) |
| `src/driver/rtl_embedded.hpp` | 新增x86资源ID枚举, create()加arch参数 |
| `src/driver/rtl_embedded.cpp` | create(arch)按架构提取对应.lib |
| `src/driver/driver.hpp` | CompileOptions新增arch字段(默认"x64"), target默认改"win-x64" |
| `src/driver/driver.cpp` | parseArgs加--arch解析+校验, session.create传arch, runLinker传arch, help文本 |
| `src/backend/msvc_driver.hpp` | MsvcDriverOptions新增arch字段, buildVcvarsPrefix加arch参数 |
| `src/backend/msvc_driver.cpp` | buildVcvarsPrefix(arch)动态vcvarsall, compileAndLink加/MACHINE:X86 |

## 技术细节

### c3rtl.rc 资源ID分配

```
// x64 (原有)
110 RCDATA "../rtl/lib/vb6rtl.lib"
111 RCDATA "../rtl/lib/vb6rtl_dll.lib"
112 RCDATA "../rtl/lib/vb6rtl_gui.lib"

// x86 (新增)
120 RCDATA "../rtl/lib/x86/vb6rtl.lib"
121 RCDATA "../rtl/lib/x86/vb6rtl_dll.lib"
122 RCDATA "../rtl/lib/x86/vb6rtl_gui.lib"
```

### SessionManager::create(arch) 提取逻辑

- Headers (IDs 100-106): 总是提取, arch-neutral
- x64 .libs (IDs 110-112): arch=="x64" 时提取
- x86 .libs (IDs 120-122): arch=="x86" 时提取
- .lib文件名相同 (vb6rtl.lib等), 提取到同一个rtlDir, MsvcDriver不需要知道架构差异

### MSVC驱动架构感知

1. `buildVcvarsPrefix("x86")` → `call vcvarsall.bat x86`
2. `buildVcvarsPrefix("x64")` → `call vcvarsall.bat x64`
3. x86目标添加 `/MACHINE:X86` 链接器标志
4. .lib路径从session rtlDir获取 (已是正确架构的.lib)

## 测试结果

| 测试项 | 结果 |
|--------|------|
| x64 74/74回归测试 | PASS |
| x86 hello.bas编译运行 | PASS |
| x86 test_rtl.bas编译运行 | ALL TESTS PASSED |
| x86 GUI form编译 | PASS (PE Machine=0x14C确认) |
| x64默认输出 | PASS (PE Machine=0x8664确认) |
| --arch无效值 | 正确报错 |
| --help显示--arch | 正确显示 |

## 编译器能力总结 (截至M13)

- **语言**: VB6完整语法 (条件编译、Pratt表达式、软关键字、嵌套类型、UDT/Enum/Class)
- **COM**: CreateObject/GetObject/TypeLib导入/前期绑定/后期绑定/ActiveX DLL/Implements/WithEvents
- **窗体**: Win32窗体+13+控件+控件数组+MDI+菜单+WebView2+Timer+ScrollBar
- **双架构**: x64 (默认) + x86 (--arch x86, 用于32位COM组件如MSCOMCTL.OCX)
- **RTL**: BSTR/VARIANT/SAFEARRAY/文件I/O/错误处理/字符串/数学/日期/类型转换/COM运行时/窗体运行时
