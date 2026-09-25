# ListView 控件（MSComctlLib）

> 组件：Microsoft Windows Common Controls 6.0 (SP6)，MSCOMCTL.OCX

## 1. 概述
多视图列表控件（资源管理器右半区那种），数据单位为 **ListItem**，
报表视图下用 **ColumnHeaders** 定义列、**SubItems** 填第 2 列以后的数据。

## 2. 主要属性

| 属性 | 说明 |
|---|---|
| `View` | 0 - lvwIcon 大图标、1 - lvwSmallIcon 小图标、2 - lvwList 列表、3 - lvwReport 报表（最常用） |
| `ColumnHeaders` | 列头集合（报表视图） |
| `ListItems` | 行集合（核心） |
| `SelectedItem / SelectedItems` | 选中项 / 选中项集合（MultiSelect 时） |
| `Icons / SmallIcons / ColumnHeaderIcons` | 分别关联大图标 / 小图标 / 列头图标的 ImageList |
| `GridLines` | 报表视图显示网格线 |
| `FullRowSelect` | 整行高亮 |
| `MultiSelect` | 允许多选 |
| `CheckBoxes` | 每行带复选框 |
| `LabelEdit` | 0 - lvwAutomatic / 1 - lvwManual |
| `Sorted / SortKey / SortOrder` | 是否排序 / 排序列（0=主文本）/ 0 - lvwAscending 升、1 - lvwDescending 降 |
| `AllowColumnReorder` | 报表视图允许用户拖动列头调序 |
| `HideColumnHeaders` | 隐藏列头 |

**ListItem 常用成员：** `Key`、`Text`（第 1 列）、`Index`、`Tag`、`Icon`、
`SmallIcon`、`Selected`、`Checked`、`ListSubItems`；
**注意：`SubItems(i)` 从 1 开始，对应第 2 列**，且只能存字符串。

**集合 Add 签名：**
- `ColumnHeaders.Add([index], [key], [text], [width], [alignment])`
- `ListItems.Add([index], [key], [text], [icon], [smallicon])`

## 3. 事件

| 事件 | 说明 |
|---|---|
| `ItemClick(Item)` | **核心**：点击行触发 |
| `ColumnClick(ColumnHeader)` | 点击列头触发（常用来排序） |
| `ItemDrag(Item)` | 拖动行触发（配合 OLE 拖放） |
| `BeforeLabelEdit / AfterLabelEdit` | 编辑行标题前/后 |
| 通用 | Click、DblClick、KeyDown/Press/Up、OLEDragDrop 系列等 |

## 4. 示例（报表视图 + 点列头排序）

```vb
Private Sub Form_Load()
    Dim itm As MSComctlLib.ListItem
    With ListView1
        .View = lvwReport: .GridLines = True: .FullRowSelect = True
        .ColumnHeaders.Add , , "姓名", 1200
        .ColumnHeaders.Add , , "部门", 900
        .ColumnHeaders.Add , , "工资", 900, lvwColumnRight
    End With
    Data1.Recordset.MoveFirst
    Do While Not Data1.Recordset.EOF
        Set itm = ListView1.ListItems.Add(, , Data1.Recordset!姓名)
        itm.SubItems(1) = Data1.Recordset!部门      ' 第 2 列
        itm.SubItems(2) = Data1.Recordset!工资      ' 第 3 列
        Data1.Recordset.MoveNext
    Loop
End Sub

Private Sub ListView1_ColumnClick(ByVal ColumnHeader As MSComctlLib.ColumnHeader)
    With ListView1
        .SortKey = ColumnHeader.Index - 1          ' 列头 1 → SortKey 0
        .SortOrder = IIf(.SortOrder = lvwAscending, lvwDescending, lvwAscending)
        .Sorted = True
    End With
End Sub
```