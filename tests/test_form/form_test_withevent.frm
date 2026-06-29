VERSION 5.00
Begin VB.Form FormWE
   Caption         =   "P16 WithEvents Test"
   ClientHeight    =   3600
   ClientLeft      =   60
   ClientTop       =   390
   ClientWidth     =   4800
   StartUpPosition =   3
   Begin VB.CommandButton Command1
      Caption         =   "Click Me"
      Height          =   495
      Left            =   480
      Top             =   360
      Width           =   2175
   End
   Begin VB.TextBox Text1
      Text            =   "Hello"
      Height          =   375
      Left            =   480
      Top             =   1080
      Width           =   2175
   End
   Begin VB.Label Label1
      Caption         =   "Status"
      Height          =   375
      Left            =   480
      Top             =   1680
      Width           =   3855
   End
End
Attribute VB_Name = "FormWE"

' P16: WithEvents控件事件绑定测试
Dim WithEvents cmd As CommandButton
Dim WithEvents txt As TextBox

Dim clickCount As Long
Dim changeCount As Long

Private Sub Form_Load()
    clickCount = 0
    changeCount = 0
    ' 绑定WithEvents变量到控件
    Set cmd = Command1
    Set txt = Text1
    ' 测试WithEvents属性读取
    Label1.Caption = cmd.Caption
End Sub

Private Sub cmd_Click()
    clickCount = clickCount + 1
    ' 测试WithEvents属性写入
    cmd.Caption = "Clicked"
    Label1.Caption = "Clicks: "
End Sub

Private Sub txt_Change()
    changeCount = changeCount + 1
    Label1.Caption = "Changes: "
End Sub

Private Sub Form_Unload(Cancel As Integer)
    Cancel = 0
End Sub
