VERSION 5.00
Begin VB.Form CtrlProp 
   Caption         =   "CtrlProp"
   ClientHeight    =   3000
   ClientLeft      =   120
   ClientTop       =   465
   ClientWidth     =   6000
   LinkTopic       =   "Form1"
   ScaleHeight     =   3000
   ScaleWidth      =   6000
   StartUpPosition =   3  '窗口缺省
   Begin VB.ListBox List2 
      Height          =   600
      Left            =   2400
      TabIndex        =   4
      Top             =   2160
      Width           =   2000
   End
   Begin VB.ListBox List1 
      Height          =   600
      Left            =   240
      TabIndex        =   3
      Top             =   2160
      Width           =   2000
   End
   Begin VB.TextBox txtOne 
      Height          =   300
      Left            =   240
      TabIndex        =   2
      Text            =   "T"
      Top             =   1680
      Width           =   2000
   End
   Begin VB.Label lblArr 
      Caption         =   "A1"
      Height          =   300
      Index           =   1
      Left            =   2400
      TabIndex        =   1
      Top             =   1080
      Width           =   2000
   End
   Begin VB.Label lblArr 
      Caption         =   "A0"
      Height          =   300
      Index           =   0
      Left            =   240
      TabIndex        =   0
      Top             =   1080
      Width           =   2000
   End
   Begin VB.Label lblSingle 
      Caption         =   "S"
      Height          =   300
      Left            =   240
      TabIndex        =   5
      Top             =   600
      Width           =   2000
   End
End
Attribute VB_Name = "CtrlProp"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit

' Fix 194 回归: 控件属性的字符串写入值必须转 BSTR, 且 List(j) 参与字符串相等比较
' 必须按 BSTR 处理 (RTL 里 vb6_GetListItem 的 C 返回类型是 void*)。
' 旧行为: ① lblArr(n).Caption = <数值> 生成的 C 把整数当 BSTR 指针 →
'            非 0 时 0xC000041D (用户回调未处理异常), 为 0 时静默不生效;
'         ② B = List2.List(J) 恒为假 (void* 被 _Generic 判成 VariantObject, CStr 得空串)。
Private Sub Form_Load()
    Dim I As Long
    Dim J As Long
    Dim B As String
    Dim hit As Long
    List1.AddItem "aaa"
    List1.AddItem "bbb"
    List2.AddItem "aaa"
    List2.AddItem "ccc"
    lblArr(0).Caption = List1.ListCount
    lblArr(1).Caption = List2.ListCount
    Debug.Print "CP1=" & lblArr(0).Caption
    Debug.Print "CP2=" & lblArr(1).Caption
    hit = 0
    For I = 0 To List1.ListCount - 1
        B = List1.List(I)
        For J = 0 To List2.ListCount - 1
            If B = List2.List(J) Then
                hit = hit + 1
                Exit For
            End If
        Next
    Next
    Debug.Print "CP3=" & CStr(hit)
    lblSingle.Caption = List1.ListCount
    txtOne.Text = List1.ListCount
    Debug.Print "CP4=" & lblSingle.Caption
    Debug.Print "CP5=" & txtOne.Text
    Debug.Print "CTRLPROP-DONE"
    Unload Me
End Sub
