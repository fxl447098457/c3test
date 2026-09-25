# ProgressBar 控件（MSComctlLib）

> 组件：Microsoft Windows Common Controls 6.0 (SP6)，MSCOMCTL.OCX

## 1. 概述
进度条控件，用于显示耗时操作（复制文件、导入数据、启动加载）的完成百分比。
纯显示控件：**无事件、无自有方法**。

## 2. 主要属性

| 属性 | 说明 |
|---|---|
| `Min / Max` | 取值范围，默认 0 ~ 100 |
| `Value` | 当前进度值（必须在 Min~Max 之间，否则报错） |
| `Orientation` | 0 - ccOrientationHorizontal 水平（默认）；1 - ccOrientationVertical 垂直 |
| `Scrolling` | 填充样式：ccScrollingSmooth 平滑连续 / ccScrollingStandard 分块格状 |
| `(Name) / Index / Tag / Visible / Left / Top / Width / Height` | 通用属性 |

## 3. 使用要点
- 长循环中必须加 `DoEvents`，否则界面冻结、进度条不刷新。
- 进度未知时可用定时器来回滚动 Value 模拟"忙碌"效果（VB6 无 marquee 模式）。
- 常与 StatusBar 面板文字配合：进度条显示比例，状态栏显示说明。

## 4. 示例

```vb
Private Sub cmdStart_Click()
    Dim i As Long
    ProgressBar1.Min = 0: ProgressBar1.Max = 100: ProgressBar1.Value = 0
    For i = 1 To 100
        ' ...这里做实际工作...
        ProgressBar1.Value = i
        StatusBar1.Panels(1).Text = "已完成 " & i & "%"
        DoEvents                      ' 让界面刷新
    Next
    StatusBar1.Panels(1).Text = "完成"
End Sub
```