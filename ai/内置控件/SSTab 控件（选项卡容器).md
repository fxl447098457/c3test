# SSTab 控件（选项卡容器）

> 组件：Microsoft Tabbed Dialog Control 6.0 (SP6)，TABCTL32.OCX

## 1. 概述
选项卡容器控件，每个标签页可容纳自己的子控件，适合做"属性页"式多页界面。

## 2. 主要属性

| 属性 | 说明 |
|---|---|
| `Tabs` | 标签页总数 |
| `Tab` | 当前活动页索引（**从 0 开始**） |
| `TabCaption(i)` | 第 i 页的标题文字 |
| `TabVisible(i)` | 第 i 页是否可见（可借此隐藏某些页） |
| `TabOrientation` | 标签位置：0 - 上（默认）、1 - 下、2 - 左、3 - 右 |
| `TabStyle` | 0 - 选项卡对话框式（默认）、1 - 属性页式 |
| `TabsPerRow` | 每行显示的标签数 |
| `WordWrap` | 标题过长时是否换行 |

## 3. 事件

| 事件 | 说明 |
|---|---|
| `Click(PreviousTab As Integer)` | 切换标签页时触发，参数为**切换前**的页索引 |
| 通用事件 | DblClick、DragDrop、DragOver、GotFocus、LostFocus 等 |

## 4. 使用要点与示例

- **设计期**：先选中 SSTab 再放控件，控件即属于当前页；点击标签或右键属性页切换页面。
- **运行期**：用 `Tab` 属性切页；子控件随页自动显示/隐藏。

```vb
SSTab1.Tabs = 3
SSTab1.TabCaption(0) = "常规"
SSTab1.TabCaption(1) = "视图"
SSTab1.TabCaption(2) = "高级"

Private Sub SSTab1_Click(PreviousTab As Integer)
    Debug.Print "从第 " & PreviousTab & " 页切到第 " & SSTab1.Tab & " 页"
End Sub
```