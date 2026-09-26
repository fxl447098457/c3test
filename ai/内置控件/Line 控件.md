# Line 控件（线条控件）

> VB 内置轻量级图形控件，无需引用部件

## 1. 概述
与 Shape 同族，只画一条直线（水平/垂直/斜线），常用作分隔线。
**与 Shape 一样：无事件、无自有方法。**

## 2. 主要属性

| 属性 | 说明 |
|---|---|
| `X1 / Y1` | 起点坐标（相对容器客户区，单位：缇） |
| `X2 / Y2` | 终点坐标 |
| `BorderColor` | 线条颜色 |
| `BorderStyle` | 0 透明、1 实线（默认）、2 虚线、3 点线、4 点划、5 双点划、6 内实线 |
| `BorderWidth` | 线宽（像素），默认 1 |
| `DrawMode` | 绘制模式 1~16，默认 13 - Copy Pen |
| `Index / Tag / Visible / (Name)` | 控件数组索引 / 备用字符串 / 是否可见 / 名称 |

> Line 没有 Left/Top/Width/Height，位置完全由 X1,Y1,X2,Y2 决定。
> 画分隔线：水平令 Y1=Y2，垂直令 X1=X2。

## 3. 事件
**无任何事件**（原因同 Shape）。需要"可点击的线"请用 Label（设 Height 很小）或 PictureBox.Line 自绘。

## 4. 示例

```vb
Line1.X1 = 0: Line1.Y1 = 500
Line1.X2 = Me.ScaleWidth: Line1.Y2 = 500   ' 横贯窗体的水平分隔线
Line1.BorderStyle = 2                      ' 虚线
```