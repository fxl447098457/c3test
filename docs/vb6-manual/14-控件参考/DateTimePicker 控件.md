# DateTimePicker 控件

# DateTimePicker控件

               

**DateTimePicker**控件使您可以提供格式化的日期字段，使得进行日期选择很容易。另外，用户还可以从类似于**MonthView**控件的下拉式日历界面中选择日期。

**语法**

**DTPicker**

**说明**

**DateTimePicker**控件，有两种操作模式：

- 下拉式日历模式（缺省）—允许用户显示一种能够用来选择日期的下拉式日历。  
    
- 时间格式模式—允许用户在日期显示中选择一个字段（例如：月、日、年等等），按下控件右边的上下箭头来设置它的值。

可以自定义控件的下拉式日历的外观。使用各种颜色属性，例如**CalendarBackColor**、**CalendarForeColor**、**CalendarTitleBackColor**、**CalendarTitleForeColor**和**CalendarTrailingForeColor**，允许创建属于您自己的颜色方案。

可以使用键盘或鼠标对控件进行浏览。下拉式日历有两个按钮使您能够滚动月份数据出入视图。

**注意**   **DateTimePicker**控件是ActiveX控件组的一部分，包含在 MSCOMCT2.OCX 文件中。要在应用程序中使用**DateTimePicker**控件，必须将 MSCOMCT2.OCX 文件加入到工程之中。发布您的应用程序时，要将 MSCOMCT2.OCX 文件装入到用户的Microsoft Windows System或System32目录下。有关如何将ActiveX控件添加到工程之中的更多信息，请参阅*Programmer's Guide*中的"Adding controls to a Project"。


## 本项目的实现口径（ai/029 C29-DT-a）

`DTPicker` 在原生的 **`SysDateTimePick32`**（comctl32 自带窗口类，`ICC_DATE_CLASSES` 已在运行时初始化里请求过）上实现，语法面与本页上文一致。本页上文那条"必须将 MSCOMCT2.OCX 加入工程、并把它拷到用户的 System32 目录"的发行注意 **在本项目里不适用**：该 OCX 只有 32 位，64 位进程里 `CoCreateInstance` 直接失败，所以本项目不引它、也不需要带它。

| 写法 | 读数与实现 |
| --- | --- |
| `DTP1.Format` | `0` = 短日期（原生默认）、`1` = 长日期、`2` = 时间、`3` = 自定义（`CustomFormat` 生效）。0/1/2 的真值就是窗口样式位，getter 直读 `GWL_STYLE` |
| `DTP1.CustomFormat` | 走原生 `DTM_SETFORMATW`。**原生没有对称的 Get**（commctrl.h 里只有 `DTM_SETFORMATW`），所以这份串自存一份；写它 = `Format` 读回 `3`，把 `Format` 切回 0/1/2 会把它撤掉 |
| `DTP1.CheckBox` | 原生样式位 `DTS_SHOWNONE`（勾掉 = 无日期） |
| `DTP1.UpDown` | 原生样式位 `DTS_UPDOWN`（不弹月历、改微调按钮） |
| `DTP1.CalendarBackColor` / `CalendarForeColor` / `CalendarTrailingForeColor` / `CalendarTitleBackColor` / `CalendarTitleForeColor` | 下拉月历的五个颜色槽，走 `DTM_SETMCCOLOR` / `DTM_GETMCCOLOR`（颜色值按 GDI 的 `COLORREF` 传，与 `ForeColor`/`BackColor` 同一口径） |
| `DTP1.IdealWidth` | **本项目的扩展读数，不是 VB6 属性**：原生 `DTM_GETIDEALSIZE` 问控件"刚好装得下当前格式"要多宽。它存在的意义见下第二条 |

两条要提前知道的边界（都是量出来的，不是设计选择）：

1. **`CheckBox` 与 `UpDown` 只能在设计期给**。这两位是复选框 / 微调按钮那两枚**子窗口**的创建参数，控件只在 `WM_CREATE` 读一次；运行期赋值改的是窗口样式位，控件会立刻把它们抹回去 —— 于是 `DTP1.CheckBox = True` 之后 `DTP1.CheckBox` 读回 `False`。想要这两种外观，请在设计期勾上（回归里的 `DT11` 那条针就是钉这个行为的：谁做出真运行期切换，它会翻红，届时请回来改本节）。
2. **`Format` 运行期切档是有效的**。判据不信"样式位写进去了"这种自家人读数 —— SDK 常数 `DTS_TIMEFORMAT = 0x0009` 本身就带着 `DTS_UPDOWN` 那一位，只对自己的掩码看永远说"我写对了"。所以每一条格式类判据都配一条上表 `IdealWidth` 的控件侧宽度读数。

## 本项目的实现口径（ai/029 C29-DT-b：`Value` / `MinDate` / `MaxDate`）

三条 Date 型属性走原生 `DTM_GETSYSTEMTIME` / `DTM_SETSYSTEMTIME` / `DTM_GETRANGE` / `DTM_SETRANGE`，换算用 oleaut32 的 `VariantTimeToSystemTime` / `SystemTimeToVariantTime`。本项目的 `Date` 在生成代码里就是**一个 double 序列号**（`Dim d As Date` 直接发成 `double d`），所以这三条不经过任何装箱：

| 写法 | 读数 |
| --- | --- |
| `DTP1.Value` | Date。刚建好 = 今天；赋值后按"年/月/日"读回的就是所赋的那天 |
| `DTP1.MinDate` / `MaxDate` | Date。**没设过时读回 0**（原生那一位有效标志没立） |

三条与 VB6 有差的行为，都是量出来的：

1. **越出范围的赋值被控件拒绝、值保持原样**，不是钳到边界。`MinDate = #2020-1-1#` 之后再写 `Value = #2009-6-12#`，读回来仍是原来那天。
2. **改一端不动另一端**：原生范围是一张 `(min, max)` 加两位有效标志，只发 `GDTR_MIN` 会把 max 清成未设，所以本项目读回整张表、只换要改那格、再连着标志一起发回去。
3. **"无日期"那一态（`CheckBox` 勾掉）在 VB6 是 `Value = Null`**，而本项目的 `Value` 是 double，装不了 Null。因此拆开两条读数：`Value` 在未勾时读回 `0`，另给一条本项目扩展读数 **`HasDate`**（写 `HasDate = False` 取消勾选、写 `True` 勾回）。**取消前那天会被记住**，勾回来还是它 —— 原生在 `GDT_NONE` 态下回填的是它自己的内部日期（本机实测 36494），不记账就会串值。

### C3 的实现面（续）：`Change` / `DropDown` / `CloseUp` 三个事件（C29-DT-c 已做）

三条都由父窗收 `WM_NOTIFY` 派发，码值来自公共控件头：`DTN_DATETIMECHANGE = -759`、
`DTN_DROPDOWN = -754`、`DTN_CLOSEUP = -753`。处理器写法与 VB6 一致，**三条都没有参数**：

```vb
Private Sub DTPicker1_Change()      ' 值变了（拨日期、勾/取消勾选框都算）
Private Sub DTPicker1_DropDown()    ' 下拉月历弹出
Private Sub DTPicker1_CloseUp()     ' 下拉月历收起
```

两条与 VB6 有差或有边界的地方，都是量出来的：

1. **程序化赋值不会触发 `Change`**。原生 `SysDateTimePick32` 的这条通知只由用户交互驱动，
   `DTP1.Value = Date` 走的是 `DTM_SETSYSTEMTIME`，控件不发通知（本机实测：一次都不叫）。
   VB6 那颗 OCX 里同样这句会 raise `Change` —— C3 这一格**照原生、不伪造**。若你的代码原来
   靠"赋值 → Change → 联动"这条链，请把联动的那句自己写出来。
2. `DropDown` / `CloseUp` 只在**真的弹出/收起**时到，弹出来的月历窗口在本项目里是控件自己的，
   不需要宿主处理；期间可以拿 `DTM_GETMONTHCAL` 问出那枚句柄（本语言面还没开放这一条）。

还欠的两条刻意不做，理由记在 `ai/029`：`CallbackKeyDown`（要 `DTN_FORMAT` / `DTN_USERSTRING` /
`DTN_WMKEYDOWN` 三条回调协议，且只在 `CustomFormat` 含回调字段时有意义）、`FutureDate`。

**DTPicker 这一族到此收口**：窗口、样式、标量属性、Date 值面、事件面都在了。

