VERSION 5.00
Begin VB.Form CtrlStateForm 
   Caption         =   "CtrlStateForm"
   ClientHeight    =   3000
   ClientLeft      =   120
   ClientTop       =   465
   ClientWidth     =   4200
   LinkTopic       =   "Form1"
   ScaleHeight     =   3000
   ScaleWidth      =   4200
   Begin VB.Timer tProbe 
      Enabled         =   -1  'True
      Interval        =   50
      Left            =   3720
      Top             =   2400
   End
   Begin VB.Frame fr 
      Caption         =   "容器"
      Height          =   2100
      Left            =   120
      TabIndex        =   0
      Top             =   120
      Width           =   1800
      Begin VB.CheckBox cbIn 
         Caption         =   "内勾"
         Value           =   1  'Checked
         Height          =   300
         Left            =   120
         TabIndex        =   1
         Top             =   240
         Width           =   1500
      End
      Begin VB.CheckBox cbDisIn 
         Caption         =   "内禁"
         Enabled         =   0  'False
         Height          =   300
         Left            =   120
         TabIndex        =   2
         Top             =   600
         Width           =   1500
      End
      Begin VB.CheckBox cbHidIn 
         Caption         =   "内藏"
         Visible         =   0  'False
         Height          =   300
         Left            =   120
         TabIndex        =   3
         Top             =   960
         Width           =   1500
      End
      Begin VB.Label lbIn 
         Caption         =   "内标"
         Height          =   300
         Left            =   120
         TabIndex        =   4
         Top             =   1320
         Width           =   1500
      End
   End
   Begin VB.CheckBox cbOut 
      Caption         =   "外勾"
      Value           =   1  'Checked
      Height          =   300
      Left            =   2160
      TabIndex        =   5
      Top             =   120
      Width           =   1800
   End
   Begin VB.CheckBox cbDisOut 
      Caption         =   "外禁"
      Enabled         =   0  'False
      Height          =   300
      Left            =   2160
      TabIndex        =   6
      Top             =   480
      Width           =   1800
   End
   Begin VB.CheckBox cbHidOut 
      Caption         =   "外藏"
      Visible         =   0  'False
      Height          =   300
      Left            =   2160
      TabIndex        =   7
      Top             =   840
      Width           =   1800
   End
   Begin VB.CheckBox cbDef 
      Caption         =   "默认"
      Height          =   300
      Left            =   2160
      TabIndex        =   8
      Top             =   1200
      Width           =   1800
   End
   Begin VB.OptionButton obDef 
      Caption         =   "默认选"
      Height          =   300
      Left            =   2160
      TabIndex        =   9
      Top             =   1560
      Width           =   1800
   End
   Begin VB.OptionButton obOn 
      Caption         =   "选中"
      Value           =   -1  'True
      Height          =   300
      Left            =   2160
      TabIndex        =   10
      Top             =   1920
      Width           =   1800
   End
   Begin VB.Label lbOut 
      Caption         =   "外标"
      Height          =   300
      Left            =   120
      TabIndex        =   11
      Top             =   2400
      Width           =   1800
   End
End
Attribute VB_Name = "CtrlStateForm"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit

' ai/029 账 #125: 设计期 Enabled / Visible / CheckBox·OptionButton 的 Value 三件
' 以前两条创建路都没打到窗口上（发码里 BM_SETCHECK / EnableWindow / ShowWindow 各 0 次）。
' 判据纪律：**Visible 只能在窗体真显示之后问** —— Form_Load 里父窗还没 Show，
' IsWindowVisible 对任何控件都返回假（第一版探针就是这么假绿的）。所以读数放 Timer。
' 负向那一半同样是判据：没写过这三项的控件必须照旧（cbDef 不勾、不藏、不灰，
' fr 仍可见）—— 少了它，把"全都设一遍"的写错了也照样绿。

Private Function TF(ByVal ok As Boolean) As String
    If ok Then TF = "Y" Else TF = "N"
End Function

Private Sub Form_Load()
    ' 装载期问的是"窗口还没显示"那一态，只记不判（DS0 两条与 Timer 里的 DS5/DS6 对照）
    Debug.Print "DS0=" & CStr(cbHidIn.Visible) & CStr(cbHidOut.Visible)
End Sub

Private Sub tProbe_Timer()
    tProbe.Enabled = False
    Debug.Print "DS1=" & CStr(cbIn.Value) & CStr(cbOut.Value)          ' 设计期 Value=1（两条路）
    Debug.Print "DS2=" & CStr(cbDisIn.Enabled) & CStr(cbDisOut.Enabled) ' 设计期 Enabled=0
    Debug.Print "DS3=" & CStr(cbHidIn.Visible) & CStr(cbHidOut.Visible) ' 设计期 Visible=0
    Debug.Print "DS4=" & CStr(cbDef.Value) & CStr(cbDef.Visible) & CStr(cbDef.Enabled)
    Debug.Print "DS5=" & CStr(obDef.Value) & CStr(obOn.Value)           ' OptionButton 两个方向
    Debug.Print "DS6=" & TF(fr.Visible) & TF(lbIn.Caption = "内标") & TF(lbOut.Caption = "外标")
    Debug.Print "DS7=" & CStr(obDef.Enabled)
    Debug.Print "CTRLSTATE-DONE"
    Unload Me
End Sub
