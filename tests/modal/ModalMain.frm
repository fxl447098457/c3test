VERSION 5.00
Begin VB.Form ModalMain 
   Caption         =   "ModalMain"
   ClientHeight    =   1800
   ClientLeft      =   120
   ClientTop       =   465
   ClientWidth     =   3600
   LinkTopic       =   "ModalMain"
   ScaleHeight     =   1800
   ScaleWidth      =   3600
   Begin VB.TextBox txtSecond 
      Height          =   285
      Left            =   1800
      TabIndex        =   2
      Top             =   720
      Width           =   1455
   End
   Begin VB.TextBox txtMain 
      Height          =   285
      Left            =   240
      TabIndex        =   1
      Top             =   720
      Width           =   1455
   End
   Begin VB.Label lblMain 
      Caption         =   "main"
      Height          =   255
      Left            =   240
      TabIndex        =   0
      Top             =   240
      Width           =   1215
   End
   Begin VB.Timer tMain 
      Enabled         =   -1   'True
      Interval        =   40
      Left            =   2880
      Top             =   240
   End
End
Attribute VB_Name = "ModalMain"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit

' 账 #157 的夹具（判据建在"事件自己驱动 + LongPtr 形参比对"这两条已证的机器上）：
' VB6 把窗体显示出来时，焦点交给**这枚窗体里 TabIndex 最小的那枚拿得到焦点的控件** ——
' 不是创建顺序、也不是"停在窗体自己身上"。这里问两件事：
'   M1 = 普通（非模态、启动）窗体显示后，焦点是不是落在 txtMain（TabIndex=1）而不是
'        lblMain（TabIndex=0，Label 拿不到焦点）；
'   D* = 模态窗体里那一整套排除（见 ModalDlg）。
' 比对一律走 HexEq/N 这两个助手（`ByVal ... As LongPtr` 形参）—— 控件的 `.hwnd` 直接
' 装箱那条路是坏的（账 #159：CStr(ctl.hwnd) 打空串、与 GetFocus() 比较恒假），
' 拿它当判据就是又一次判据自伤。
Private gState As Long
Private gA As Long
Private gB As Long

Private Declare PtrSafe Function PostMessage Lib "user32" Alias "PostMessageW" (ByVal hWnd As LongPtr, ByVal Msg As Long, ByVal wParam As LongPtr, ByVal lParam As LongPtr) As Long
Private Declare PtrSafe Function GetFocus Lib "user32" () As LongPtr
Private Declare PtrSafe Function GetParent Lib "user32" (ByVal hWnd As LongPtr) As LongPtr

Private Const WM_CLOSE As Long = &H10

Private Function TF(ByVal ok As Boolean) As String
    If ok Then TF = "Y" Else TF = "N"
End Function

Private Function N(ByVal v As LongPtr) As String
    N = CStr(v)
End Function

Private Function HexEq(ByVal a As LongPtr, ByVal b As LongPtr) As String
    If a = b Then
        HexEq = "Y"
    Else
        HexEq = "N(" & CStr(a) & "<>" & CStr(b) & ")"
    End If
End Function

Private Sub Log1(ByVal s As String)
    Dim h As Long
    h = FreeFile
    Open App.Path & "\modal.log" For Append As #h
    Print #h, s
    Close #h
    Debug.Print s
End Sub

Private Sub tMain_Timer()
    gState = gState + 1
    If gState = 1 Then
        Log1 "M1-startup=" & HexEq(GetFocus(), txtMain.hwnd) & "/second=" & HexEq(GetFocus(), txtSecond.hwnd)
        tMain.Enabled = False
        ModalDlg.Show vbModal
        Log1 "M2-returned=Y/ticks=" & CStr(gState)
        Log1 "MODAL-DONE"
        Unload Me
    End If
End Sub
