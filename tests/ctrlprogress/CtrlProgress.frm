VERSION 5.00
Begin VB.Form CtrlProgress
   Caption         =   "CtrlProgress"
   ClientHeight    =   3000
   ClientLeft      =   120
   ClientTop       =   465
   ClientWidth     =   6000
   LinkTopic       =   "Form1"
   ScaleHeight     =   3000
   ScaleWidth      =   6000
   StartUpPosition =   3  '窗口缺省
   Begin MSComctlLib.ProgressBar ProgressBar1
      Height          =   195
      Left            =   240
      TabIndex        =   0
      Top             =   240
      Width           =   3000
      _ExtentX        =   5297
      _ExtentY        =   325
      Max             =   100
      Min             =   0
      Value           =   -10             ' 低于 Min, 钳位
      Scrolling       =   1               ' ccScrollingStandard
   End
   Begin MSComctlLib.ProgressBar ProgressBar2
      Height          =   195
      Left            =   240
      TabIndex        =   1
      Top             =   660
      Width           =   3000
      _ExtentX        =   5297
      _ExtentY        =   325
      Max             =   10
      Min             =   -10
      Orientation     =   1               ' ccOrientationVertical
      Scrolling       =   0               ' ccScrollingSmooth
   End
End
Attribute VB_Name = "CtrlProgress"
Option Explicit

' P20-38: ProgressBar 复刻 (msctls_progress32, 不加载 mscomctl.ocx)。
' PB11 专门盯一个易漏的坑: `Scrolling = 0` 是合法的显式赋值, 若 RTL 用
' SetPropW 直存值, 存 0 得到的句柄是 NULL, 读时会被当成"未设置"回落到默认 1。

Private Sub Form_Load()
    Debug.Print "PB1-MAX=" & ProgressBar1.Max
    Debug.Print "PB2-MIN=" & ProgressBar1.Min
    Debug.Print "PB3-VALUE=" & ProgressBar1.Value
    Debug.Print "PB4-SCROLLSTD=" & ProgressBar1.Scrolling
    Debug.Print "PB5-ORIENTH=" & ProgressBar1.Orientation
    ProgressBar1.Value = 40
    Debug.Print "PB6-SET40=" & ProgressBar1.Value
    ProgressBar1.Max = 200
    Debug.Print "PB7-MAX200=" & ProgressBar1.Max
    Debug.Print "PB8-D2MAX=" & ProgressBar2.Max
    Debug.Print "PB9-D2MIN=" & ProgressBar2.Min
    Debug.Print "PB10-D2ORI=" & ProgressBar2.Orientation
    Debug.Print "PB11-D2SCR=" & ProgressBar2.Scrolling
    Debug.Print "CTRLPROGRESS-DONE"
    Unload Me
End Sub
