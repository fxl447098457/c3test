# CommonDialog 控件（通用对话框）

> 组件：Microsoft Common Dialog Control 6.0 (SP6)，COMDLG32.OCX

## 1. 概述
封装的 Windows 标准对话框，**非可视控件**，靠调用方法弹出对话框；本身**没有事件**。

## 2. 六类对话框与方法

| 方法 | 对话框 | 旧式 Action 值 |
|---|---|---|
| `ShowOpen` | 打开文件 | 1 |
| `ShowSave` | 保存文件 | 2 |
| `ShowColor` | 选择颜色 | 3 |
| `ShowFont` | 选择字体 | 4 |
| `ShowPrinter` | 打印设置 | 5 |
| `ShowHelp` | 帮助 | 6 |

## 3. 主要属性（按对话框分组）

| 分组 | 属性 |
|---|---|
| 通用 | `CancelError`（True 时用户取消会触发错误 cdlCancel=32755）、`DialogTitle`、`Flags` |
| 打开/保存 | `FileName`、`FileTitle`、`Filter`、`FilterIndex`、`InitDir`、`DefaultExt` |
| 颜色 | `Color`（进出参数）、`Flags`（如 cdlCCRGBInit） |
| 字体 | `FontName`、`FontSize`、`FontBold`、`FontItalic`、`Color`（字体色，需 cdlCFEffects）、`Min / Max`（字号范围）、`Flags`（必须含 cdlCFScreenFonts 等，否则报"无字体"错误） |
| 打印 | `Copies`、`FromPage`、`ToPage`、`hDC`（只读，取打印机设备上下文） |

**Filter 语法：** `"描述|通配符|描述|通配符"`，如 `"文本文件 (*.txt)|*.txt|所有文件 (*.*)|*.*"`

## 4. 示例

```vb
Private Sub cmdOpen_Click()
    On Error GoTo Cancelled
    With CommonDialog1
        .CancelError = True
        .DialogTitle = "打开文件"
        .Filter = "文本文件 (*.txt)|*.txt|所有文件 (*.*)|*.*"
        .FilterIndex = 1
        .Flags = cdlOFNFileMustExist Or cdlOFNPathMustExist
        .ShowOpen
        txtPath.Text = .FileName
    End With
    Exit Sub
Cancelled:
    If Err.Number = cdlCancel Then Exit Sub     ' 32755：用户点了取消
    MsgBox Err.Description
End Sub

Private Sub cmdFont_Click()
    With CommonDialog1
        .Flags = cdlCFScreenFonts Or cdlCFEffects
        .ShowFont
        Text1.FontName = .FontName
        Text1.FontSize = .FontSize
        Text1.FontBold = .FontBold
        Text1.ForeColor = .Color
    End With
End Sub
```