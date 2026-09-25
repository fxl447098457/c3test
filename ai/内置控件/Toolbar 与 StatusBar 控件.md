# Toolbar 与 StatusBar 控件

> 组件：Microsoft Windows Common Controls 6.0 (SP6)，MSCOMCTL.OCX

## 1. 概述
Toolbar 通常配合 **ImageList** 提供按钮图标；StatusBar 停靠在窗体底部显示状态信息。

## 2. Toolbar 主要属性与事件

**主要属性：**
| 属性 | 说明 |
|---|---|
| `Align` | 停靠：0 无、1 靠上（默认）、2 靠下、3 靠左、4 靠右 |
| `ImageList` | 关联的 ImageList 控件（提供按钮图标） |
| `Buttons` | 按钮集合（设计期用属性页添加，运行期用 `Buttons.Add`） |
| `TextStyle` | 0 - 文字在图标下方（默认）、1 - 文字在图标右侧 |
| `AllowCustomize` | 是否允许用户双击自定义工具栏 |
| `ShowTips` | True 时显示按钮工具提示 |

**Button 对象要点：** 
`Key`（字符串标识）、`Caption`、`Image`（ImageList 中图标索引）、`Enabled`、`Width`、`ToolTipText`；
`Style`：0 普通、1 复选（可按下）、2 按钮组（单选互斥）、3 分隔符、4 占位符、5 下拉菜单；
`Value`：0 未按下、1 已按下（仅 Style=1/2 有意义）。

**核心事件：**
- `ButtonClick(Button As Button)`：点击按钮时触发，用 `Button.Key` 或 `Index` 分发。
- `ButtonMenuClick(ButtonMenu As ButtonMenu)`：点击下拉按钮（Style=5）的菜单项时触发。

## 3. StatusBar 主要属性与事件

**主要属性：**
| 属性 | 说明 |
|---|---|
| `Align` | 默认 2 - 靠下 |
| `Style` | 0 - 多面板（默认）、1 - 简单模式（单格） |
| `SimpleText` | 简单模式下显示的文字 |
| `Panels` | 面板集合 |

**Panel 对象要点：** 
`Key`、`Text`、`Width / MinWidth`、`ToolTipText`；
`AutoSize`：0 固定宽、1 弹簧（自动伸展填满）、2 随内容；
`Style`：0 文本、1 CapsLock 状态、2 NumLock 状态、5 时间、6 日期（1~6 为系统自动显示）。

**核心事件：**
- `PanelClick(Panel)`、`PanelDblClick(Panel)`：点击/双击某个面板时触发。

## 4. 示例

```vb
' 运行期添加工具栏按钮：Add([index], [key], [caption], [style], [image])
Toolbar1.Buttons.Add , "New", "新建", tbrDefault, 1
Toolbar1.Buttons.Add , , , tbrSeparator          ' 分隔符
Toolbar1.Buttons.Add , "Save", "保存", tbrDefault, 2

Private Sub Toolbar1_ButtonClick(ByVal Button As MSComctlLib.Button)
    Select Case Button.Key
        Case "New":  mnuNew_Click
        Case "Save": mnuSave_Click
    End Select
End Sub

' 状态栏：第 1 格弹簧伸展显示提示，第 2 格自动显示时间
StatusBar1.Panels(1).AutoSize = sbrSpring
StatusBar1.Panels(1).Text = "就绪"
StatusBar1.Panels(2).Style = sbrTime
```