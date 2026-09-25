# Data 控件（数据控件）

> VB 内置数据库控件（DAO/Jet），无需引用部件

## 1. 概述
设计期配置即可连接 Access 等数据库，运行时自带 首/前/后/末 导航按钮，
并作为其他控件的数据绑定源（TextBox、Label、DBGrid 等）。

## 2. 主要属性

| 属性 | 说明 |
|---|---|
| `Connect` | 数据库类型："Access"（默认）、"dBASE 5.0"、"Excel 8.0" 等 |
| `DatabaseName` | 数据库文件路径（.mdb 或目录） |
| `RecordSource` | 记录源：表名 / 查询名 / SQL 语句 |
| `RecordsetType` | 0 - Table 表、1 - Dynaset 动态集（默认）、2 - Snapshot 快照 |
| `DefaultType` | 0 - Use ODBC、1 - Use Jet（默认） |
| `Exclusive` | 是否独占打开 |
| `ReadOnly` | 是否只读 |
| `BufferSize` | 一次读取的记录缓冲数 |
| `BOFAction` | 到 BOF 时：0 - Move First（默认）、1 - BOF |
| `EOFAction` | 到 EOF 时：0 - Move Last（默认）、1 - EOF、2 - Add New |
| `Caption / Align / Visible` | 标题文字 / 对齐 / 是否可见 |

## 3. 事件（3 个）

| 事件 | 触发时机与参数 |
|---|---|
| `Validate(Action, Save)` | 移动记录或更新**前**触发，可校验并取消。Action：1 MoveFirst、2 MovePrevious、3 MoveNext、4 MoveLast、5 AddNew、6 Update、0 Cancel 等；Save≠0 表示绑定数据已修改 |
| `Reposition` | 记录指针移动**后**触发（刷新界面、主从联动常用） |
| `Error(DataErr, Response)` | 数据操作出错时触发；Response：0 显示默认错误（默认）、1 忽略继续 |

## 4. 方法
`Refresh` 重新打开记录集｜`UpdateControls` 撤销修改恢复显示｜`UpdateRecord` 将绑定控件内容写入缓冲区

## 5. 数据绑定
其他控件设 `DataSource = Data1`、`DataField = "字段名"` 即自动同步显示/编辑。

## 6. 示例

```vb
Private Sub Data1_Validate(Action As Integer, Save As Integer)
    If Save <> 0 And Len(Trim(txtName.Text)) = 0 Then
        MsgBox "姓名不能为空！"
        Action = 0          ' vbDataActionCancel：取消本次移动/更新
    End If
End Sub

Private Sub Data1_Reposition()
    lblPos.Caption = "记录 " & (Data1.Recordset.AbsolutePosition + 1)
End Sub
```