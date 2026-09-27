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

还不在这格里（后续批）：`Value` / `MinDate` / `MaxDate` 这三条 Date 型属性，以及 `Change` / `DropDown` / `CloseUp` 三个事件。
