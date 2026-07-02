# FRX 文件解析专题

## 1. 什么是 .frx 文件

.frx 是 VB6 窗体的**二进制资源附属文件**，与 .frm 同目录同名（如 `Form1.frm` → `Form1.frx`）。当 .frm 中某些属性的值无法用纯文本表示时，VB6 将二进制数据写入 .frx，并在 .frm 中记录引用格式 `"Form1.frx":HEXOFFSET`。

**只有存在二进制资源时才会产生 .frx 文件**，纯文本窗体不会有 .frx。

---

## 2. 哪些控件属性会产生 .frx 引用

### 2.1 图片类属性（最常见）

| 控件 | 属性 | .frx 数据类型 | 支持的图片格式 |
|------|------|---------------|---------------|
| Form | Icon | Picture (normal) | ICO |
| Form | Picture | Picture (normal) | JPEG/BMP/PNG/GIF/WMF/EMF |
| CommandButton | Picture (Style=1) | Picture (normal) | 同上 |
| CommandButton | DownPicture (Style=1) | Picture (normal) | 同上 |
| CommandButton | DisabledPicture (Style=1) | Picture (normal) | 同上 |
| CheckBox | Picture (Style=1) | Picture (normal) | 同上 |
| CheckBox | DownPicture (Style=1) | Picture (normal) | 同上 |
| CheckBox | DisabledPicture (Style=1) | Picture (normal) | 同上 |
| OptionButton | Picture (Style=1) | Picture (normal) | 同上 |
| OptionButton | DownPicture (Style=1) | Picture (normal) | 同上 |
| OptionButton | DisabledPicture (Style=1) | Picture (normal) | 同上 |
| PictureBox | Picture | Picture (normal) | 同上 |
| Image | Picture | Picture (normal) | 同上 |
| ImageList | ListImage[i] | Picture (**ImageList**) | 同上（有 GUID 头部） |
| Toolbar | Button[i].Image 等间接引用 | — | 通过 ImageList 间接 |
| TabStrip | Image 等间接引用 | — | 通过 ImageList 间接 |
| ListView | Icons/SmallIcons 等间接引用 | — | 通过 ImageList 间接 |
| TreeView | ImageList 等间接引用 | — | 通过 ImageList 间接 |
| Slider | TickPicture | Picture (normal) | 同上 |
| FlatScrollBar | PictureUp/PictureDown 等 | Picture (normal) | 同上 |
| 任何控件 | MouseIcon | Picture (normal) | ICO/CUR |
| 任何控件 | DragIcon | Picture (normal) | ICO/CUR |

**关键区别**：
- **Picture (normal)**：12字节头部 `[4B totalSize] [4B magic "lt\0\0"] [4B imgSize] [data]`
- **Picture (ImageList)**：28字节头部 `[4B totalSize] [16B GUID] [4B magic "lt\0\0"] [4B imgSize] [data]`
  - GUID = `{0B9D1E52-E304-11CE-9DE3-00AA004BB851}`（VB6 ImageList 标准格式标识）

### 2.2 文本类属性

| 控件 | 属性 | .frx 数据类型 | 说明 |
|------|------|---------------|------|
| TextBox | Text (MultiLine=-1 时) | Text | 多行文本用 .frx 存储；单行 Text 仍在 .frm 内 |

单行 TextBox 的 Text 直接写在 .frm 里（`Text = "hello"`），只有 MultiLine=True 时才可能用 .frx（因为文本包含 CRLF 换行符）。

### 2.3 列表类属性

| 控件 | 属性 | .frx 数据类型 | 说明 |
|------|------|---------------|------|
| ListBox | List | ListString | 字符串数组（GBK 编码） |
| ListBox | ItemData | ListInt | 整数数组 |
| ComboBox | List | ListString | 同 ListBox |
| ComboBox | ItemData | ListInt | 同 ListBox |

**注意**：只有设计时在属性窗口中输入的 List/ItemData 才写入 .frx。如果代码中用 `AddItem` 添加的项不在此列。

### 2.4 其他可能的 .frx 引用

| 控件 | 属性 | 说明 |
|------|------|------|
| Menu | Picture (VB6 后期版本) | 菜单项图片 |
| StatusBar | Panel[i].Picture | 状态栏面板图片 |
| ProgressBar | Picture (某些版本) | — |

---

## 3. .frx 二进制格式详解

### 3.1 整体结构

.frx 是一个**连续的二进制仓库**，各属性的数据按 .frm 中出现的顺序依次写入，每个属性的数据块从各自的偏移量开始。偏移量以十六进制记录在 .frm 中。

```
Form1.frx 的布局示例:
┌──────────┐ 0x0000  Form.Icon (ICO, 4290 bytes)
│  ICO数据  │
├──────────┤ 0x10CA  Form.Picture (JPEG, 3991 bytes)
│  JPEG数据 │
├──────────┤ 0x27C4  Text1.Text (多行文本, 39 bytes + 4B header)
│  文本数据  │
├──────────┤ 0x27DE  List1.ItemData (3个整数, 10 bytes)
│  整数列表  │
├──────────┤ 0x27EB  List1.List (3个字符串, 22 bytes)
│  字符串列表│
├──────────┤ 0x2801  Check1(2).Picture (JPEG)
│  JPEG数据 │
└──────────┘ ...后续图片数据
```

### 3.2 Picture 属性格式（普通控件）

```
偏移 +0:  [4 bytes] totalSize     — LE DWORD, 整个数据块的大小
偏移 +4:  [4 bytes] magic         — "lt\0\0" (0x6C 0x74 0x00 0x00)
偏移 +8:  [4 bytes] imgSize       — LE DWORD, 图片数据的字节数
偏移 +12: [N bytes] imageData     — 原始图片数据 (JPEG/ICO/BMP/PNG/GIF/WMF/EMF)
```

**头部固定 12 字节**，图片数据从 offset+12 开始，长度为 imgSize。

**imgSize 不包含头部**，totalSize 通常等于 imgSize + 8（去掉 magic）或 imgSize + 12。

### 3.3 Picture 属性格式（ImageList）

```
偏移 +0:  [4 bytes]  totalSize    — LE DWORD
偏移 +4:  [16 bytes] GUID         — {0B9D1E52-E304-11CE-9DE3-00AA004BB851}
偏移 +20: [4 bytes]  magic        — "lt\0\0"
偏移 +24: [4 bytes]  imgSize      — LE DWORD
偏移 +28: [N bytes]  imageData    — 原始图片数据
```

**头部固定 28 字节**。GUID 的原始字节序列（LE）：
`52 1E 9D 0B 04 E3 CE 11 9D E3 00 AA 00 4B B8 51`

### 3.4 Text 属性格式（TextBox 多行文本）

```
偏移 +0:  [4 bytes] textLen       — LE DWORD, 文本字节数
偏移 +4:  [N bytes] textData      — GBK (Code Page 936) 编码的文本
```

**实测确认**：frxParse 的 Text1.Text 在 offset 0x27C4：
- 字节 `27 00 00 00` = textLen=39
- 后跟 39 字节 GBK 文本 `"Text1\r\n大师的\r\n佛挡杀佛\r\n"`

**注意**：早期分析曾认为有 1 字节长度前缀的格式，但实测 4 字节 DWORD 长度是正确的主格式。FrxReader 中保留 1 字节回退逻辑作为容错。

### 3.5 ListString 属性格式（ListBox/ComboBox.List）

```
偏移 +0:  [2 bytes] itemCount     — LE WORD, 列表项数
偏移 +2:  [2 bytes] prefix        — LE WORD, 值等于 itemCount（额外前缀）
偏移 +4:  逐项:
    [2 bytes] strLen              — LE WORD, 本项字符串的字节长度
    [N bytes] strData             — GBK 编码的字符串
```

**关键发现**：VB6 .frx 的 List 格式在 count 之后有一个**额外的 2 字节前缀**（值等于 itemCount），真正的字符串数据从 offset+4 开始。这个前缀在早期实现中被遗漏，导致解析偏移错误。

**实测验证**（frxParse List1.List at 0x27EB）：
```
03 00   ← count=3
04 00   ← prefix=4（注意：不是第1项的strLen！）
04 00   ← strLen=4（第1项）
31 32 33 34   ← "1234"
04 00   ← strLen=4（第2项）
34 35 36 37   ← "4567"
04 00   ← strLen=4（第3项）
C4 E3 BA C3   ← "你好" (GBK)
```

总消耗：2 + 2 + (2+4) + (2+4) + (2+4) = 22 字节

### 3.6 ListInt 属性格式（ListBox/ComboBox.ItemData）

```
偏移 +0:  [2 bytes] itemCount     — LE WORD
偏移 +2:  [2 bytes] prefix        — LE WORD, 值等于 itemCount（额外前缀）
偏移 +4:  逐项:
    [2 bytes] value               — LE WORD (VB6 Integer, 16位有符号)
```

**同样有 2 字节前缀**，与 ListString 格式一致。

**实测验证**（frxParse List1.ItemData at 0x27DE）：
```
03 00   ← count=3
03 00   ← prefix=3
01 00   ← value=1
30 01   ← value=304
00 30   ← value=12288
```

### 3.7 图片格式魔数检测

| 格式 | 魔数 (前4字节) | 说明 |
|------|---------------|------|
| JPEG | `FF D8 FF` | JFIF/Exif |
| ICO | `00 00 01 00` | Windows 图标 |
| BMP | `42 4D` ("BM") | Windows 位图 |
| PNG | `89 50 4E 47` | PNG 签名 |
| GIF | `47 49 46 38` ("GIF8") | GIF87a/GIF89a |
| WMF | `D7 CD C6 9A` | Windows 元文件 |
| EMF | `01 00 00 00` | 增强元文件 (需 size>=6 确认) |

---

## 4. 编译器中的 FRX 处理流程

### 4.1 完整数据流

```
┌─────────────────────────────────────────────────────────────────┐
│ .frm 文本文件                                                    │
│   Picture = "Form1.frx":10CA                                    │
└──────────┬──────────────────────────────────────────────────────┘
           │ FrmParser::parseValue() [frm_parser.cpp:36-53]
           │ 识别模式: "filename.frx":HEXOFFSET
           │ → FrmValue::fromFrxRef(file, offset, raw)
           ▼
┌─────────────────────────────────────────────────────────────────┐
│ FrmControl.properties["Picture"]                                 │
│   type = FrxReference                                            │
│   frxFile = "Form1.frx"                                         │
│   frxOffset = 0x10CA                                            │
└──────────┬──────────────────────────────────────────────────────┘
           │ driver.cpp:632-640
           │ frmDesc.frmFilePath.replace_extension(".frx")
           │ if exists → frmDesc.form.frxFilePath = frxPath
           ▼
┌─────────────────────────────────────────────────────────────────┐
│ cgen_form.cpp:57-61                                              │
│ FrxReader::load(frxFilePath) → 整个 .frx 读入 fileData_         │
└──────────┬──────────────────────────────────────────────────────┘
           │ cgen_form.cpp 按属性类型分发
           │
    ┌──────┼──────────┬──────────────┬──────────────┐
    ▼      ▼          ▼              ▼              ▼
  Icon   Picture     Text          List         ItemData
  readP  readP      readText   readStringList  readIntList
    │      │          │              │              │
    ▼      ▼          ▼              ▼              ▼
  .h:    .h:       .c:            .c:            .c:
  hex[]  hex[]   Utf8ToWide()  LB_ADDSTRING   LB_SETITEMDATA
    │      │          │              │              │
    ▼      ▼          ▼              ▼              ▼
  WM_    WM_      SetWindowText  ListBox       ListBox
  SETICON PAINT   W()            项添加        数据设置
```

### 4.2 代码生成模式

#### 图片属性 → hex 数组嵌入 .h + 运行时加载

```c
// .h 文件中生成:
static const unsigned char vb6_frx_pic_Command1[] = { 0xFF, 0xD8, 0xFF, ... };
static const int vb6_frx_pic_Command1_size = 1234;

// .c 文件中生成:
{ void* vb6_pic = vb6_LoadPictureFromMemory(vb6_frx_pic_Command1, vb6_frx_pic_Command1_size);
  if (vb6_pic) SendMessageW((HWND)vb6_hwnd_Command1, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)vb6_pic); }
```

#### 文本属性 → GBK→UTF-8 转换 + SetWindowTextW

```c
{ wchar_t* vb6_wtxt = vb6_Utf8ToWide("Text1\r\n大师的\r\n佛挡杀佛\r\n");
  if (vb6_wtxt) { SetWindowTextW((HWND)vb6_hwnd_Text1, vb6_wtxt); free(vb6_wtxt); } }
```

#### 列表属性 → 逐项 LB_ADDSTRING

```c
{ wchar_t* vb6_witem = vb6_Utf8ToWide("1234");
  if (vb6_witem) { SendMessageW((HWND)vb6_hwnd_List1, LB_ADDSTRING, 0, (LPARAM)vb6_witem); free(vb6_witem); } }
```

---

## 5. FrxReader 类 API 参考

### 头文件: `src/project/frx_reader.hpp`

```cpp
namespace vb6c3 {

// 数据类型枚举
enum class FrxDataType { Picture, PictureImageList, Text, ListString, ListInt, Unknown };

// 图片格式枚举
enum class FrxImageFormat { JPEG, ICO, BMP, PNG, WMF, EMF, GIF, Unknown };

// 结果结构体
struct FrxPictureData { std::vector<uint8_t> data; FrxImageFormat format; };
struct FrxTextData    { std::string text; };     // UTF-8
struct FrxListData    { std::vector<std::string> items; };  // UTF-8
struct FrxIntListData { std::vector<int> items; };

class FrxReader {
public:
    static bool load(const std::filesystem::path& frxPath);
    static FrxPictureData readPicture(size_t offset, bool isImageList = false);
    static FrxTextData readText(size_t offset);
    static FrxListData readStringList(size_t offset);
    static FrxIntListData readIntList(size_t offset);
    static const std::string& lastError();
    static FrxImageFormat detectFormat(const uint8_t* data, size_t size);
    static std::string gbkToUtf8(const std::string& gbk);
private:
    static std::vector<uint8_t> fileData_;
    static std::string lastError_;
    static uint32_t readLE32(const uint8_t* p);
    static uint16_t readLE16(const uint8_t* p);
};

} // namespace vb6c3
```

### 设计要点

- **全静态类**：`fileData_` 为静态成员，一次 `load()` 后所有 `read*()` 方法共享同一份数据
- **GBK→UTF-8**：所有文本读取后自动转换为 UTF-8，由 `gbkToUtf8()` 使用 Win32 API `MultiByteToWideChar(936, ...)` 实现
- **非 Windows 平台**：`gbkToUtf8()` 返回原字符串（不转换），需要额外处理
- **无内存泄露**：`vb6_Utf8ToWide()` 返回的 `wchar_t*` 由调用方 `free()` 释放；`vb6_LoadPictureFromMemory()` 对 BITMAP 做 BitBlt 复制（IPicture 拥有原始 handle），对 ICON 直接返回 handle

---

## 6. RTL 运行时函数

### vb6_LoadPictureFromMemory()

```c
void* vb6_LoadPictureFromMemory(const void* data, int size);
```

**流程**：
1. COM 初始化（`OleInitialize` 或 `CoInitialize` 回退）
2. `GlobalAlloc` + `CreateStreamOnHGlobal` 创建 IStream
3. `OleLoadPicture(pStream, size, FALSE, &IID_IPicture, &pPicture)` 加载 IPicture
4. `get_Handle()` + `get_Type()` 获取底层 handle 和类型
5. **picType==1 (BITMAP)**：`BitBlt` 复制到新 HBITMAP（因为 IPicture 拥有原 handle）
6. **picType==3 (ICON)** 或其他：直接返回 handle
7. Release IPicture 和 IStream

### vb6_Utf8ToWide()

```c
wchar_t* vb6_Utf8ToWide(const char* utf8);
```

- `MultiByteToWideChar(CP_UTF8, ...)` 转换
- 返回 `malloc` 分配的 `wchar_t*`，调用方必须 `free()`

---

## 7. cgen_form.cpp 中的控件样式常量

与 FRX 相关的 Win32 样式常量：

| 常量 | 值 | 用途 |
|------|-----|------|
| `kBsBitmap` | 0x00000080 | BS_BITMAP，Graphical 按钮显示图片 |
| `kBsPushLike` | 0x00001000 | BS_PUSHLIKE，CheckBox/OptionButton 按钮外观（凹陷=选中，凸起=未选中） |
| `kEsMulti` | 0x00000004 | ES_MULTILINE，TextBox 多行 |
| `kEsAutoV` | 0x00000040 | ES_AUTOVSCROLL，TextBox 自动垂直滚动 |
| `kWsVscroll` | 0x00200000 | WS_VSCROLL，垂直滚动条 |
| `kWsHscroll` | 0x00100000 | WS_HSCROLL，水平滚动条 |

### Graphical 按钮的完整样式组合

VB6 `Style = 1 'Graphical` 时：

| 控件 | 基础样式 | +Style=1 | 效果 |
|------|---------|----------|------|
| CommandButton | BS_PUSHBUTTON | +BS_BITMAP+BS_PUSHLIKE | 图片按钮（凸起/凹陷） |
| CheckBox | BS_AUTOCHECKBOX | +BS_BITMAP+BS_PUSHLIKE | 图片切换按钮（凸起=未选中，凹陷=选中） |
| OptionButton | BS_AUTORADIOBUTTON | +BS_BITMAP+BS_PUSHLIKE | 图片单选按钮（同上） |

### 图片应用方式

| 控件类型 | 图片加载方式 |
|---------|------------|
| CommandButton/CheckBox/OptionButton (Style=1) | `SendMessageW(hwnd, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBmp)` |
| PictureBox/Image | `vb6_SetControlPicture(hwnd, hBmp)` — 内部用 `SetPropW` + `STM_SETIMAGE` |
| Form.Picture | `SetPropW(hwnd, L"VB6_Picture", (HANDLE)hBmp)` + WM_PAINT 中 StretchBlt |
| Form.Icon | `SendMessageW(hwnd, WM_SETICON, ICON_BIG/ICON_SMALL, (LPARAM)hIcon)` |

---

## 8. 已知问题与历史修复

### 8.1 List/ItemData 2字节前缀问题（已修复）

**问题**：最初假设 List 格式为 `[2B count] [per item: 2B len + data]`，但实际 VB6 .frx 格式有额外的 2 字节前缀（值等于 count），导致所有字符串偏移错位。

**表现**：ListBox 只显示 1 个乱码项而非 3 个正确项。

**修复**：`readStringList()` 和 `readIntList()` 中 pos 从 2 改为 4，跳过 count + prefix。

### 8.2 Graphical 按钮缺少 BS_PUSHLIKE（已修复）

**问题**：只添加了 BS_BITMAP 使按钮能显示图片，但 CheckBox/OptionButton 仍为标准复选框/单选框外观，没有凹陷/凸起效果。

**修复**：添加 BS_PUSHLIKE (0x1000) 样式。

### 8.3 Form.Picture 无 WM_PAINT 绘制（已修复）

**问题**：Form.Picture 通过 SetPropW 存储 HBITMAP，但没有 WM_PAINT 处理来实际绘制背景。

**修复**：在 WndProc 中添加 WM_PAINT case，使用 StretchBlt 从 VB6_Picture 属性绘制背景。

### 8.4 L"VB6_Picture" 引号嵌套（已修复）

**问题**：`emitLine("SetPropW(hwnd, L"VB6_Picture", ...)")` 中嵌套引号导致 C++ 编译器将 `L"VB6_Picture"` 识别为用户定义字面量。

**修复**：正确转义为 `L\"VB6_Picture\"`。

### 8.5 const map operator[]（已修复）

**问题**：在 `const auto& ctrl` 循环中，`ctrl.properties["Style"]` 调用非 const 的 `operator[]`。

**修复**：改用 `ctrl.properties.at("Style")`。

### 8.6 尚未实现的功能

| 功能 | 状态 | 说明 |
|------|------|------|
| ImageList ListImage 运行时应用 | 未实现 | 解析正确但未将图片关联到 ImageList 控件 |
| DownPicture / DisabledPicture | 未实现 | Graphical 按钮的按下/禁用状态图片 |
| MouseIcon / DragIcon | 未实现 | 鼠标指针图标 |
| ComboBox List/ItemData | 未实现 | 格式与 ListBox 相同，但 cgen_form 中未处理 |
| Menu Picture | 未实现 | 菜单项图片 |
| StatusBar Panel.Picture | 未实现 | 状态栏面板图片 |

---

## 9. frxParse 测试工程偏移映射表

frxParse 是用于验证 .frx 解析的测试工程，其 Form1.frx (15775 bytes) 的完整偏移映射：

| 偏移 | 控件.属性 | 数据类型 | 图片格式 | 数据大小 |
|------|----------|---------|---------|---------|
| 0x0000 | Form.Icon | Picture (normal) | ICO | 4290 B |
| 0x10CA | Form.Picture | Picture (normal) | JPEG | 3991 B |
| 0x1A89 | ImageList1.ListImage1 | Picture (ImageList) | JPEG | ~734 B |
| 0x1D9E | ImageList1.ListImage2 | Picture (ImageList) | JPEG | ~733 B |
| 0x20DB | ImageList1.ListImage3 | Picture (ImageList) | JPEG | ~733 B |
| 0x232C | ImageList1.ListImage4 | Picture (ImageList) | JPEG | ~598 B |
| 0x2572 | ImageList1.ListImage5 | Picture (ImageList) | JPEG | ~734 B |
| 0x27C4 | Text1.Text | Text (4B len + GBK) | — | 43 B |
| 0x27DE | List1.ItemData | ListInt (2B prefix) | — | 10 B |
| 0x27EB | List1.List | ListString (2B prefix) | — | 22 B |
| 0x2801 | Check1(2).Picture | Picture (normal) | JPEG | ~735 B |
| 0x2AE4 | Check1(1).Picture | Picture (normal) | JPEG | ~735 B |
| 0x2DCD | Option2.Picture | Picture (normal) | JPEG | ~735 B |
| 0x30A6 | Check1(0).Picture | Picture (normal) | JPEG | ~735 B |
| 0x334C | Option1.Picture | Picture (normal) | JPEG | ~735 B |
| 0x3620 | Command1.Picture | Picture (normal) | JPEG | ~713 B |
| 0x38E5 | Picture1.Picture | Picture (normal) | JPEG | ~543 B |
| 0x3B00 | Image1.Picture | Picture (normal) | JPEG | ~275 B |

---

## 10. 涉及的源码文件

| 文件 | 职责 |
|------|------|
| `src/project/frm_parser.hpp` | FrmValueType::FrxReference 枚举, FrmValue::frxFile/frxOffset 字段, FrmFormDesc::frxFilePath |
| `src/project/frm_parser.cpp` | parseValue() 检测 `"file.frx":HEXOFFSET` 模式 |
| `src/project/frx_reader.hpp` | FrxReader 类声明, 结果结构体, 枚举 |
| `src/project/frx_reader.cpp` | FrxReader 实现: load/readPicture/readText/readStringList/readIntList/gbkToUtf8 |
| `src/backend/cgen_form.cpp` | 代码生成: hex数组/WM_PAINT/BM_SETIMAGE/LB_ADDSTRING/BS_PUSHLIKE 等 |
| `src/driver/driver.cpp` | 设置 frxFilePath (与 .frm 同目录, 扩展名替换) |
| `src/rtl/core/vb6forms.h` | vb6_LoadPictureFromMemory/vb6_Utf8ToWide 声明 |
| `src/rtl/core/vb6forms.c` | OleLoadPicture + MultiByteToWideChar 实现 |
