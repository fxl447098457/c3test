# Frame 控件

# Frame 控件

               

**Frame** 控件为控件提供可标识的分组。**Frame** 可以在功能上进一步分割一个窗体－例如，把 **OptionButton** 控件分成几组。

**语法**

**Frame**

**说明**

为了将控件分组，首先需要绘制 **Frame** 控件，然后绘制 **Frame** 里面的控件。这样就可以把框架和里面的控件同时移动。如果在 **Frame** 外部绘制了一个控件并试图把它移到框架内部，那么控件将在 **Frame** 的上部，这时需分别移动 **Frame** 和控件。

为了在 **Frame** 中选择多个控件，在使用鼠标在控件周围绘制框时，按住 CTRL 键。

## 本项目的实现口径（ai/029 账 #83：框架里的控件与框架外的对齐）

Frame（以及 `PictureBox` / `SSTab`）里的控件走的是**第二条创建路**，与窗体上的控件不是同一段代码。
本线已在这一族修过的三处，都是"外面那枚好、里面那枚静默丢"的形状：

| 属性 | 现在的行为 |
| --- | --- |
| `Text` / `Caption`、`Enabled` / `Visible` / `Value`、`ToolTipText` / `Tag` | 早已对齐（账 #125 / #142） |
| Slider 的 `Min` / `Max` / `Value` / `TickFrequency` / `Sel*` | 对齐（C29-SL-o，两条路共用同一处出口） |
| `FontName` / `FontSize` | 对齐（账 #154；设计期写的字体现在会打到窗口上） |
| `TabIndex` / `TabStop` | `TabStop` 默认 `True`（VB6 一致），且**框架里的控件与外面一样**立这一位（账 #83(a)）；`Label` / `Image` / `Shape` / `Line` / `Frame` 自己拿不到焦点，不立 |
| 窗体显示时的初始焦点 | 焦点交给**这枚窗体里 TabIndex 最小的那枚拿得到焦点的控件**（账 #157）。容器里的控件也算在内 —— 选目标时递归整棵控件树；设计期 `Enabled=False` / `Visible=False` 或写了 `TabStop=False` 的会被跳过。只应用**一次**（应用一次是本批选的口径）：之后再 Show 这枚窗体，不会把焦点从用户停下的地方抢回来；"再 Show 之后焦点回到上次那枚"这一形**没实测** |

> **键盘导航的现状**（三条都是实测，不是推的）：样式位立上了（账 #83(a)）、窗体显示时焦点自己就落在
> 第一枚 tabstop（账 #157）之后，**模态窗体里按 Tab 已经真跳格**（往当前焦点 post 一对 `VK_TAB`，
> 下一拍焦点就到了下一枚 —— `IsDialogMessageW` 那条路是通的）。**普通（非模态）窗体现在也走这一条**：
> 主消息循环每条消息先问一次 `IsDialogMessage(GetActiveWindow(), …)`（账 #83(b1)），实测站点序列
> `txtMain → cmdY → cmdX → txtSecond → 回 txtMain`。⚠ 走的是 **z-order（≈创建顺序）不是 `TabIndex` 顺序**
> —— VB6 那个口径还差一刀，记账 #163。框架这一层另有一刀**还没补**：
> 实测（探针 `.build/slfr`，tab 序 0=cmdA、1=cmdB、2=Frame 里 3/4 两枚按钮，每拍 post 一对
> `VK_TAB`）站点序列是 `A → B → A → B` —— 对话框管理器**不走进框架**（也不停在框架本身），
> 因为容器没带 `WS_EX_CONTROLPARENT`。所以框架里的控件要参与跳格，得先补那一位（账 #83(b2)；
> 夹具 `tests/tabwalk` 已把改前 baseline 钉住：从 Frame 里的 `cmdIn1` 起步只走得到
> `cmdIn1, cmdTop2, cmdTop1`，`cmdIn2` 与嵌在框架里的框架那枚 `cmdDeep` 一步不到）。
> 运行期现给焦点是通的：`CmdOk.SetFocus` 实测落地（探针那枚按钮不在框架里 —— 框架里的子控件这一形本批没重测）。
> 但按到之后 `_GotFocus` 不会发 —— 那是账 #158：Windows 对 BUTTON 的程序化 SetFocus 不发 `BN_SETFOCUS`。
