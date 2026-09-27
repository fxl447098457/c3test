VERSION 5.00
Begin VB.Form frmMain 
   Caption         =   "dbgdlg"
   ClientHeight    =   900
   ClientWidth     =   2400
   LinkTopic       =   "Form1"
   ScaleHeight     =   900
   ScaleWidth      =   2400
End
Attribute VB_Name = "frmMain"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit

Private Declare Function GlobalAlloc Lib "kernel32" (ByVal wFlags As Long, ByVal dwBytes As Long) As Long
Private Declare Function GlobalLock Lib "kernel32" (ByVal hMem As Long) As Long
Private Declare Sub CopyMemory Lib "kernel32" Alias "RtlMoveMemory" (hpvDest As Any, hpvSource As Any, ByVal cbCopy As Long)
Private Declare Function ChooseColor Lib "comdlg32.dll" Alias "ChooseColorA" (pChoosecolor As T_ChooseColor) As Long

Private Type T_ChooseColor
        lStructSize As Long
        hwndOwner As Long
        hInstance As Long
        RGBResult As Long
        lpCustColors As Long
        Flags As Long
        lCustData As Long
        lpfnHook As Long
        lpTemplateName As String
End Type

Private mCustomColors(0 To 15) As Long

Private Sub Form_Load()
    On Error GoTo EH
    Debug.Print "STEP1-begin"
    Dim t As T_ChooseColor
    t.lStructSize = Len(t)
    Debug.Print "STEP2-len=" & t.lStructSize
    Dim hMem As Long, addr As Long, sz As Long
    sz = Len(mCustomColors(0)) * 16
    Debug.Print "STEP2b-size=" & sz
    hMem = GlobalAlloc(&H42, sz)
    Debug.Print "STEP3-alloc=" & hMem
    If hMem = 0 Then End
    addr = GlobalLock(hMem)
    Debug.Print "STEP4-lock=" & addr
    If addr = 0 Then End
    CopyMemory ByVal addr, mCustomColors(0), sz
    Debug.Print "STEP5-copy-ok"
    t.lpCustColors = addr
    t.Flags = &H1   ' CC_RGBINIT, 无 hook 无 owner
    Debug.Print "STEP6-choosecolor-enter"
    Dim rc As Long
    rc = ChooseColor(t)
    Debug.Print "STEP7-ret=" & rc & " flags-after=" & t.Flags
    ' --- 类模块语境 (真工程形态): cDlg.ShowColor, mCustomColors 是类成员数组 ---
    Dim iDlg As New cDlg
    Debug.Print "STEP8-cls-showcolor-enter"
    iDlg.ShowColor
    Debug.Print "STEP9-cls-ret canceled=" & iDlg.Canceled & " color=" & iDlg.Color
    Debug.Print "STEP10-cls-showfont-enter"
    iDlg.ShowFont
    Debug.Print "STEP11-cls-font-ret canceled=" & iDlg.Canceled & " fontname=" & iDlg.FontName
    Debug.Print "STEP12-cls-showcolor2-enter"
    iDlg.ShowColor
    Debug.Print "STEP13-cls-ret2 canceled=" & iDlg.Canceled & " color=" & iDlg.Color
    End
EH:
    Debug.Print "ERR " & Err.Number
    End
End Sub
