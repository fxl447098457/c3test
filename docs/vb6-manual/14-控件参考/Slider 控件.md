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
