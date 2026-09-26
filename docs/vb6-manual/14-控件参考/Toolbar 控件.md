# Toolbar 控件

# Toolbar 控件

               

**Toolbar** 控件包含一个 **Button** 对象集合，该对象被用来创建与应用程序相关联的工具栏。

**语法**

**Toolbar**

**说明**

一般来说，工具栏包含一些按钮，这些按钮与应用程序菜单中各项的按钮对应，工具栏为用户访问应用程序的最常用功能和命令提供了图形接口。

有了 **Toolbar** 控件，就可以通过将 **Button** 对象添加到 **Buttons** 集合中来创建工具栏。每个 **Button** 对象都可有可选的文本或一幅图象，或者兼而有之，这些都是由相关联的 **ImageList** 控件提供的。可在一个按钮上用 **Image** 属性为每个 **Button** 对象加一幅图象，或用 **Caption** 属性显示文本，或者二者兼而有之。在设计时可用 Toolbar 控件的属性页将 **Button** 对象添加到控件中。在运行时可用 **Add** 和 **Remove** 方法添加按钮或从 **Buttons** 集合中删除按钮。

为了给 **Toolbar** 编程，将代码添加到 ButtonClick 事件中，以便对已选定的按钮作出反应。也可用 **Style** 属性确定每个 **Button** 对象的状态和外观。例如，如果已对四个按钮赋以 ButtonGroup 样式，则在任何时候只能按下一个按钮，而且至少总有一个按钮已被按下。

可对 **Button** 对象赋以 PlaceHolder 样式，然后将控件定位在定位符的上方，从而在工具栏上为其它控件创建空间。例如，在设计时，为将下拉组合框放置在工具栏上，用 PlaceHolder 样式添加一个 **Button** 对象，并调整其大小使之与 **ComboBox** 控件一样宽。然后将 **ComboBox** 控件放置在定位符上。

在运行时双击工具栏后就会调用“自定义工具栏”对话框，有了这个对话框，用户就可隐藏、显示或重新安排工具栏按钮。可用 **AllowCustomize** 属性来允许或禁用对话框。也可用 **Customize** 方法调用“自定义工具栏”对话框。如果希望保存或恢复工具栏的状态，或者允许用户这样做，则有两种方法可供使用：**SaveToolbar** 和 **RestoreToolbar** 方法。在改动工具栏时产生的 Change 事件一般被用来调用 **SaveToolbar** 方法。

**注意** 自定义的对话框也包含帮助按钮。当终端用户单击帮助按钮时，使用 **HelpFile** 和 **HelpContextID** 属性来决定显示哪个帮助文件。

通过对每个 **Button** 对象的 **ToolTipText** 描述进行编程可进一步增强可用性。为显示工具提示，必须将 **ShowTips** 属性设置为 **True**。当用户调用“自定义工具栏”对话框时，单击按钮就会导致在对话框中显示按钮的描述；这种描述可通过设置 **Description** 属性来编程实现。

**发行注意**   **Toolbar** 控件是一组ActiveX 控件的一部分，这组自定义控件可在文件 MSCOMCTL.OCX 中找到。为在应用程序中使用 **Toolbar** 控件，必须将文件 MSCOMCTL.OCX 添加到工程中。在发行应用程序时，应将文件 MSCOMCTL.OCX 安装到用户的 Microsoft Windows 的SYSTEM 或 System32 ( Windows NT 平台上)的文件夹。关于怎样将一个 ActiveX 控件添加到工程中去的详细信息，请参阅《部件工具指南*》*中的“加载 ActiveX 控件”。

## 本项目的实现口径（ai/029 C29-5a）

`Toolbar` 在原生的 **`ToolbarWindow32`**（comctl32 自带窗口类）上实现，语法面与本页上文一致。本页上文那条"必须将 MSCOMCTL.OCX 添加到工程"的发行注意**在本项目里不适用**：该 OCX 只有 32 位，64 位工程加载不了。

| 写法 | 读数 |
| --- | --- |
| `Toolbar1.ShowTips` | 默认 `True`；真值就是窗口的 `TBSTYLE_TOOLTIPS` 位 |
| `Toolbar1.TextStyle` | `0` = 文字在图标下方（默认）、`1` = 文字在右侧（`TBSTYLE_LIST`）|
| `Toolbar1.AllowCustomize` | 对应窗口的 `CCS_ADJUSTABLE` 位。该位原生只在**创建时**起作用，所以运行期改它读得回、真拖拽自定义还没生效 |
| `Toolbar1.Align` | 存的是 VB6 那一套 `0..4`，默认 `1`（靠上）。**停靠行为还没接**：控件按 `.frm` 的 `Left/Top/Width/Height` 摆放 |
| `Toolbar1.Buttons.Count` | 问的是控件自己（`TB_BUTTONCOUNT`），所以设计期那些按钮**没真进控件就数不到**；分隔符也计入 |
| `Toolbar1.Visible` / `Enabled` / `Left` / `Top` / `Width` / `Height` / `Font*` / `ForeColor` / `BackColor` | 与其它可见控件同一套读法 |

设计期属性页里加的按钮（`Key` / `Caption` / `Style` / `ToolTipText` / 分隔符宽度）会逐条建进原生控件；`Image` 索引也原样记下，但**图标要等 `ImageList` 关联那一格**才显出来 —— 没关联时按钮一律"无图"，与 VB6 里不给工具栏配图标时的观感一致。

尚未落地（`Buttons`/`Button` 由本页 `C29-5b` 那节接上、两条按钮事件由 `C29-5c` 那节接上）：`ImageList` 关联、真停靠（`Align`）、真自定义工具栏行为。设计期写在这些属性上的值目前**不会**报错，也**不会**见效。

另两条与本页上文不同的地方：改之前这枚控件在 C3 里**连窗口都没有**（读任意属性都会 `C2065: vb6_hwnd_tb1 未声明的标识符` 而编不过），以及 `Debug.Print "" & CStr(Toolbar1.Buttons.Count)` 这种 `CStr(...)` 包一层的写法目前拿不到数（`&` 直接拼是正常的）—— 与本批无关的既有面，记在 `ai/029` §九。

判据在 `tests\ctrltoolbar\`（TB1 认原生数到三条设计期按钮、TB2 认不被另一枚继承、TB3-TB4 认矩形按 `.frm`、TB5-TB7 认标量属性的设计期值与默认值、TB8-TB9 认运行期赋值可逆、TB10 认两枚不串、TB11-TB12 认通用属性面）。

## 本项目的实现口径（ai/029 C29-5b：`Buttons` 集合与 `Button` 对象）

`Toolbar1.Buttons` 是一枚**真的 `IDispatch` 集合对象**（与 TreeView 的 `Nodes`、ListView 的 `ListItems`、StatusBar 的 `Panels` 同一族机制，`src\rtl\core\vb6forms\vb6forms_memberobj.c`）。5a 那只只把 `Buttons.Count` 特例改道的钩子就此退休 —— 现在 `Count` 由集合自己答，而它问的还是原生 `TB_BUTTONCOUNT`。

读数按"**原生答不答得了**"分家，这条分界线就是本项目的口径：

| `Button` 的写法 | 落在哪里 | 说明 |
| --- | --- | --- |
| `Caption` | **现问控件**（`TB_GETBUTTONINFOW` / `TB_SETBUTTONINFOW`，`TBIF_TEXT`） | 文本存在控件自己的字符串表里（设计期那些 caption 也走 `TB_ADDSTRINGW`），所以"读到的是屏幕上的字"不是第二份抄本。VB6 里这条就叫 `Caption`，没有 `Text` |
| `Image` | 现问控件（`TBIF_IMAGE`） | `-1` = 无图（原生 `I_IMAGENONE`）。图标要显形还欠 `ImageList` 关联那一格 |
| `Enabled` / `Visible` / `Value` | 现问控件（`TBIF_STATE` 的 `TBSTATE_ENABLED` / `TBSTATE_HIDDEN` / `TBSTATE_CHECKED`） | `TBBUTTONINFO` 没有 stateMask 这一栏，写要先读后改 |
| `Style` | 5a 那张表 | 原生 `fsStyle` 分不出**分隔符**（3）与**占位符**（4）—— 两者都发成 `BTNS_SEP`，只在宽度上不同。问原生就丢一档，所以 VB 侧那套 `0..5` 以表为准，写下去时同步换原生位 |
| `Key` / `Tag` / `ToolTipText` / `Width` | 5a 那张表 | `Key` 不区分大小写地支持 `Buttons("key")` 与 `Buttons.Remove "key"`；`ToolTipText` 的原生面要在 `TTN_GETDISPINFO` 里回文本，那一格还没做；`Width` 是 **VB 侧请求值**（原生 `cx` 只对分隔符起作用，普通按钮的宽度由图标与文字量出来） |

| 集合写法 | 读数 |
| --- | --- |
| `Toolbar1.Buttons.Count` | 原生 `TB_BUTTONCOUNT` |
| `Toolbar1.Buttons(i)` / `Toolbar1.Buttons("key")` | 同一条 `Item`，下标与 Key 都收；取不到给 `Nothing` |
| `Toolbar1.Buttons.Add([Index], [Key], [Caption], [Style], [Image])` | 返回 `Button` 对象；`Index` 是"插到第几格前面"，省略 = 追加。省略的实参发成 `vb6_ComPackMissing()` 而不是 0（0 会被当成"插到第 1 格前面"） |
| `Toolbar1.Buttons.Remove(i 或 "key")` / `.Clear` | 控件与表**一起**退格；`Clear` 从后往前逐条 `TB_DELETEBUTTON`（正着删会跳格） |
| `For Each b In Toolbar1.Buttons` | 走 `_NewEnum`，顺序 = 集合序 = 槽位序 |

一处 v6 主题下的坑（写进了 RTL 注释）：**`TB_ADDBUTTONSW` / `TB_INSERTBUTTONW` 不吃调用方给的 `fsState`** —— 实测建完之后 `TB_GETBUTTONINFOW(TBIF_STATE)` 读回 `0`，连 `TBSTATE_ENABLED` 都没有，于是 `Button.Enabled` 的默认读数会是 `False`，与 VB6 相反。补法：建完再发一条 `TB_SETBUTTONINFOW` 把启用位打上去（`TB_SETBUTTONINFO` 那一路是认的）。同族前例：5a 记的"没先收 `TB_BUTTONSTRUCTSIZE` 就静默吞按钮"，以及 TreeView 8b 记的"`commctrl.h` 里根本没有 `TVM_SETITEMSTATE`"。

还没做：`ButtonClick` / `ButtonMenuClick` 事件（要接 `TBN_*` 的 `WM_NOTIFY` 派发，与 TreeView 的 `NodeClick` 同一格欠账）、`ImageList` 关联、`Mask`、`MenuItem`、`StateImage`，以及真自定义工具栏行为（`AllowCustomize` 那位原生只在创建时起作用）。`Button.Description` 之类的成员**刻意不进名字表** —— 进去就是"看着支持、实则答错"。

判据在 `tests\ctrltoolbar\`（TB13-TB27 十五条）：`Count`（TB13）、下标与 Key 两条取法（TB14）、**Caption 的控件侧往返**（TB15 读、TB20 写后再换一枚对象读）、ToolTipText/Style/Width 三条表侧读数（TB16-TB18）、默认启用与可见（TB19）、`Enabled`/`Visible` 反向可逆（TB21-TB22）、改 `Style` 为复选后 `Value` 勾得上（TB23）、运行期 `Add` 返回对象与序号（TB24）、`For Each`（TB25）、`Remove` 按 Key 后序号前移（TB26）、`Clear` 与"集合这条路没抢走标量属性"（TB27）。x86 与 x64 都跑。

## 本项目的实现口径（ai/029 C29-5c：`ButtonClick` / `ButtonMenuClick`）

两条事件**不在同一条通道上**（原生的规矩，VB6 的文档把两条都记在工具栏身上）：

| 写法 | 原生通道 | 派发条件 |
| --- | --- | --- |
| `Toolbar1_ButtonClick(ByVal Button As Button)` | `WM_COMMAND` | `HIWORD(wParam)=0` 且 `lParam` = 这枚工具栏的 HWND；`LOWORD(wParam)` 就是被按那颗按钮的 `idCommand`，直接当 1 基序号造 `Button` 对象 |
| `Toolbar1_ButtonMenuClick(ByVal Button As Button)` | `WM_NOTIFY` 的 `TBN_DROPDOWN`（`-710`） | `hdr.hwndFrom` = 这枚工具栏；`hdr.idFrom` 同上。**只有 `Style = 5`（带下拉箭头）那颗按钮发得出来** |

三条口径：

- 处理器收的是**一枚 `Button` 对象**（与 5b 那一族同源，`.Key` / `.Caption` 都读得到）。本页上文那个 VB6 签名里的第二个形参 `Cancel` **没接** —— `ButtonMenuClick` 的 `Cancel` 在 VB6 里用来阻止弹菜单，而本项目压根没有菜单可弹（`MenuItem` / `Mask` 那两格还没做）。
- `idCommand` 是 5a 建按钮时写进去的"槽号 + 1"，派发按同一个数回查表 ⇒ 控件、消息、表三边同源，任何一边错位都会当场读数翻红，不存在"收到通知就无脑回调"。
- 工具栏在**同一窗体上有几枚都不串**：认来源靠的是 HWND，不是序号。

判据怎么触发（这条会反复用到，写死在这里）：事件必须在 **`VB.Timer`** 里发 —— `Form_Load` 阶段派发被 `block events during form init` 拦掉，而 `Form_Activate` 在无头会话里永远不来。无头环境点不了鼠标，RTL 因此交两条**判据专用**方法 `Toolbar1.SimButtonClick i` / `SimButtonMenuClick i`：它们不直接调处理器，而是先向控件问那颗按钮的 `idCommand` 与 `fsStyle`（`TB_GETBUTTON`），再照真控件的样子把消息发给父窗 ⇒ 问不到（序号越界）或原生没有下拉位（普通按钮）就**不发**，"派发分支 + 序号换算 + `ButtonAt` 造对象"三段才算都被验到。

判据在 `tests\ctrltoolbar\`（TB28-TB34 七条）：`SimButtonClick` 连点两颗 ⇒ 计数增量 2（TB28）、handler 里读到的 `Button.Key` 分别是那两颗（TB29，证的是对象指对了格）、另一枚工具栏的 handler 一次没进（TB30，认来源）、越界序号不进（TB31）、下拉按钮的 `SimButtonMenuClick` 进且 Key 对上（TB32）、普通按钮的下拉模拟**不发**（TB33）、事件面没抢走 5b 的集合读数与标量属性（TB34）。x86 与 x64 都跑。
