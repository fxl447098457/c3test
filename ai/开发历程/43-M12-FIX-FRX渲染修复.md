# M12-FIX: FRX资源渲染修复 + Graphical按钮样式

> **历史说明**：本文涉及的 AIGC 零宽字符水印及其全部防御手段（`strip_zw` / `.temp/` 中转 / `CMakeLists_clean.txt` / `full_build.bat` / 强制 text-writer 纪律）已于 2026-09-14 撤销，注入源已不存在。以下为历史记录，请勿照做。

## 日期
2026-07-02

## 概述
在M12 FRX二进制资源读取基础上，修复了frxParse工程编译后运行的4个视觉bug，并优化了Graphical按钮样式。

## 修复内容

### F1: cgen_form.cpp 编译错误修复
- **L"VB6_Picture" 引号嵌套**：emitLine中C++字符串字面量里的宽字符串需要转义引号 `L\"VB6_Picture\"`
- **const map operator[]**：`ctrl.properties["Style"]` 在 const 上下文中非法，改用 `.at()`

### F2: Form.Picture 背景绘制
- 之前只调 `SetPropW` 存储 HBITMAP，没有 WM_PAINT 处理来实际绘制
- 添加 WM_PAINT case，使用 StretchBlt 从 VB6_Picture 属性绘制背景

### F3: Graphical 按钮样式 (Style=1)
- **BS_BITMAP**：已添加，使按钮能显示 BM_SETIMAGE 设置的图片
- **BS_PUSHLIKE** (0x1000)：新增，使 CheckBox/OptionButton 变为按钮外观
  - 选中时凹陷（按下去），未选中时凸起
  - 与 VB6 Graphical 按钮效果一致
- 同时应用于 CommandButton/CheckBox/OptionButton

### F4: TextBox MultiLine/ScrollBars
- MultiLine = -1 → ES_MULTILINE | ES_AUTOVSCROLL
- ScrollBars = 1/2/3 → WS_VSCROLL / WS_HSCROLL
- 滚动条已内嵌在 EDIT 控件中（Win32扁平样式 vs VB6 3D边框，属平台差异，非bug）

### F5: ListBox List/ItemData .frx 格式修复
- **根因**：VB6 .frx 的 List/ItemData 格式有**额外的2字节前缀**（值等于count）
- 修正前格式假设：`[2B count] [per item: 2B len + data]`
- 修正后格式：`[2B count] [2B prefix(=count)] [per item: 2B len + data]`
- readStringList：pos 从 2 改为 4（跳过 count + prefix）
- readIntList：简化为 2B count + 2B prefix + per item 2B Integer

## 验证结果
- frxParse 编译运行正确
- Form.Picture 背景图片正确显示
- Graphical 按钮：选中凹陷、未选中凸起，图片正确
- ListBox：3项 "1234", "4567", "你好" 正确显示
- TextBox：多行文本 + 滚动条正确
- 74/74 回归测试零失败

## 修改文件
- `src/backend/cgen_form.cpp` — BS_PUSHLIKE常量 + Graphical按钮样式 + 编译错误修复
- `src/project/frx_reader.cpp` — readStringList/readIntList 格式修正(2B前缀)
- `CMakeLists.txt` — 零宽字符清洗

## 辅助脚本
- `.temp/fix_cgen_compile.py` — cgen_form.cpp 编译错误修复
- `.temp/fix_frx_reader.py` — frx_reader.cpp 格式修正
- `.temp/fix_graphical_btn.py` — BS_PUSHLIKE 样式修复
