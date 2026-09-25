# DirListBox 控件

# DirListBox 控件

               

在运行时，**DirListBox** 控件显示目录和路径。这个控件可以显示分层的目录列表。例如，可以创建对话框，在所有可用目录中，从文件列表打开一个文件。

**语法**

**DirListBox**

**说明**

设置 **List**、**ListCount** 和 **ListIndex** 属性，就可以访问列表中的项目。如果需要显示 **DriveListBox** 和 **FileListBox** 控件，那么可以编写代码，使它们与 **DirListBox** 同步，并使它们之间彼此同步。
## 本项目的实现口径（ai/029 C29-1b）

`DirListBox` 在原生的 **`LISTBOX`**（带 `LBS_NOTIFY`）上实现，语法面与本页上文一致。

| 写法 | 读数 |
| --- | --- |
| `Dir1.Path` | 当前目录；赋新值**立刻重刷**列表 |
| `Dir1.List(j)` | 每一项写成 `[名字]` —— 方括号就是"这是个目录"的记号（与 VB6 同），列表里不含 `.` 与 `..` |
| 设计期 `.frm` 里的 `Path = "..."` | 建窗时即按该目录填表；没写则退回当前目录 |
| 事件 | `Change`（`Path` 变了）、`Click`；**双击一项即下钻一层**，下钻成功才发 `Change` |

因此本页上文那条"使它们彼此同步"的惯用写法在这里就是：

```vb
Private Sub Drive1_Change()
    Dir1.Path = Drive1.Drive      ' 赋 Path 会自动重刷，并触发 Dir1_Change
End Sub

Private Sub Dir1_Change()
    File1.Path = Dir1.Path
End Sub
```

判据在 `tests\ctrlfiles\`（CF4 认 `[名字]` 约定、CF5 认设计期 `Path`、CF12 / CF13 认两条联动），通知接线由 `cf_emitc_shape` 断发码形状（含下钻的那道前置判定）。
本实现里**单击**一项只发 `Click`、不改 `Path`（也就不发 `Change`）；下钻靠双击。
尚未落地：卷标行（VB6 在列表顶部列一行**不带**方括号的卷名）、`Hidden` / `System` 那类属性过滤开关。
