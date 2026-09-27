# MonthView 控件

# MonthView 控件

               

**MonthView** 控件可以用来创建一个能够让用户通过日历风格的界面查看和设置日期信息的应用程序。

**语法**

**MonthView**

**说明**

**MonthView** 控件的 **Value** 属性返回当前被选定的日期。

可以允许最终用户通过将 **MultiSelect** 属性设置为 **True**，并使用 **MaxSelProperty** 指定可选择的天数来选择一个连续的日期范围。**SelStart** 和 **SelEnd** 属性返回所选择的日期范围的第一个日期和最后一个日期。

可以用许多方法自定义一个 **MonthView** 控件的外观。可以使用各种颜色属性，例如 **MonthBackColor**、**TitleBackColor**、**TitleForeColor** 和 **TrailingForeColor** 为控件创建一个唯一的配色方案。

通过设置 **MonthRows** 和 **MonthColumns** 属性，可以在一个 **MonthView** 控件中一次显示多个月份（多至 12）。**MonthRows** 和 **MonthColumns** 属性的总数必须小于或等于 12。

**注意**    **MonthView** 控件是 ActiveX 控件组的一部分，位于 Mscomct2.ocx 文件中。如果要在应用程序中使用 **MonthView** 控件，必须将 Mscomct2.ocx 文件添加到工程中。在发布该应用程序时，需要在用户的 Microsoft Windows 的 System 或 System32 目录中安装这个 Mscomct2.ocx 文件。有关如何将 ActiveX 控件添加到工程的详细信息，请参阅《程序员指南》中的“添加控件到工程”。


---

## C3 的实现面（C29-MV-a 已做：窗口 + 样式 + 标量属性面）

C3 里 **MonthView 不走 `Mscomct2.ocx`**（本页末尾那条"必须把 ocx 加进工程、发布时装到
System32"的要求在 C3 不存在）：那颗 OCX 是 32 位 inproc 服务器，64 位进程里
`CoCreateInstance` 直接失败。C3 把它复刻成 Win32 公共控件里的 `SysMonthCal32`，
**语法与属性名照 VB6**，不链任何 OCX。

| 写法 | C3 里实际发生的事 |
| --- | --- |
| `MView1.MultiSelect` | 样式位 `MCS_MULTISELECT`（0x2） |
| `MView1.ShowWeekNumbers` | 样式位 `MCS_WEEKNUMBERS`（0x4） |
| `MView1.ShowToday` | 样式位 `MCS_NOTODAY`（0x10），**与原生那位相反**：`ShowToday = False` 才是挂上它 |
| `MView1.MaxSelCount` | `MCM_SETMAXSELCOUNT` / `MCM_GETMAXSELCOUNT`（真往返过控件） |
| `MView1.MonthRows` / `MonthColumns` | 见下面第 1 条：原生按"窗口多大"决定画几个月 |
| `MonthBackColor` / `TitleBackColor` / `TitleForeColor` / `TrailingForeColor` / `BackColor` / `ForeColor` | `MCM_SETCOLOR` / `MCM_GETCOLOR` 的六格配色（`MCSC_*` 0..5） |

三条量出来的口径，写代码时要按它们想：

1. **多月平铺不是开关，而是"窗口多大"**。VB6 文档说 `MonthRows` 与 `MonthColumns` 之积最多 12，
   而原生压根没有"给几行几列"这条消息 —— 控件按自己的客户区尺寸决定画几个月。所以
   `MonthRows` / `MonthColumns` 的设计期值由 RTL 折算成窗口尺寸（先按行列乘出矩形，再用
   `MCM_SIZERECTTOMIN` 撑到真装得下，最后挪一次窗），要问"眼下真画了几个月"就读
   **`MView1.MonthCount`**（`MCM_GETCALENDARCOUNT`，控件自己的答案）。C3 不复查"≤ 12"这条上限。
2. **`MaxSelCount` 只在挂了 `MultiSelect` 的控件上写得住**。没那一位时 `MCM_SETMAXSELCOUNT`
   直接被拒（本机实测：默认 1，写 9 之后读回还是 1）。这条顺手成了"样式位真不真"的控件侧
   证人 —— 问的是控件有没有按那位办事，不是"我们的掩码读数说写进去了"。
3. **这三条样式位运行期改是有效的**（与 `DTPicker` 的 `CheckBox` / `UpDown` 相反，那两位会被
   控件抹回去）。判据用的是 `MCM_GETMINREQRECT`：`ShowToday = True` 之后控件自己报的最小高度
   就跟着涨。两枚控件的边界各量各的，不许互相外推。

两条本项目扩展读数（不是 VB6 属性，别当 VB6 代码往回搬）：**`MonthCount`**（眼下画了几个月）、
**`MinReqWidth` / `MinReqHeight`**（`MCM_GETMINREQRECT` 那个"装得下一个月"的最小尺寸）。

### C3 的实现面（续）：`Value` / `SelStart` / `SelEnd`（C29-MV-b 已做）

三条都是 Date（本语言里 `Date` 就是一个 double 序列号），换算走 oleaut32 那对现成函数。
两条原生行为会咬人，写代码前先看：

1. **`Value` 与 `SelStart`/`SelEnd` 两张表互斥**：`MultiSelect = True` 之后 `Value` 问不出
   也写不进（原生那两条 `MCM_GET/SETCURSEL` 直接失败，读数回 `0`）；不开多选时反过来 ——
   范围表问不出（`SelStart`/`SelEnd` 回 `0`）。所以"要不要范围选择"决定了你能读哪一组，
   这条与 VB6 那颗 OCX 的模型一致。
2. **范围只能落在当前显示的那一个自然月里**。跨到没显示的月份会被控件夹掉（实测会把
   止端夹回与起端同一天）；`MCM_SETMAXSELCOUNT` 的上限也真的管得住 —— 窗口贴着上限时，
   "往里扩"的那一端被原样顶回去，不会把已经选好的那头挪走。

止端两头都是**闭区间**：写 `SelEnd = #2026-9-6#` 之后读回来还是它（控件内部把范围存成
半开区间，RTL 折回了一天）。

### C3 的实现面（续）：`DateClick`（C29-MV-c 已做 ⇒ 这一族到此收口）

```vb
Private Sub MonthView1_DateClick(ByVal DateSelected As Date)
    DTPicker1.Value = DateSelected      ' 联动
End Sub
```

原生对应一条 `MCN_SELCHANGE`（-749），负载里带所选那天，派发时折算成 Date 传进来。
两条要注意：① **程序化改选不会触发它**（写 `Value` / `SelStart` / `SelEnd` 都不叫），
原生那条通知只由用户交互驱动 —— 与 `DateTimePicker` 的 `Change` 同一条口径；
② `DateDblClick` 原生**没有**对应通知（公共控件头里没有双击那条 `MCN_`），刻意不做。
