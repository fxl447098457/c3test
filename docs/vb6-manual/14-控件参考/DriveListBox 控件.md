# DriveListBox 控件

# DriveListBox 控件

               

在运行时，由于有 **DriveListBox** 控件，所以可选择一个有效的磁盘驱动器。该控件用来显示用户系统中所有有效磁盘驱动器的列表。可以创建对话框，通过它从任一可用驱动器的磁盘文件列表中打开文件。

**语法**

**DriveListBox**

**说明**

设置 **List**、**ListCount**、和 **ListIndex** 属性，就可以访问列表中的项目。如果需要显示 **DirListBox** 和 **FileListBox** 控件，那么可以编写代码，使它们与 **DriveListBox** 控件同步，并使它们之间彼此同步。
## 本项目的实现口径（ai/029 C29-1b）

`DriveListBox` 在原生的 **`COMBOBOX`**（`CBS_DROPDOWNLIST`，只能选、不能改）上实现，语法面与本页上文一致。

| 写法 | 读数 |
| --- | --- |
| `Drive1.Drive` | 盘符字母加冒号，形如 `C:`（**不带**尾反斜杠）；下拉列表里的每一项仍是 `C:\` |
| `Drive1.Drive = "D:"` | 选中对应的盘；机器上没有该盘则选中项不动 |
| `Drive1.List(j)` / `ListCount` / `ListIndex` | 与 `ListBox` 同一套读法，`List(j)` 返回 `C:\` 这种带反斜杠的原样文本 |
| 事件 | `Change`（选中项变了）、`Click` |

本页上文那句"编写代码，使它们与 `DirListBox` 控件同步"在本项目里同样是**手写**的：赋 `Drive` 不会自己去刷 `DirListBox`，反过来也一样（写法见 `DirListBox` 页）。

判据在 `tests\ctrlfiles\`（CF2 认列表项、CF3 认 `Drive` 出口、CF13 认盘 → 目录那一跳）。
尚未落地：`Font` / `ForeColor` 等外观属性对下拉列表项的作用。
