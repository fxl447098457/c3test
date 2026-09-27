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
另外 `LoadLibraryW(L"Msftedit.dll")` 一次。它没有备胎：实测 `riched20.dll`（老通道）只注册
`RichEdit20W`，问它要 `RICHEDIT50W` 回 `err=1411`（类不存在），而发码里的类名是写死的字面量
⇒ 换 dll 并不能把窗口建出来。真拿不到时 RTL 在 stderr 喊一条 `[C3_FORMS] ...`，控件窗口
不存在、判据整批红（刻意的：宁可响亮地失败，也不要静默空转）。

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

### C3 的实现面（续）：`Sel*` 的格式面（C29-RT-b 已做）

| 写法 | C3 里实际发生的事 |
| --- | --- |
| `RT.SelBold` / `SelItalic` / `SelUnderline` / `SelStrikethru` | `EM_SET/GETCHARFORMAT` + `CHARFORMAT2W` 的 `CFM_/CFE_BOLD\|ITALIC\|UNDERLINE\|STRIKEOUT`（`SCF_SELECTION`，只作用在选中那段上） |
| `RT.SelColor` | `CFM_COLOR`；**没涂色时读回的是控件自己的前景色**（原生那一位是"自动色"，与 VB6 观感一致），涂过才回显式值 |
| `RT.SelFontName` | `CFM_FACE` + `szFaceName`（回 `String`） |
| `RT.SelFontSize` | `CFM_SIZE` + `yHeight`，**原生单位是 1/20 磅**，C3 折成磅（设 14 读回 14） |
| `RT.SelAlignment` | `EM_SET/GETPARAFORMAT` 的 `PFM_ALIGNMENT`；**VB6 的 0左/1中/2右 对原生 1/3/2，两套数不一样** |
| `RT.SelIndent` / `SelRightIndent` / `SelHangingIndent` | `PFM_STARTINDENT` / `PFM_RIGHTINDENT` / `PFM_OFFSET`，单位 = twips；悬挂在原生里是 `dxOffset` **取负** |

三条要注意的口径：

1. **混合状态读回来是"没有"**。选区里一半加粗一半不加粗时，原生会把 `dwMask` 里那一位清掉
   （问得出"混合"），但本语言的 `Integer/Boolean` 装不了 VB6 的 `Null` ⇒ C3 一律按 `False` 给。
   这与 VB6 文档教的写法等价（`If RT.SelBold = True` 在 `Null` 下本来就是假），
   但 `If Not RT.SelBold Then` 在 VB6 里会报 94、在 C3 里会进分支 —— 从 VB6 搬代码时留意。
2. **别用原生那条"按偏移量缩进"的消息做悬挂缩进**：`PFM_OFFSETINDENT` 实测是把整段往右推
   （起始缩进 720 变 960），不是首行外凸。C3 用 `PFM_OFFSET` 取负值实现，读写都是"悬挂的磅数"。
3. **段落的格式属性不跨段串台**：`SelAlignment` / 三个缩进作用在选区**所在的段**上，
   选区跨段时读回来是"混合"⇒ 按 `0` 给。想让整段都改，先把插入点放进那一段（或整段选中）。

新控件的默认字体不是 12 磅，而是**继承窗体的字体**（本机是 8.25 磅 MS Sans Serif），
所以"没设过字号时 `SelFontSize` 是多少"这种针不能写死数 —— 要问就与同一枚控件的 `FontSize` 比。

### C3 的实现面（续）：`TextRTF` / `LoadFile` / `SaveFile` / `Find`（C29-RT-c 已做）

| 写法 | C3 里实际发生的事 |
| --- | --- |
| `RT.TextRTF` | 读 `EM_STREAMOUT(SF_RTF)`；写 `EM_STREAMIN(SF_RTF)`，写之前先全选 ⇒ 语义是**换掉内容**（不是往选区里追加） |
| `RT.SaveFile path[, type]` | `type` 0 = rtfRTF（缺省）、1 = rtfText；整串用 `CreateFileW` 落盘 |
| `RT.LoadFile path[, type]` | 反向；`type` 缺省 0 |
| `RT.Find(s[, start][, end][, flags])` | `EM_FINDTEXTEXW`；命中回**起点**（与 `SelStart` 同一把尺），问不出回 `-1`。`flags` 用 VB6 那两位（1 整词 / 2 区分大小写），原生 `FR_WHOLEWORD=2`、`FR_MATCHCASE=4` 由 RTL 逐位折算 |

五条本机量出来的口径（探针 `.buildtprobe5.c` / `rtprobe6.c` / `rtprobe7.c`，x64 与 x86 逐字相同）：

1. **`TextRTF` 串的头里带本机 ANSI 码页**（实测 `ansicpg936`、`deflangfe2052`）⇒ 别拿这串当身份比字节。
   判据一律问"前缀 `{tf1` + 正文在里面 + 同一枚控件连问两次自比"。
2. **`Find` 缺省 `start` = `-1` 是"从当前选区起点起找"** ⇒ 连问同一句两次结果会不同（游标被前一次挪走）。
   要固定起点就写 `Find(s, 0)`。
3. **命中只看起点在不在范围里**：`Find("alpha", 0, 10)` 与 `(0, 11)` 都回 5 —— 尾巴跨过右端不算越界。
4. `LoadFile` 找不到文件时**内容保持原样**（原生拒绝，不钳位、不清空）。VB6 那两条 Sub 靠运行期错误报告
   失败，本项目还没有那条通道 ⇒ 现在是"静默保持原样"，读不回错误码。
5. VB6 的 `LoadFile` 还能收一个**已打开的文件号**（`FreeFile` 那一族）。C3 这一格只做路径字符串那一形，
   文件号那一形刻意不做（边界记在 ai/029）。

### 还没做的一格

`Change` / `SelChange` 两条事件（RT-d —— 它们是"工具栏按钮状态跟着选区刷新"的关键，
两条走的还是不同的通知通道：`EN_CHANGE` 走 `WM_COMMAND`、`EN_SELCHANGE` 走 `WM_NOTIFY`）。
`SelPrint` 与 OLE 对象嵌入 / 拖放不在计划内。
