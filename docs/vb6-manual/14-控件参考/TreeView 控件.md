# TreeView 控件

# TreeView 控件

               

**TreeView** 控件显示 **Node** 对象的分层列表，每个 **Node** 对象均由一个标签和一个可选的位图组成。**TreeView** 一般用于显示文档标题、索引入口、磁盘上的文件和目录、或能被有效地分层显示的其它种类信息。

**语法**

**Treeview**

**说明**

创建了 **TreeView** 控件之后，可以通过设置属性与调用方法对各 **Node** 对象进行操作，这些操作包括添加、删除、对齐和其它操作。可以编程展开与折回 **Node** 对象来显示或隐藏所有子节点。Collapse、Expand 和 NodeClick 三个事件也提供编程功能。

**Node** 对象使用 **Root**、**Parent**、**Child**、**FirstSibling**、**Next**、**Previous** 和 **LastSibling** 属性。在代码中可通过检索对 **Node** 对象的引用，从而在树上定位。也可以使用键盘定位。UP ARROW 键和 DOWN ARROW 键向下循环穿过所有展开的 **Node** 对象。从左到右、从上到下地选择 **Node** 对象。若在树的底部，选择便跳回树的顶部，必要时滚动窗口。RIGHT ARROW 键和 LEFT ARROW 键也穿过所有展开的 **Node** 对象，但是如果选择了未展开的 **Node**之后再按 RIGHT ARROW 键，该 **Node** 便展开；第二次按该键，选择将移向下一个 **Node**。相反，若扩展的 **Node** 有焦点，这时再按 LEFT ARROW 键，该 **Node** 便折回。如果按下 ANSI 字符集 中的键，焦点将跳转至以那个字母开头的最近的 **Node**。后续的按该键的动作将使选择向下循环，穿过以那个字母开头的所有展开节点。

控件的外观有八种可用的替换样式，它们是文本、位图、直线和 +/- 号的组合，**Node** 对象可以任一种组合出现。

**TreeView** 控件使用由 **ImageList** 属性指定的 **ImageList** 控件，来存储显示于 **Node** 对象的位图和图标。任何时刻，**TreeView** 控件只能使用一个 **ImageList**。这意味着，当 **TreeView** 控件的 **Style** 属性被设置成显示图像的样式时，**TreeView** 控件中每一项的旁边都有一个同样大小的图像。

**发行注意 TreeView** 控件是 MSCOMCTL.OCX 文件中的一组 ActiveX 控件的一部分。为了在应用程序中使用 **TreeView** 控件，必须将 MSCOMCTL.OCX 文件添加到工程中。在发行应用程序时，要在用户的 Microsoft Windows System 或 System32 目录中安装 MSCOMCTL.OCX 文件。

## 本项目的实现口径（ai/029 C29-8a）

`TreeView` 在原生的 **`SysTreeView32`**（comctl32 自带窗口类）上实现，语法面与本页上文一致。本页上文那条"必须将 MSCOMCTL.OCX 添加到工程"的发行注意**在本项目里不适用**：该 OCX 只有 32 位，64 位工程加载不了，所以本项目不引它。

| 写法 | 读数 |
| --- | --- |
| `Tree1.LineStyle` | `0` = `tvwTreeLines`（画树线，根层不画）、`1` = `tvwRootLines`（根层也画）。设计期写在哪一版就是哪一版 |
| `Tree1.CheckBoxes` | `True` 时每个节点带复选框；读回 `True`/`False`（`-1`/`0`） |
| `Tree1.HotTracking` | `True` 时鼠标悬停的节点标题呈超链接样式 |
| `Tree1.HideSelection` | 默认 `True` = 控件失焦时不画选中项；`False` 对应 Win32 的"失焦仍显示选中" |
| `Tree1.Indentation` | 单位是**缇**（与 `Left`/`Width` 一致），写进去多少就读回多少；下发给控件时换算成像素，像素侧上限 50 px 会钳，但 VB 侧读数不被改写 |
| `Tree1.Visible` / `Enabled` / `Left` / `Top` / `Width` / `Height` / `Font*` / `ForeColor` / `BackColor` | 与其它可见控件同一套读法 |

上面这五条是**窗口样式位本身**，不是另存的一份副本：`CheckBoxes`、`HotTracking`、`LineStyle`、`HideSelection` 四条的 getter 直接读窗口的 `GWL_STYLE`，运行期赋值会连带重算布局并重绘，所以"读到的"与"画出来的"不会分叉。

尚未落地（本页下一节 `C29-8b` 已把 `Nodes` 那一族接上）：`Style` 八种组合、`SelectedItem`、`LabelEdit`、`Sorted`、`ImageList` 关联，以及 `NodeClick`/`Expand`/`Collapse` 等事件。设计期写在这些属性上的值目前**不会**报错，也**不会**见效。

判据在 `tests\ctrltreeview\`（TV1-TV5 认设计期五值、TV6-TV7 认默认值与"消息真打进窗口"、TV8-TV9 认运行期赋值可逆、TV10-TV11 认缇值往返、TV12 认通用属性面没被抢走）。

## 本项目的实现口径（ai/029 C29-8b：`Nodes` 集合与 `Node` 对象）

`TreeView1.Nodes` 是一个**真的 `IDispatch` 集合对象**（复用 `ImageList`/`ListView` 那一批立的成员对象机制，`src\rtl\core\vb6forms\vb6forms_memberobj.c`），不是先把节点抄进一张 VB 侧的表再算：

- **结构只有一个真相**：父子与兄弟一律现问原生树（`TVM_GETNEXTITEM`），展开态问 `TVIS_EXPANDED`，勾选态问节点的 state image 位（`TVM_GETITEMSTATE`）。这张表只存原生给不出来的四样：`Key`、`Text`、`Tag`、两个图索引。所以"读到的"与"屏幕上画的"不会分叉。
- **`Text` 写回去会同步到控件**（`TVM_SETITEMW` + 重绘）；只改表不改控件，正是本线一直防的那种假实现。

| 写法 | 读数 |
| --- | --- |
| `Tree1.Nodes.Count` / `Node.Index` / `For Each n In Tree1.Nodes` | **集合序 = 插入序**。三条问的都是同一个序（不是显示序：原生控件按关系插入，插入序与视觉序可以不同，这一条是本项目定的口径） |
| `Tree1.Nodes.Add([relative], [relationship], [key], [text], [image], [selectedimage])` | `relationship` 认 `0..4`（`tvwFirst`/`tvwLast`/`tvwBefore`/`tvwAfter`/`tvwChild`），缺省 = `tvwChild`；`relative` 给**下标或 Key 都收**。没有相对项时一律挂在根末尾。返回的就是 `Node` 对象 |
| `Tree1.Nodes(i)` / `Tree1.Nodes("key")` | 同一条 `Item`，实参是下标还是宽字符串都认（Key 不区分大小写）。取不到给 `Nothing` |
| `Node.Text` / `Key` / `Tag` | 可读可写；越界读给空串 |
| `Node.Checked` | 节点的 state image 位（`2` = 勾上）。**没开 `CheckBoxes` 也写得进、读得出**，只是不画方框 —— 与 VB6"先设 `CheckBoxes=True` 才看得到"的次序一致 |
| `Node.Expanded` | 读写都过原生（`TVM_EXPAND`）；`EnsureVisible` 会把父节点撑开，这条读数是控件自己做的 |
| `Node.Parent` / `Child` / `Children` / `Next` / `Previous` | 现问原生树；`Children` 是**子节点个数**，其余给 1 基下标，没有就给 `0` |
| `Node.Root` | 所属树的根祖先。**顶层节点返回自己**（VB6 同口径），越界返回 `0` |
| `Tree1.Nodes.Remove(i 或 "key")` / `.Clear` | `Remove` 删的是**整棵子树**（原生删父本就带走子，表里跟着一起销），`Clear` 清表 |

勾选态的写**只能走 `TVM_SETITEMW`**：`commctrl.h` 里根本没有 `TVM_SETITEMSTATE` 这条消息（只有 `TVM_GETITEMSTATE`）。照 getter 的样子"对称地"发明一条 SET，编得过、跑得通、勾就是不亮 —— 未定义的 `WM_USER+n` 会被默认窗口过程吞掉并返回 0，一点动静都没有。

还没做的成员：`Bold`、`Sorted`、`RelativeX`、`Node.Style`、`Node.Button` —— 名字表里**刻意不登记**这些，登记了就是"看着支持、实则答错"（与 `ListView` 那族同一取舍）。

判据在同一个工程里（`tests\ctrltreeview\`，TV13-TV27）：`Count`、`Add` 返回对象的 `Index`、下标与 Key 两条取法（TV13-TV15）、结构导航（TV16-TV18）、写回后换一枚对象再读（TV19）、勾选（TV20）、展开三态含 `EnsureVisible` 撑开父节点（TV21-TV23）、`For Each` 与集合序（TV24）、`Remove` 带走子树与 `Clear`（TV25-TV26）、以及"集合这条路没把标量属性面抢走"（TV27）。x86 与 x64 都跑。
