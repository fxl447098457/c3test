VERSION 5.00
Begin VB.Form LvFrame 
   Caption         =   "LvFrame"
   ClientHeight    =   4200
   ClientLeft      =   120
   ClientTop       =   465
   ClientWidth     =   6600
   LinkTopic       =   "Form1"
   ScaleHeight     =   4200
   ScaleWidth      =   6600
   Begin VB.Timer Timer1 
      Enabled         =   -1  'True
      Interval        =   40
      Left            =   6000
      Top             =   120
   End
   Begin VB.CommandButton Command2 
      Caption         =   "COUT"
      Height          =   375
      Left            =   4560
      TabIndex        =   4
      Top             =   120
      Width           =   1000
   End
   Begin VB.Frame Frame1 
      Caption         =   "F"
      Height          =   900
      Left            =   120
      TabIndex        =   0
      Top             =   120
      Width           =   4200
      Begin VB.CommandButton Command1 
         Caption         =   "CIN"
         Height          =   375
         Index           =   0
         Left            =   2400
         TabIndex        =   2
         Top             =   240
         Width           =   1000
      End
      Begin VB.CheckBox Check1 
         Caption         =   "CB0"
         Height          =   255
         Index           =   0
         Left            =   240
         TabIndex        =   1
         Top             =   300
         Width           =   1200
      End
   End
   Begin ComctlLib.ListView ListView1 
      Height          =   1800
      Left            =   120
      TabIndex        =   3
      Top             =   1200
      Width           =   6300
      _ExtentX        =   11113
      _ExtentY        =   3175
      LabelWrap       =   -1  'True
      HideSelection   =   -1  'True
      _Version        =   327682
      View            =   3
   End
End
Attribute VB_Name = "LvFrame"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit

Private Declare Function SendMessage Lib "user32" Alias "SendMessageA" (ByVal hWnd As Long, ByVal wMsg As Long, ByVal wParam As Long, lParam As Any) As Long

Private Const LVM_SETEXTENDEDLISTVIEWSTYLE As Long = 4150
Private Const LVM_GETEXTENDEDLISTVIEWSTYLE As Long = 4151
Private Const LVS_EX_GRIDLINES As Long = 1
Private Const LVS_EX_FULLROWSELECT As Long = 32
Private Const BM_CLICK As Long = 245

' 账 #612 回归: 枚举成员引用模块级 Const 时必须真求值, 不能回落成声明序号
Private Const LSX_FULL As Long = &H20
Private Const LSX_TRACK As Long = &H8
Private Enum LvFlag
    FlagFull = LSX_FULL
    FlagTrack = LSX_TRACK
    FlagBoth = FlagFull Or FlagTrack
End Enum

Private Function GetEx() As Long
    GetEx = SendMessage(ListView1.hWnd, LVM_GETEXTENDEDLISTVIEWSTYLE, 0&, ByVal 0&)
End Function

Private Function ToggleEx(lv As ListView, ByVal NewStyle As Long, ByVal NewVal As Boolean) As Boolean
    ' 与 MListViewEx.bas 的 LVSetStyleEx 逐句对等: 对象形参取 .hWnd + GET/Xor/SET
    Dim nStyle As Long
    nStyle = SendMessage(lv.hWnd, LVM_GETEXTENDEDLISTVIEWSTYLE, 0&, ByVal 0&)
    If NewVal Then
        nStyle = nStyle Or NewStyle
    Else
        nStyle = nStyle Xor NewStyle
    End If
    ToggleEx = CBool(SendMessage(lv.hWnd, LVM_SETEXTENDEDLISTVIEWSTYLE, 0&, ByVal nStyle))
End Function

Private Sub Form_Load()
    Debug.Print "STYLE0=" & GetEx()
    ' 期望 32/8/40; 回落成序号时会是 0/1/2
    Debug.Print "ENUM=" & LvFlag.FlagFull & "/" & LvFlag.FlagTrack & "/" & LvFlag.FlagBoth
    ' 跨模块 (.bas 定义 / .frm 引用) 同口径
    Debug.Print "ENUM.X=" & LvFlag2.Flag2Full & "/" & LvFlag2.Flag2Track & "/" & LvFlag2.Flag2Both
End Sub

Private Sub Check1_Click(Index As Integer)
    Debug.Print "CB=" & Index
End Sub

Private Sub Command1_Click(Index As Integer)
    Debug.Print "CMDIN"
End Sub

Private Sub Command2_Click()
    Debug.Print "CMDOUT"
End Sub

Private Sub Timer1_Timer()
    Static step As Long
    step = step + 1
    Select Case step
    Case 1
        ' 原生通路: BM_CLICK = 鼠标点击后按钮给父窗发的 BN_CLICKED (与 SetCheckValue 的合成通告不同)
        SendMessage Check1(0).hWnd, BM_CLICK, 0&, ByVal 0&
        Debug.Print "AFTER_BMCLICK chk=" & Check1(0).Value
        Check1(0).Value = 1
        Debug.Print "AFTER_PROG chk=" & Check1(0).Value
        Command1(0).Value = True
        Command2.Value = True
    Case 2
        Debug.Print "STYLE_A=" & GetEx()
        SendMessage ListView1.hWnd, LVM_SETEXTENDEDLISTVIEWSTYLE, 0&, ByVal LVS_EX_GRIDLINES
        Debug.Print "STYLE_B=" & GetEx()
        SendMessage ListView1.hWnd, LVM_SETEXTENDEDLISTVIEWSTYLE, 0&, ByVal LVS_EX_FULLROWSELECT
        Debug.Print "STYLE_C=" & GetEx()
    Case 4
        Debug.Print "WRAP_ON=" & ToggleEx(ListView1, LVS_EX_GRIDLINES, True)
        Debug.Print "WRAP_A=" & GetEx()
        Debug.Print "WRAP_OFF=" & ToggleEx(ListView1, LVS_EX_GRIDLINES, False)
        Debug.Print "WRAP_B=" & GetEx()
    Case 5
        Debug.Print "LVFRAME-DONE"
        Unload Me
    End Select
End Sub
