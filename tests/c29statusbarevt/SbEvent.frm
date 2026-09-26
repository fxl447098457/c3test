VERSION 5.00
Begin VB.Form Form1
   Caption         =   "StatusBar事件判据"
   ClientHeight    =   3000
   ClientLeft      =   60
   ClientTop       =   345
   ClientWidth     =   5880
   Begin VB.StatusBar StatusBar1
      Align           =   2
      Height          =   375
      Left            =   0
      Top             =   2625
      Width           =   5880
   End
End
Attribute VB_Name = "Form1"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Private Sub Form_Load()
    ' 数据面 (P20-40 特例直译已通的那半) —— 建三个面板当点击目标
    StatusBar1.Panels.Add , "p1", "one"
    StatusBar1.Panels.Add , "p2", "two"
    StatusBar1.Panels.Add , "p3", "three"
    Debug.Print "SB1-COUNT=" & StatusBar1.Panels.Count
End Sub

' 程序化真实点击: RTL SimClick 发真 WM_NOTIFY → 窗体派发 → handler (判据专用)。
' ⚠ 必须放 Activate: dispatch 生成的分支有 `if (vb6_formLoading_X) break;`,
' Form_Load 阶段窗体还没显示, 事件会被"block events during form init"拦掉。
Private Sub Form_Activate()
    Static done As Integer
    If done Then Exit Sub
    done = 1
    StatusBar1.SimClick 1, 0
    StatusBar1.SimClick 3, 0
    StatusBar1.SimClick 2, 1
    Unload Me
End Sub

' 事件面: handler 内**真读** Panel 对象的成员 (移交记录里"没拍下来的那格")
Private Sub StatusBar1_PanelClick(ByVal Panel As Panel)
    Debug.Print "EVT1-CLICK-INDEX=" & Panel.Index
    Debug.Print "EVT2-CLICK-TEXT=" & Panel.Text
    Debug.Print "EVT3-CLICK-KEY=" & Panel.Key
    Panel.Text = Panel.Text & "!"
    Debug.Print "EVT4-CLICK-AFTER=" & Panel.Text
End Sub

Private Sub StatusBar1_PanelDblClick(ByVal Panel As Panel)
    Debug.Print "EVT5-DBL-INDEX=" & Panel.Index
End Sub
