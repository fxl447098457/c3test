# Shape 控件

# Shape 控件

               

**Shape** 控件是图形控件，显示矩形、正方形、椭圆、圆形、圆角矩形或者圆角正方形。

**语法**

**Shape**

**说明**

或者不调用在运行时的 **Circle** 和 **Line** 方法而使用在设计时的 **Shape** 控件，或者二者同时使用。可以在容器中绘制 **Shape** 控件，但是不能把该控件当作容器。设置 **BorderStyle** 属性产生的效果取决于 **BorderWidth** 属性的设置。如果 **BorderWidth** 不是 1，并且 **BorderStyle** 不是 0 或者 6，则将 **BorderStyle** 设置成 1。
## 本项目的实现口径（ai/029 C29-1a）

`Shape` 在本项目里是**有窗句柄的自绘控件**（自注册窗口类 `VB6_SHAPE`），与 VB6 的 windowless 实现不同；但对外的语言面一致 —— **它仍然没有任何事件**，`Click` / `MouseDown` / `KeyDown` 都不会触发，要命中判断请按本页上文用容器的鼠标事件 + 坐标比较。

| 写法 | 读数 |
| --- | --- |
| `Shape1.Left / Top / Width / Height` | 缇（容器坐标），与 `.frm` 设计期一致 |
| `Shape1.Shape / FillStyle / BorderStyle / BorderWidth` | 按设置的值原样读回，包括值为 `0` 的那些（`Shape=0` 矩形、`FillStyle=0` 实心、`BorderStyle=0` 透明） |
| `Shape1.FillColor / BorderColor` | 颜色值原样读回 |
| 改任一外观属性 | 立即重绘，不需要手动刷新（与 VB6 同） |

本页上文那条规则**已实现**：`BorderWidth` 设成非 1 而 `BorderStyle` 又不是 0（透明）或 6（内实线）时，`BorderStyle` 被强制回 1（实线）—— 判据在 `tests\ctrlshape\`（CS15、CS16）。

尚未落地的三项，写在这里免得按 VB6 去期待：`DrawMode`（1~16 的绘制模式，含 Xor 反色）、`BackStyle` / `BackColor`（背景是否不透明；当前实现背景恒为透明）、`FillStyle` 2~7 网格图案的**底色**（图案线色用 `FillColor`，底色 `BackColor` 不生效）。
