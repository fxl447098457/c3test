VERSION 5.00
Begin VB.Form TbForm 
   Caption         =   "TbForm"
   ClientHeight    =   3200
   ClientLeft      =   120
   ClientTop       =   465
   ClientWidth     =   6000
   LinkTopic       =   "TbForm"
   ScaleHeight     =   3200
   ScaleWidth      =   6000
   Begin VB.Toolbar tb1 
      Align           =   2   '靠下
      Height          =   300
      Left            =   240
      TabIndex        =   0
      Top             =   240
      TextStyle       =   1   '文字在右侧
      Width           =   4800
      Buttons(1)      =   "Open"
         .Key        =   "open"
         .Style      =   0
      End
      Buttons(2)      =   ""
         .Style      =   3   '分隔符
         .Width      =   8
      End
      Buttons(3)      =   "Save"
         .Key        =   "save"
         .Style      =   0
         .ToolTipText=   "Save all"
      End
   End
   Begin VB.Toolbar tb2 
      Height          =   300
      Left            =   240
      TabIndex        =   1
      Top             =   720
      Width           =   2400
   End
   Begin VB.ListBox auxList 
      Height          =   600
      Left            =   3000
      TabIndex        =   2
      Top             =   1200
      Width           =   1200
   End
End
Attribute VB_Name = "TbForm"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit

' ai/029 C29-5a: Toolbar 换成原生 ToolbarWindow32（不加载 MSCOMCTL.OCX）。
' 改之前这枚控件**连窗口都没有**：controlTypeToWin32Class 缺格，而且被"ImageList || Toolbar
' 走 CoCreateInstance"那一组扣住 (直接 continue) ⇒ vb6_hwnd_tb1 压根不声明 —— 实测读一个
' tb1.Visible 就是 `C2065: vb6_hwnd_tb1 未声明的标识符`，整件工程编不过 (那组在 64 位里本来就
' 是静默空转)。本批三面一起接上：创建那一刀、标量属性表、设计期 Buttons 逐条 TB_ADDBUTTONSW。
' 口径: Buttons.Count 的读数来自**原生 TB_BUTTONCOUNT**，不是我那张表自己报数 —— 所以它证的
' 是"设计期那三条真进了控件"。分隔符也计入原生按钮数 (与 VB6 一致)。
' Button 对象那一族 ((i).Key/.Caption、Add、ButtonClick) 留 5b：它吃成员对象机制。

Private Function TF(ByVal ok As Boolean) As String
    If ok Then TF = "Y" Else TF = "N"
End Function

Private Sub Form_Load()

    ' --- 1. 设计期三条按钮真进了控件 (含一条分隔符) ---
    Debug.Print "TB1=" & TF(tb1.Buttons.Count = 3)

    ' --- 2. 没写 Buttons 的那枚不被继承 ---
    Debug.Print "TB2=" & TF(tb2.Buttons.Count = 0)

    ' --- 3. 矩形是按 .frm 给的 (CCS_NORESIZE|CCS_NOPARENTALIGN 那两位的读数) ---
    Debug.Print "TB3=" & TF(tb1.Width = 4800 And tb1.Height = 300)
    Debug.Print "TB4=" & TF(tb2.Width = 2400 And tb2.Height = 300)

    ' --- 4. 标量属性: 设计期写的读回、没写的走 VB6 默认 ---
    Debug.Print "TB5=" & TF(tb1.TextStyle = 1 And tb2.TextStyle = 0)
    Debug.Print "TB6=" & TF(tb1.Align = 2 And tb2.Align = 1)
    Debug.Print "TB7=" & TF(tb1.ShowTips <> 0)

    ' --- 5. 运行期可逆赋值 (真值就是窗口样式位) ---
    tb2.ShowTips = False
    tb2.TextStyle = 1
    tb2.AllowCustomize = True
    Debug.Print "TB8=" & TF(tb2.ShowTips = 0 And tb2.TextStyle = 1 And tb2.AllowCustomize <> 0)
    tb2.ShowTips = True
    tb2.TextStyle = 0
    tb2.AllowCustomize = False
    Debug.Print "TB9=" & TF(tb2.ShowTips <> 0 And tb2.TextStyle = 0 And tb2.AllowCustomize = 0)

    ' --- 6. 两枚不串: 改 tb2 不动 tb1 ---
    Debug.Print "TB10=" & TF(tb1.TextStyle = 1 And tb1.Align = 2 And tb1.Buttons.Count = 3)

    ' --- 7. 通用属性面没被抢走 (改之前正是这条路编不过) ---
    ' 这里刻意不问 Visible: 无头跑里父窗从未 ShowWindow，任何子窗口的 Visible 读数都是假
    ' (实测 TreeView 那条 TV1 也一样) —— 换成与父窗无关的 Enabled 才问得出东西。
    tb2.Enabled = False
    Debug.Print "TB11=" & TF(tb2.Enabled = 0 And tb1.Enabled <> 0)
    tb2.Enabled = True
    auxList.AddItem "one"
    Debug.Print "TB12=" & TF(auxList.ListCount = 1 And tb2.Enabled <> 0)

    Debug.Print "CTRLTOOLBAR-DONE"
    Unload Me
End Sub
