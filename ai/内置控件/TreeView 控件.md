# TreeView 控件（MSComctlLib）

> 组件：Microsoft Windows Common Controls 6.0 (SP6)，MSCOMCTL.OCX

## 1. 概述
树形（层级）列表控件，用于显示目录结构、组织架构、分类导航等。
数据单位为 **Node（节点）**，通过 `Nodes` 集合管理。

## 2. 主要属性

| 属性 | 说明 |
|---|---|
| `Nodes` | 节点集合（核心） |
| `SelectedItem` | （只读）当前选中节点 |
| `ImageList` | 关联的 ImageList（Node.Image / SelectedImage 取其索引或 Key） |
| `Style` | 0 - tvwTreelineOnly 文本+树线；1 - tvwTextOnly 仅文本；2 - tvwTreelinePicture 树线+图标；3 - tvwTreelinePictureText 树线+图标+文本（最常用） |
| `LineStyle` | 0 - tvwTreeLines 默认；1 - tvwRootLines 根节点也画线 |
| `Indentation` | 子节点缩进量 |
| `LabelEdit` | 0 - tvwAutomatic 单击选中节点标题可编辑；1 - tvwManual 手动 |
| `CheckBoxes` | True 时每个节点带复选框 |
| `HotTracking` | True 时鼠标悬停标题呈超链接样式 |
| `PathSeparator` | 节点路径分隔符（默认 "\\"） |

**Node 对象常用成员：**
- 属性：`Key`、`Text`、`Image`、`SelectedImage`、`Tag`、`Index`、`Bold`、
  `Expanded`（是否展开）、`Selected`、`Sorted`、`Checked`（CheckBoxes=True 时）
- 关系导航：`Parent`、`Root`、`Child`、`Children`（子节点数）、
  `FirstSibling`、`LastSibling`、`Next`、`Previous`
- 方法：`EnsureVisible`（滚动到可见并展开父链）、`Remove`（亦可用 Nodes.Remove）

**Nodes.Add 签名：**
`Add([relative], [relationship], [key], [text], [image], [selectedimage])`
relationship 常量：`tvwFirst=0`、`tvwLast=1`、`tvwNext=2`、`tvwPrevious=3`、`tvwChild=4`

## 3. 事件

| 事件 | 说明 |
|---|---|
| `NodeClick(Node)` | **核心**：点击节点触发 |
| `BeforeLabelEdit / AfterLabelEdit(Cancel, Node)` | 编辑标题前/后（可 Cancel） |
| `Expand / Collapse(Node)` | 展开/折叠后 |
| `BeforeExpand / BeforeCollapse(Cancel, Node)` | 展开/折叠前（可 Cancel） |
| 通用 | Click、DblClick、KeyDown/Press/Up、OLEDragDrop 系列等 |

## 4. 示例

```vb
Private Sub Form_Load()
    Dim nRoot As Node, n As Node
    TreeView1.CheckBoxes = True
    Set nRoot = TreeView1.Nodes.Add(, , "dept", "全部部门", 1, 1)
    Set n = TreeView1.Nodes.Add("dept", tvwChild, "sales", "销售部", 2, 2)
    TreeView1.Nodes.Add "sales", tvwChild, , "张三", 3, 3
    TreeView1.Nodes.Add "sales", tvwChild, , "李四", 3, 3
    nRoot.Expanded = True
End Sub

Private Sub TreeView1_NodeClick(ByVal Node As MSComctlLib.Node)
    StatusBar1.Panels(1).Text = "选中：" & Node.Text
End Sub

' 收集所有打勾节点
Private Function CheckedKeys() As String
    Dim n As Node
    For Each n In TreeView1.Nodes
        If n.Checked Then CheckedKeys = CheckedKeys & n.Key & ","
    Next
End Function
```