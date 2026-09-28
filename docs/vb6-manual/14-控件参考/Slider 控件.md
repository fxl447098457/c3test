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

