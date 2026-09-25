# OLE 容器控件

> VB 内置控件，无需引用部件

## 1. 概述
在窗体中**嵌入或链接**外部对象（Excel 工作表、Word 文档、公式、图片等），
支持就地激活编辑（双击进入对象自己的编辑界面）。

## 2. 主要属性

| 属性 | 说明 |
|---|---|
| `Class` | 对象类名："Excel.Sheet.8"、"Word.Document.8"、"Equation.3" 等 |
| `OLEType` | （只读）0 - 链接、1 - 嵌入、3 - 无对象 |
| `OLETypeAllowed` | 0 - 仅链接、1 - 仅嵌入、2 - Either 两者皆可（默认） |
| `SourceDoc` | 链接源文件路径（链接方式时） |
| `SourceItem` | 源文件中的项目（如 Excel 区域） |
| `DisplayAsIcon` | True 时以图标形式显示 |
| `AutoActivate` | 激活方式：0 手动、1 获焦点、2 双击（默认）、3 单击 |
| `AutoVerbMenu` | True 时右键弹出对象动词菜单（默认） |
| `SizeMode` | 0 - Clip 裁剪、1 - Stretch 拉伸、2 - Autosize 控件随对象、3 - Auto 对象随控件 |
| `BorderStyle` | 0 - 无边框、1 - 单线边框 |
| `Object` | （只读）对象自动化接口，可后期绑定操作 |
| `DataSource / DataField` | 可绑定到含 OLE 对象的数据库字段 |

## 3. 事件
很少：Click、DblClick、GotFocus、LostFocus、DragDrop、DragOver 等。
> 对象被就地激活后，键盘/鼠标输入由嵌入的应用程序接管，容器不再收到。

## 4. 主要方法
`InsertObjDlg` 弹出系统"插入对象"对话框｜`CreateEmbed` 创建嵌入｜
`CreateLink` 创建链接｜`DoVerb(n)` 执行第 n 个动词（0 为主动词，如"编辑"）｜
`Close`｜`Copy`｜`Paste`｜`PasteSpecialDlg`｜`Delete`｜`FetchVerbs`｜
`SaveToFile / LoadFromFile / ReadFromFile / WriteToFile`

## 5. 示例

```vb
OLE1.InsertObjDlg                    ' 让用户选择插入/嵌入对象
OLE1.SizeMode = 2                    ' 控件大小自动适应对象
OLE1.DoVerb 0                        ' 执行主动词（打开/编辑）
OLE1.SaveToFile "C:\obj.dat"         ' 保存对象到文件
```