# ImageList 控件（MSComctlLib）

> 组件：Microsoft Windows Common Controls 6.0 (SP6)，MSCOMCTL.OCX

## 1. 概述
非可视的**图像列表容器**，本身不显示，只负责存一组同尺寸图片，为
Toolbar（按钮图标）、TreeView（节点图标）、ListView（大/小图标）、
StatusBar（面板图标）等提供图像源。

## 2. 主要属性

| 属性 | 说明 |
|---|---|
| `ListImages` | 图像集合（核心），成员为 ListImage 对象 |
| `ImageWidth / ImageHeight` | 图像尺寸，由**第一张加入的图**决定，之后加入的图自动统一为该尺寸 |
| `(Name) / Index / Tag` | 通用属性 |

**ListImage 成员：** `Index`、`Key`（字符串标识）、`Picture`（图像对象）。

## 3. 方法
- `ListImages.Add([index], [key], [picture])` —— 添加图像
- `ListImages.Remove(index 或 key)` —— 删除
- `ListImages.Clear` —— 清空

## 4. 事件
**无事件**（纯数据容器）。

## 5. 使用要点
- 设计期通过属性页 "Images" 标签加图；运行期用 `ListImages.Add`。
- 支持 ico / bmp / gif / jpg / wmf 等（`LoadPicture` 能加载的格式）。
- 不同用途建议不同尺寸：Toolbar / TreeView / StatusBar 用 **16×16**；
  ListView 大图标用 **32×32**。尺寸不同就放**多个 ImageList**。
- 引用方式：`Toolbar1.ImageList = ImageList1`、`TreeView1.ImageList = ImageList1`、
  `ListView1.Icons / SmallIcons / ColumnHeaderIcons = ImageListX`。

## 6. 示例

```vb
ImageList1.ListImages.Add , "Open", LoadPicture(App.Path & "\open.ico")
ImageList1.ListImages.Add , "Save", LoadPicture(App.Path & "\save.ico")

Set Toolbar1.ImageList = ImageList1
Toolbar1.Buttons.Add , "Open", "打开", tbrDefault, "Open"   ' Image 参数可给 Key
```