# FileListBox 控件

# FileListBox 控件

               

在运行时，在 **Path** 属性指定的目录中，**FileListBox** 控件将文件定位并列举出来。该控件用来显示所选择文件类型的文件列表。例如，可以在应用程序中创建对话框，通过它选择一个文件或者一组文件。

**语法**

**FileListBox**

**说明**

设置 **List、ListCount** 和 **ListIndex** 属性，可以访问列表中的项目。如果需要显示 **DirListBox** 和 **DriveListBox** 控件，那么可以编写代码，使它们与 **FileListBox** 控件同步，并使它们之间彼此同步。
## 本项目的实现口径（ai/029 C29-1b）

`FileListBox` 在原生的 **`LISTBOX`**（带 `LBS_NOTIFY`）上实现，语法面与本页上文一致。

| 写法 | 读数 |
| --- | --- |
| `File1.Path` | 当前目录；赋新值**立刻重刷**列表 |
| `File1.Pattern` | 缺省 `*.*`；赋新值**立刻按新模式重过滤**（无匹配则列表为空） |
| `File1.FileName` | 当前选中项的文件名；赋值则把选中项移到同名项（列表里没有该名字时不动） |
| `File1.List(j)` / `ListCount` / `ListIndex` | 与 `ListBox` 同一套读法 |
| 设计期 `.frm` 里的 `Path` / `Pattern` | 建窗时即按它们填表 |
| 事件 | `Click`、`DblClick` |

判据在 `tests\ctrlfiles\`（CF6 认设计期两值、CF7 认按模式过滤、CF8 / CF9 认改 `Pattern` 立刻重刷、CF10 / CF11 认 `ListIndex` 与 `FileName` 的回路、CF14 认与原生 `ListBox` 的读数口径一致）。
尚未落地：`MultiSelect` 与配套的 `Selected(j)` / `NewIndex` 那一族、`Archive` / `Hidden` / `System` / `Normal` / `ReadOnly` 这些属性开关（当前口径 = 只按 `Pattern` 过滤，不掺属性过滤）。
