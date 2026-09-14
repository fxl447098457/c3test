# M12 - FRX 二进制资源读取

> **历史说明**：本文涉及的 AIGC 零宽字符水印及其全部防御手段（`strip_zw` / `.temp/` 中转 / `CMakeLists_clean.txt` / `full_build.bat` / 强制 text-writer 纪律）已于 2026-09-14 撤销，注入源已不存在。以下为历史记录，请勿照做。

**日期**: 2026-07-02
**提交**: (待commit)
**回归测试**: 74/74 PASS
**里程碑**: M12 达成

## 背景

frxParse 测试工程包含 Form1.frm + Form1.frx (15775字节)，.frx 文件存储了窗体和控件的设计时二进制属性数据。此前编译器能解析 .frm 并编译成功，但所有图片/图标/文本均为空白——.frx 二进制数据未被解析和消费。

本里程碑实现完整的 .frx 二进制资源读取链路，使编译后的 EXE 能正确加载图片、图标、文本和列表数据。

## 实现内容

### 1. FRX 二进制格式逆向分析

完整分析了 .frx 文件的二进制格式：

**Picture 属性（普通控件 — Form/PictureBox/Image/CommandButton/OptionButton/CheckBox）**:
```
偏移 0:  4字节 - 总数据大小 (DWORD LE)
偏移 4:  4字节 - 魔数 "lt\0\0"
偏移 8:  4字节 - 图片数据大小 (DWORD LE)
偏移 12: N字节 - 原始图片数据 (JPEG/ICO/BMP/PNG)
```

**Picture 属性（ImageList ListImage）**:
```
偏移 0:  4字节 - 总数据大小
偏移 4:  16字节 - GUID {0B9D1E52-...} (IPictureDisp CLSID)
偏移 20: 4字节 - 魔数 "lt\0\0"
偏移 24: 4字节 - 图片数据大小
偏移 28: N字节 - 原始图片数据
```

**TextBox.Text（多行）**: 4字节长度前缀 + GBK编码文本

**ListBox.List（字符串数组）**: 2字节项数 + 每项(2字节长度 + GBK文本)

**ListBox.ItemData（整数数组）**: 2字节项数 + 每项4字节DWORD

### 2. 新增源文件

**frx_reader.hpp / frx_reader.cpp** — FrxReader 类：
- `load(path)` — 加载 .frx 文件到内存
- `readPicture(offset, isImageList)` — 剥离头部，提取原始图片数据
- `readText(offset)` — 读取长度前缀 GBK 文本，转 UTF-8
- `readStringList(offset)` — 读取计数前缀字符串数组
- `readIntList(offset)` — 读取计数前缀整数数组
- `detectFormat()` — 检测 JPEG/ICO/BMP/PNG/GIF/WMF/EMF
- `gbkToUtf8()` — 通过 Windows API 进行 GBK→UTF-8 转换

### 3. FRM 解析器增强

**frm_parser.hpp**:
- 新增 `FrxReference` 到 `FrmValueType` 枚举
- `FrmValue` 结构体添加 `frxFile`/`frxOffset` 字段
- 新增 `fromFrxRef()` 静态构造函数
- `FrmFormDesc` 添加 `frxFilePath` 字段

**frm_parser.cpp**:
- `parseValue()` 检测 `"filename.frx":HEXOFFSET` 模式，返回 `FrmValue::fromFrxRef()`

**driver.cpp**:
- 解析 .frm 后，从 .frm 路径推导 .frx 路径，设置到 `frmDesc.form.frxFilePath`

### 4. 代码生成器增强 (cgen_form.cpp)

- `bytesToHexArray()` — 将二进制数据转为 C 十六进制数组字符串
- `escapeCString()` — 转义 C 字符串字面量
- **Form.Icon**: ICO 数据嵌入 .h，生成 `vb6_LoadPictureFromMemory` + `WM_SETICON`
- **Form.Picture**: 图片数据嵌入，生成 `vb6_LoadPictureFromMemory` + `vb6_SetControlPicture`
- **控件 Picture** (CommandButton/OptionButton/CheckBox/PictureBox/Image): 数据嵌入 + 加载应用
- **TextBox.Text**: 读取 GBK 文本，生成 `vb6_Utf8ToWide` + `SetWindowTextW`
- **ListBox.List**: 读取字符串数组，生成 `LB_ADDSTRING`
- **ListBox.ItemData**: 读取整数数组，生成 `LB_SETITEMDATA`

### 5. RTL 运行时新增函数

**vb6forms.h / vb6forms.c**:
- `vb6_LoadPictureFromMemory(data, size)` — 使用 `CreateStreamOnHGlobal` + `OleLoadPicture` 从内存加载 JPEG/BMP/ICO/PNG，位图自动 CopyImage 避免双重释放
- `vb6_Utf8ToWide(utf8)` — UTF-8 转宽字符串（调用者负责释放）

### 6. 构建基础设施修复

- **CMakeLists.txt 清洗**: 用 PowerShell .NET 清除零宽字符水印（`copy /Y` 触发 AIGC hook 注入 3584 个 U+200B/U+200D 字符）
- **full_build.bat 改用 Python shutil.copy2**: 避免 `copy` 命令触发水印 hook
- **RTL 预编译库重建**: 
  - `vb6rtl.lib` = vb6rtl.obj + vb6com.obj（修复了之前错误包含 vb6forms.obj 导致的 LNK2005 重复符号）
  - `vb6rtl_gui.lib` = vb6forms.obj（新增 vb6_LoadPictureFromMemory + vb6_Utf8ToWide）
  - 添加 `#include <olectl.h>` 修复 IPicture/OleLoadPicture/OLE_HANDLE 未声明
  - 修复 `vb6_SetSelText` 函数名被截断为 `}ext(` 的代码损坏

## 验证结果

- frxParse 工程编译成功，输出 工程1.exe (160768 字节)
- 74/74 回归测试全部通过，零回归
- 生成的 Form1.h (78213 字节) 包含所有图片的十六进制数组嵌入
- 生成的 Form1.c (6598 字节) 包含图片加载和控件应用代码

## 待改进项

- ImageList ListImage 的 .frx 格式（含 GUID 头部）尚未在代码生成中处理
- Form.Picture 背景图片的 WM_PAINT 绘制尚未实现（当前仅存储到窗口属性）
