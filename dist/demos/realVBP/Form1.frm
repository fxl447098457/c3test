VERSION 5.00
Begin VB.Form Form1 
   Caption         =   "Form1"
   ClientHeight    =   6510
   ClientLeft      =   225
   ClientTop       =   870
   ClientWidth     =   6435
   LinkTopic       =   "Form1"
   ScaleHeight     =   6510
   ScaleWidth      =   6435
   StartUpPosition =   2  '屏幕中心
   Begin VB.ListBox List1 
      Height          =   1320
      Left            =   4440
      TabIndex        =   5
      Top             =   360
      Width           =   1695
   End
   Begin VB.OptionButton Option1 
      Caption         =   "Option1"
      Height          =   1455
      Left            =   480
      TabIndex        =   4
      Top             =   3480
      Width           =   975
   End
   Begin VB.CheckBox Check1 
      Caption         =   "Check1"
      Height          =   615
      Left            =   2280
      TabIndex        =   3
      Top             =   4440
      Width           =   2175
   End
   Begin VB.CommandButton Command1 
      Caption         =   "Command1"
      Height          =   495
      Left            =   1800
      TabIndex        =   2
      Top             =   3000
      Width           =   2655
   End
   Begin VB.TextBox Text1 
      Height          =   855
      Left            =   1320
      TabIndex        =   1
      Text            =   "Text1"
      Top             =   1680
      Width           =   2895
   End
   Begin VB.Label Label1 
      Caption         =   "Label1"
      Height          =   855
      Left            =   600
      TabIndex        =   0
      Top             =   360
      Width           =   3135
   End
   Begin VB.Menu MF 
      Caption         =   "文件"
      Begin VB.Menu MQ 
         Caption         =   "退出"
      End
   End
   Begin VB.Menu Mtest 
      Caption         =   "测试菜单"
   End
End
Attribute VB_Name = "Form1"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit

Private Sub Check1_Click()
    Option1.Visible = Check1.Value
End Sub

Private Sub Command1_Click()
    Text1.Text = Now
    Label1 = Text1
End Sub

Private Sub Form_Load()
    Dim a As New Class1
    a.Add "1", "2"
    Me.Caption = myName
    
    Dim i As Long
    For i = 0 To 5
        List1.AddItem "1 - " & i
    Next
    Check1.Value = 1
End Sub

Private Sub MQ_Click()
    End
End Sub

Private Sub Mtest_Click()
    MsgBox "test ok"
End Sub

Private Sub Option1_Click()
    Print Now
End Sub
