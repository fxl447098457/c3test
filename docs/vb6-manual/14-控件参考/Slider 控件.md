# Slider 控件

# Slider 控件

               

**Slider** 控件是包含滑块和可选择性刻度标记的窗口。可以通过拖动滑块，用鼠标单击滑块的任意一侧或者使用键盘移动滑块。

**语法**

**Slider**

**说明**

在选择离散数值或某个范围内的一组连续数值时，**Slider** 控件十分有用。例如，无需键入数字，通过将滑块移动到刻度标记处，可以用 **Slider** 对被显示的图象设置大小。要选择某个范围内的数值，需将 **SelectRange** 属性设置为 **True** 并对控件编程，这样当按下 SHIFT 键时就可选择范围。

可以水平或者垂直地放置 **Slider** 控件。

**发行注意** 为了在应用程序中使用 **Slider** 控件，必须将 MSCOMCTL.OCX 文件添加到工程中。在分布应用程序时，应把 MSCOMCTL.OCX 文件安装到用户的 Microsoft Windows System 或 System32 目录中。关于如何把 ActiveX 控件添加到工程的详细信息，请参阅《程序员指南》。

## 本项目的实现口径（ai/029 C29-SL-a：窗口 + 创建样式 + 标量属性面）

`Slider` 在原生的 **`msctls_trackbar32`**（comctl32 自带窗口类，`ICC_BAR_CLASSES` 已在运行时初始化里请求过）上实现，语法面与本页上文一致。本页上文那条"必须把 MSCOMCTL.OCX 加入工程并拷到 System32"的发行注意 **在本项目里不适用**：该 OCX 只有 32 位，64 位进程里 `CoCreateInstance` 直接失败，所以本项目不引它、也不需要带它。

| 写法 | 读数与实现 |
| --- | --- |
| `Slider1.Orientation` | `0` = 水平（默认）、`1` = 垂直。就是原生样式位 `TBS_VERT`；**运行期改是有效的**（写样式位 + 一次 `SWP_FRAMECHANGED` 换帧，实测滑块形状与位移轴跟着换） |
| `Slider1.TickFrequency` | 下发原生 `TBM_SETTICFREQ`（先 `TBM_CLEARTICS` 再设，免得新旧刻度叠画）。**原生没有回读这条消息**，所以读回来的是本项目自存的那一档 |
| `Slider1.TravelIsVert` | **本项目的扩展读数，不是 VB6 属性**：问控件自己"这杆横着走还是竖着走"（读 `TBM_GETTHUMBRECT` 的滑块长短边）。判据用它把"样式位写进去了"升级成"控件真按那一档在走" |
| `Slider1.TickPresent` | **本项目的扩展读数，不是 VB6 属性**：`TBM_GETTICPOS(0)` 问"当前到底画没画刻度"。实测光挂 `TBS_AUTOTICKS` 而不给 `TBM_SETTICFREQ` 是**不画**刻度的（默认频率 0） |
| `Enabled` / `Visible` | 通用窗口状态（`EnableWindow` / `ShowWindow`），读回是 VB6 的 `True`/`False` |

**本批还没有的一面**（`Min` / `Max` / `Value` / `SmallChange` / `LargeChange` / `SelStart` / `SelEnd` / `SelectRange` 与 `Change` / `Scroll` 两条事件）在 ai/029 的 C29-SL-b / SL-c 两格里排；`TickStyle` 四档与原生 `TBS_TOP`/`BOTTOM`/`LEFT`/`RIGHT`/`BOTH`/`NOTICKS` 的对应**本机拿不到 VB6 枚举真值**（OCX 未注册、类型库读不到），刻意没有实现，等口径再补。

**两个坑（实测踩过的）**：① 判方向**别问 `TBM_GETCHANNELRECT`** —— 它返回的矩形永远把行程长度放在 x 分量上，水平杆与垂直杆答同一组数，量它等于什么都没量；② `TBM_GETTHUMBRECT` / `GETCHANNELRECT` 的**返回值不是成功标志**（实测返回 0 而矩形填得好好的），只看矩形内容。

## 本项目的实现口径（ai/029 C29-SL-b：`Min` / `Max` / `Value` / `Small·LargeChange` / `Sel*`）

值面**全部直问直发控件**，没有一格自存。原生对应：`TBM_SETRANGE`/`GETRANGEMIN`/`GETRANGEMAX`、`TBM_SETPOS`/`GETPOS`、`TBM_SETLINESIZE`/`GETLINESIZE`、`TBM_SETPAGESIZE`/`GETPAGESIZE`、`TBM_SETSEL`/`GETSELSTART`/`GETSELEND`；`SelectRange` 是样式位 `TBS_ENABLESELRANGE`。

| 写法 | 读数与实现 |
| --- | --- |
| `Slider1.Min` / `.Max` | 原生这条消息只吃 **16 位**（`lParam` 是两个半字；实测 40000 会截成 -25536），所以下发前钳到 ±32767，**读回也是那一个钳过的值** —— 答出去的与控件真走得动的始终是同一个数。VB6 的 `Min`/`Max` 是 Long，超界那一档怎么办本机拿不到真值，押后 |
| `Slider1.Value` | `TBM_SETPOS`/`GETPOS`。**越界交给控件钳**（量程 10..100 时 `Value = 500` 读回 100、`= 5` 读回 10），我们不自己钳第二遍 |
| `Slider1.SmallChange` / `.LargeChange` | 原生 line / page 尺寸，真往返。实测默认档是 **1 / 20**（VB6 文档写 1 / 5）：小的一条对得上，大的那条本项目**照原生答 20**，不拿文档去改控件的读数 —— 等拿到 VB6 真值再拍 |
| `Slider1.SelectRange` | 样式位 `TBS_ENABLESELRANGE`，**运行期可改**（实测关掉之后 `CStr` 就答 `False`）。类型是 Boolean，所以 `CStr(sld.SelectRange)` 打 `True`/`False`、装箱是 `VT_BOOL` |
| `Slider1.SelStart` / `.SelEnd` | 只有 `SelectRange = True` 时设进去才生效（实测没挂那位时 `TBM_SETSEL` 答不回来）。原生"没设过"答 `-1` ⇒ getter 折成 **0**（无区段）。两端互相顶：起点越过终点时终点跟上来 |

**改设计期值面要注意顺序**：动 range 会让控件连带重算 `pos`、`page`、`selstart`（实测把 range 从 0..100 收到 10..100，`page` 从 20 变 18、`selstart` 跟到 10）。`vb6_Slider_Init` 的参数序因此是定死的：**range → line/page → pos → 刻度 → Sel**。

## 本项目的实现口径（ai/029 C29-SL-c：`Change` / `Scroll` 两条事件）

两条事件**不走 `WM_NOTIFY`**，走的是与 ScrollBar 同一条通道：控件给**父窗**发 `WM_HSCROLL`（横杆）或 `WM_VSCROLL`（竖杆），`LOWORD(wParam)` 是原生的 `TB_*` 码、`HIWORD(wParam)` 带当前值、`lParam` 就是控件句柄（派发按它认来源）。

一次真拖拽实测是这一串：`TB_THUMBTRACK(5)`×N → `TB_THUMBPOSITION(4)` → `TB_ENDTRACK(8)`；方向键是 `TB_LINEUP(0)` → `TB_ENDTRACK(8)`。

| 写法 | 触发与实现 |
| --- | --- |
| `Slider1_Change()` | 按"**值真的变了**"发（控件当前值 vs 上次派发时的基准）。所以拖拽过程中那一串 5 会连续触发，而落点那条 4 与收尾那条 8 不会再多发一次 |
| `Slider1_Scroll()` | 只认滑块那两档（码 4 / 5），与本项目 HScrollBar / VScrollBar 已发货的口径同一档（同一条原生通道不允许两套分法）。"点轨道 / 按方向键算不算 Scroll" 本机拿不到 VB6 真值 ⇒ 押后 |
| `Slider1.SimNotify(code, pos)` | **判据专用助手，不是 VB6 方法**（与 `DTPicker.SimChange` / `MonthView.SimDateClick` / `RichTextBox.SimNotify` 同先例）：先把值推到 `pos`（真手势都是控件先动、再发通知），再按原生那一档发一条**真**消息进父窗 |

**程序化写 `Value` 不会触发 `Change`**：实测 `TBM_SETPOS` / `TBM_SETRANGE` 一条通知都不发，本项目刻意照原生、不伪造（VB6 那颗 OCX 里这一走会 raise）。判据里 `SC10`/`SC11` 是这条口径的哨兵，哪天要齐平就得翻红逼人来拍。

## 本项目的实现口径（ai/029 C29-SL-d：常规事件面 `Click` / `DblClick` / `KeyDown` / `KeyUp`）

这一档**不走父窗那两条滚动通知**，走的是控件自己的子类过程（子类化换的是那一枚 HWND 的 `WNDPROC`，所以发给这个窗口的每一条消息都先过我们这一段，再转给原生过程）。

| 写法 | 原生落点与实现 |
| --- | --- |
| `Slider1_Click()` | `WM_LBUTTONUP`。实测真手势的下/抬两条都会到子类过程；裸的一条抬起**不会**惊动父窗那条通道（值不动 ⇒ `Change`、`Scroll` 都不跟） |
| `Slider1_DblClick()` | `WM_LBUTTONDBLCLK`，与 `Click` 是两档：合成一条双击消息只点 `DblClick`，`Click` 的计数不涨 |
| `Slider1_KeyDown(KeyCode, Shift)` | `WM_KEYDOWN`，`KeyCode` 就是 `wParam`（`VK_LEFT` 与 `vbKeyLeft` 同为 37）。**按键会让控件真动**（走 `SmallChange` 那一档）并发父窗 `TB_LINEUP(0)` ⇒ `Change` 会顺带跟一次；紧跟的 `WM_KEYUP` 发 `TB_ENDTRACK(8)`、值不再动 |
| `Slider1_KeyUp(KeyCode, Shift)` | `WM_KEYUP`，同上 |

三条实测口径值得记：

1. **不需要 `WS_TABSTOP` 也能收到按键**：原生轨道条在 `WM_LBUTTONDOWN` 里自己就把焦点抢过去了（实测：按下之后跟着一条 `WM_SETFOCUS`，"focus after click = 控件"）。所以"按一下方向键调音量"这种写法在本项目里是真的会跑。但 **Tab 键导航本身仍然不通** —— 主消息循环没有 `IsDialogMessage`，而且所有控件的创建样式都没挂 `WS_TABSTOP`（`Slider1.TabStop` 因此答 `False`）；那一整片缺口记在账 #83，不在本格。
2. **`Click` 这一档是通用的、不是 Slider 专属**：以前"控件级 `_Click` 处理器"只有那种会往父窗发原生通知的控件（命令按钮 / 复选 / 单选 / 列表 / 组合框 / 文件系统三件套 / SSTab / 工具栏）才会被接上，其余控件（Label / Image / PictureBox / Frame / TextBox / 滚动条 / Slider）的 `xxx_Click` 是**编得过、永远不被调用**的死代码。本格把这些补上了，同时按上表把那批"已有原生 Click 来源"的控件排除掉 —— 否则一次点击会从两条路各发一次。
3. **`Slider1.SimStdEvent(kind, wParam)` 是判据专用助手，不是 VB6 方法**（与 `SimNotify` / `DTPicker.SimChange` / `RichTextBox.SimNotify` 同先例）：`kind` 0=Click 1=DblClick 2=KeyDown 3=KeyUp，把对应的那条原生消息**同步**送进控件自己的过程。无头环境点不了鼠标，而直接调处理器会绕开整条派发链。

## 本项目的实现口径（ai/029 C29-SL-e：`ToolTipText` 与 `Tag`）

这两条是**所有可见控件通用**的属性（不只 Slider），本项目走同一对 RTL 入口：
`vb6_SetToolTipText` 把文本存进窗口属性 `VB6_ToolTipText`，并顺手把它注册到共享的 tooltip 控件
（`TOOLTIPS_CLASS`，`TTS_ALWAYSTIP`）；`vb6_SetControlTag` 同理存 `VB6_Tag`。

| 写法 | 读数与实现 |
| --- | --- |
| `Slider1.ToolTipText = "音量"` | 存一份拷贝（`SysAllocString`）并注册工具项；`CStr(Slider1.ToolTipText)` 打回 `音量`、`TypeName` 答 `String`、`VarType` 答 `8`。**没写过就读答案是空串**（VB6 同） |
| `Slider1.Tag = "sld-a"` | 同上，存在窗口属性里；`If Slider1.Tag = "sld-a"` 走字符串比较 |

两条以前都坏在**档位**上：读写面早就登记了，但类型表里没有它们，而 getter 的 C 返回型写 `void*`
⇒ 装箱那张 `_Generic` 表把"其他指针"送去 `VariantObject`，于是 `CStr` 打空、`TypeName` 答 `Object`、
`Tag` 的比较答假，而同一枚属性的 `Len` / `InStr` / 赋值这几条反而是对的。现在归到 `String` 档、
返回型改 `wchar_t*`，消费面统一。

**已知缺口（记账，未修）**：`.frm` 里设计期写的 `ToolTipText = "..."` 目前不会下发到窗口，
运行期读回来是空串；运行期赋值不受影响。
