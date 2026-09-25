# Shape 控件（形状控件）

> VB 内置轻量级图形控件，无需引用部件

## 1. 概述
在窗体上绘制矩形、正方形、椭圆、圆、圆角矩形等几何图形，用于界面装饰。
**核心特点：17 个属性、0 个事件、无自有方法**（无窗句柄的 windowless 控件）。

## 2. 属性一览（17 个）

| 属性 | 说明 |
|---|---|
| `(Name)` | 控件名称，默认 Shape1 |
| `BackColor` | 背景色。仅 BackStyle=1 时生效；FillStyle 为 2~7 时作填充图案底色 |
| `BackStyle` | 0 - Transparent 透明（默认）；1 - Opaque 不透明 |
| `BorderColor` | 边框颜色 |
| `BorderStyle` | 边框线型，见 §3.2 |
| `BorderWidth` | 边框宽度，**单位：像素**，默认 1 |
| `DrawMode` | 绘制模式 1~16，默认 13 - Copy Pen；Xor 类模式可做反色/闪烁 |
| `FillColor` | 内部填充颜色 |
| `FillStyle` | 内部填充图案，见 §3.3 |
| `Height / Width` | 高 / 宽，单位：缇（Twip） |
| `Left / Top` | 左 / 上位置，单位：缇 |
| `Index` | 控件数组索引（做一组指示灯时常用） |
| `Shape` | 形状类型，见 §3.1 |
| `Tag` | 备用字符串 |
| `Visible` | 是否可见 |

## 3. 枚举取值

### 3.1 Shape（形状）
0 矩形（默认）｜1 正方形｜2 椭圆｜3 圆｜4 圆角矩形｜5 圆角正方形

### 3.2 BorderStyle（边框）
0 透明｜1 实线（默认）｜2 虚线｜3 点线｜4 点划｜5 双点划｜6 内实线

### 3.3 FillStyle（填充）
0 实心｜1 透明不填充（默认，此时 FillColor 无效）｜2 水平线｜3 垂直线｜
4 上斜线｜5 下斜线｜6 十字网格｜7 斜十字网格
（2~7 时：线条色=FillColor，底色=BackColor）

### 3.4 BackStyle（背景）
0 透明（默认）｜1 不透明（显示 BackColor）

## 4. 事件：完全没有
无 Click / MouseDown / KeyDown 等任何事件（属性窗口"事件"页为空）。
原因：无窗句柄，不接收鼠标键盘消息。

**替代方案一：容器鼠标事件 + 坐标命中判断**

```vb
Private Sub Form_MouseUp(Button As Integer, Shift As Integer, X As Single, Y As Single)
    Dim cx As Single, cy As Single, r As Single
    cx = Shape1.Left + Shape1.Width / 2
    cy = Shape1.Top + Shape1.Height / 2
    r = Shape1.Width / 2
    If (X - cx) ^ 2 + (Y - cy) ^ 2 <= r ^ 2 Then MsgBox "点到圆了!"   ' 圆形判断
End Sub
```
（矩形则判断 X/Y 是否落在 Left~Left+Width、Top~Top+Height 内。）

**替代方案二：** 换用有事件的控件——Image（可贴图+完整鼠标事件）、
Label（有事件+背景色）、PictureBox（可绘图+响应鼠标）。

## 5. 示例与注意

```vb
Shape1.Shape = 3: Shape1.FillStyle = 0: Shape1.FillColor = vbRed   ' 红色实心圆
Shape1.BorderStyle = 2: Shape1.BorderWidth = 2                    ' 虚线边框

' 控件数组指示灯
Private Sub SetLamp(i As Integer, OnOff As Boolean)
    ShapeLamp(i).FillStyle = 0
    ShapeLamp(i).FillColor = IIf(OnOff, vbGreen, vbGray)
End Sub
```

- 尺寸/位置单位为缇，BorderWidth 为像素，勿混淆。
- FillStyle=1 时 FillColor 无效；BorderStyle=0 时 BorderColor/BorderWidth 无效。
- 改任何外观属性立即自动重绘，无需刷新。