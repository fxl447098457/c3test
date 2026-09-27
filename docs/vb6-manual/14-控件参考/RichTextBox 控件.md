# RichTextBox 控件

# RichTextBox 控件

               

**RichTextBox** 控件不仅允许输入和编辑文本，同时还提供了标准 **TextBox** 控件未具有的、更高级的指定格式的许多功能。

**语法**

**RichTextBox**

**说明**

**RichTextBox** 提供了一些属性，对于本控件文本的任何部分，用这些属性都可以指定格式。为了改变文本的格式，首先要选定它。只有选定的文本才能赋予字符和段落格式。使用这些属性，可把文本改为粗体或斜体，或改变其颜色，以及创建上标和下标。通过设置左右缩进和悬挂式缩进，可调整段落的格式。

**RichTextBox** 控件能以 rtf 格式和普通 ASCII 文本格式这两种形式打开和保存文件。可以使用控件的方法（**LoadFile** 和 **SaveFile**）直接读写文件，或使用与 Visual Basic 文件输入/输出语句联结的、诸如 **SelRTF** 和 **TextRTF** 之类的控件属性打开和保存文件。

通过使用 **OLEObjects** 集合，**RichTextBox** 控件支持对象的嵌入。插入到控件中的每个对象，都代表 **OLEObject** 对象。用这样的控件，就可以创建包含其它文档或对象的文档。例如，可创建这样的文档，它有一个嵌入的 Microsoft Excel 电子数据表格、或 Microsoft Word 文档、或其它已在系统中注册的 OLE 对象。为了把一个对象插入到 **RichTextBox** 控件中，只需简单地拖动一个文件（例如 在Windows 95“资源管理器”中的拖动），或拖动的是另一应用程序（如 Microsoft Word）所用文件的一个突出显示的区域，然后将所拖内容直接放入控件。

**RichTextBox** 控件支持 OLE 对象的剪贴板和 OLE 拖/放操作。从剪贴板中粘贴进一个对象时，它被插在当前插入点处。一个对象被拖放到控件时，插入点将跟踪着鼠标光标的移动，直至鼠标按钮释放时该对象即被插入。这种行为和 Microsoft Word 的一样。

使用 **SelPrint** 方法，可以打印 **RichTextBox** 控件的全部或部分文本。

因为 **RichTextBox** 是一个数据绑定控件，通过 **Data** 控件可以把它绑定到 Microsoft Access 数据库的 Binary 或 Memo 字段上，也可把它绑定到具有相同容量的其它数据库字段上（例如 SQL 服务器中的 TEXT 数据类型的字段）。

标准 **TextBox** 控件用到的所有属性、事件和方法，**RichTextBox** 控件几乎都能支持，例如 **MaxLength、** **MultiLine**、 **ScrollBars、** **SelLength、** **SelStart** 和 **SelText**。对于那些可以使用 **TextBox** 控件的应用程序，也可以很容易地使用 **RichTextBox** 控件。而且，**RichTextBox** 控件并没有和标准 **TextBox** 控件一样具有 64K 字符容量的限制。

**发行注意** 为了能在应用程序中使用 **RichTextBox** 控件，必须把Richtx32.ocx 文件添加到工程中。因此，在应用程序发行时，Richtx32.ocx 文件就应安装在 Microsoft Windows 的 SYSTEM 目录内。有关怎样把自定义控件添加到工程中的详细内容，请参阅《程序员指南》。

---

## C3 的实现面（C29-RT-a 已做：窗口 + 创建样式 + 文本/选区/只读/上限/滚动条/换行）

C3 里 **RichTextBox 不走 `Richtx32.ocx`**（上面那条"必须把 OCX 加进工程、发布时装到 SYSTEM
目录"的要求在 C3 不存在）：那颗 OCX 是 32 位 inproc 服务器，64 位进程里 `CoCreateInstance`
直接失败。C3 把它复刻成 **Msftedit.dll 注册的 `RICHEDIT50W`**，语法与属性名照 VB6，不链 OCX。
一个通道差别：这枚类**不在 comctl32 的 `ICC_*` 体系里**，所以 RTL 在初始化公共控件之后
另外 `LoadLibraryW(L"Msftedit.dll")` 一次（拿不到就退回 `riched20.dll`）。

| 写法 | C3 里实际发生的事 |
| --- | --- |
| `RT.Text` | 通用那条 `WM_GETTEXT` / `WM_SETTEXT`（与 TextBox 同一对函数） |
| `RT.SelStart` / `SelLength` | `EM_EXGETSEL` / `EM_EXSETSEL` 那张 `(起,止)` 表；越界由控件夹到文末 |
| `RT.SelText` | 读 `EM_GETSELTEXT`（空选区读回空串），写 `EM_REPLACESEL`（有选区=替换，空选区=插入） |
| `RT.ReadOnly` | `EM_SETREADONLY`（**运行期有效**，设计期那条走 Init） |
| `RT.MaxLength` | `EM_SET/GETLIMITTEXT`；`0` = 不限（映射到原生的默认上限 32767） |
| `RT.ScrollBars` | 创建参数 `WS_HSCROLL` / `WS_VSCROLL` + `ES_DISABLENOSCROLL`；枚举照 VB6：**0 无 / 1 水平 / 2 垂直 / 3 两者** |
| `RT.WordWrap` | `EM_SETTARGETDEVICE`（见下面第 3 条） |
| `RT.BorderStyle` | 与 TextBox 同一条 `WS_EX_CLIENTEDGE` 路 |

四条量出来的口径，写代码时要按它们想：

1. **`ScrollBars` 只能创建时给，所以它没有写口**。原生这枚控件把滚动条当自己的东西管：
   内容不需要滚动时它连样式位一起把 bar 拆掉（实测：空文本下 `WS_VSCROLL` 读回 0，灌 60 行
   又自己回来），所以 C3 在创建时一并挂 `ES_DISABLENOSCROLL` 让样式位稳定。而**事后**
   `SetWindowLong` 加那两位只有外观（客户区让位、看着像有），滚动量程停在默认的 0..100
   —— 滚不动。因此 `RT.ScrollBars = 2` 这种**运行期赋值**在本实现里不发任何东西，读数
   恒等于设计期那一位。要问"滚动条到底活没活"，读本项目扩展的 **`VScrollRange` /
   `HScrollRange`**（`GetScrollInfo` 的 nMax）：真挂着的那枚随内容长过 100，没挂的那枚不动。
2. **`MaxLength` 只管用户键盘输入，管不住 `Text =` 赋值**。限 20 再赋 30 个字符，控件把上限
   抬到了文本长度（读数从 20 变 30），程序化 `SelText =` 同样穿过去。VB6 那颗 OCX 在赋值时
   会截断，这里不会 —— 别指望它当输入长度的护栏。
3. **`WordWrap` 的原生方向与常见片段相反**：`EM_SETTARGETDEVICE` 的 lParam 才是目标 DC，
   传 `NULL` = 折到**本窗客户区**宽（= VB6 的 `True`），传 `GetDC(控件)` = 目标宽度变成整屏
   （实际不折行）。原生从没调过这条时是"不折"，与 VB6 默认相反 ⇒ C3 在设计期没写
   `WordWrap` 时也显式下发一次 `True`。`EM_SET/GETWRAPMODE` 在这枚上问不出也设不动，
   所以这一位是**自存**的（与 `DTPicker.CustomFormat` 同一条路）。
4. **`SelStart` 按 UTF-16 代码单元计数**（与 EDIT 控件同口径）：代理对（emoji 那一类）算 2，
   不要当 VB6 的"字符数"来读。

两条同族既有缺陷顺手一并修了：`BorderStyle` 的读写此前只认 `"Edit"` 类名，`RICHEDIT50W`
落到"存窗口属性"那条兜底分支，而 `SetPropW(0)` 等于删属性 ⇒ `BorderStyle = None` 永远设不上、
而且恒读回默认 1。

### 还没做的三格

`SelBold` / `SelColor` / `SelFontName` / `SelAlignment` / `SelIndent` 那一批**格式**面，
`TextRTF` + `LoadFile` / `SaveFile` + `Find`，以及 `Change` / `SelChange` 两条事件（后者是
工具栏按钮状态跟着选区刷新的关键）—— 本版本尚未实现，读写会落到未登记属性的兜底路。
OLE 对象嵌入与 `SelPrint` 同样不在计划内。
