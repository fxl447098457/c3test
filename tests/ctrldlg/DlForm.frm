VERSION 5.00
Begin VB.Form DlForm 
   Caption         =   "DlForm"
   ClientHeight    =   3200
   ClientLeft      =   120
   ClientTop       =   465
   ClientWidth     =   6000
   LinkTopic       =   "DlForm"
   ScaleHeight     =   3200
   ScaleWidth      =   6000
   Begin MSComDlg.CommonDialog dl1 
      Left            =   240
      Top             =   240
      Height          =   375
      Width           =   375
      CancelError     =   -1   'True
      DialogTitle     =   "挑一个文件"
      Filter          =   "文本 (*.txt)|*.txt|所有文件 (*.*)|*.*"
      Flags           =   528
      InitDir         =   "C:\Windows"
      DefaultExt      =   "txt"
   End
   Begin MSComDlg.CommonDialog dl2 
      Left            =   720
      Top             =   240
      Height          =   375
      Width           =   375
   End
End
Attribute VB_Name = "DlForm"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit

' ai/029 C29-9 / 决策 D6：CommonDialog 走原生 comdlg32，**不再经 MSComDlg.OCX**。
' 为什么要换（实测）：那个 OCX 只有 32 位，x64 进程里 CoCreateInstance 直接失败，
' 于是改之前这枚控件是**静默空转**的 —— 属性读回全空、六个 Show* 一个都不出现，
' 而程序照旧打完尾针、退出码 0。
' 实现口径：属性袋挂在一枚自注册的不可见子窗口（VB6_COMMONDIALOG）上，六个 Show*
' 直接调 comdlg32（LoadLibrary + GetProcAddress，不新增 import lib）。
' 这里刻意**不弹真框**（弹框要另一套"起窗 + 自关"的判据，见 029 三 D3），
' 断的是：设计期初值落位 / 属性读写回路 / 两枚互不串 / 取消出口按 CancelError 报 32755。

' ai/029 C29-9b: 弹框那条判据要一个"过了多久"的量尺 —— 探针是自关线程, 模态循环真跑过
' 才可能烧掉几十毫秒; GetTickCount 走 Declare, 不引新 import lib。
Declare Function GetTickCount Lib "kernel32" () As Long

Private Function TF(ByVal ok As Boolean) As String
    If ok Then TF = "Y" Else TF = "N"
End Function

Private Sub Form_Load()
    On Error Resume Next

    ' --- 1. 设计期四行真的落到控件上（改之前这四行**一行都没落**） ---
    Debug.Print "DL1=" & TF(dl1.DialogTitle = "挑一个文件")
    Debug.Print "DL2=" & TF(dl1.Flags = 528)
    Debug.Print "DL3=" & TF(dl1.CancelError = True)
    Debug.Print "DL4=" & TF(dl1.InitDir = "C:\Windows" And dl1.DefaultExt = "txt")

    ' --- 5. Filter 原样读回（VB6 的读数口径是竖线串，不是原生那套 \0 表） ---
    Debug.Print "DL5=" & TF(dl1.Filter = "文本 (*.txt)|*.txt|所有文件 (*.*)|*.*")

    ' --- 6. 没写初值的那一枚 = 全默认，且**不继承**上一枚 ---
    Debug.Print "DL6=" & TF(dl2.Filter = "" And dl2.Flags = 0 And dl2.CancelError = False)
    Debug.Print "DL7=" & TF(dl2.DialogTitle = "")

    ' --- 8. 运行期读写回路：字符串与数值两类属性都走同一枚句柄 ---
    dl2.Filter = "图片|*.png"
    dl2.Color = 255
    dl2.FontName = "Consolas"
    dl2.FontSize = 12
    dl2.Max = 999
    Debug.Print "DL8=" & TF(dl2.Filter = "图片|*.png" And dl2.Color = 255 _
                            And dl2.FontName = "Consolas" And dl2.FontSize = 12 And dl2.Max = 999)

    ' --- 9. 两枚不串：改 dl2 不动 dl1 ---
    Debug.Print "DL9=" & TF(dl1.Color = 0 And dl1.Flags = 528)

    ' --- 10. 六个 Show* 的发码形状：落到原生入口，运行期不弹 ---
    '     "对话框确实出现 + 取消报 32755" 要一套起窗自关的探针（下一小批 C29-9b）；
    '     这里用恒假守卫把调用留在源码里 —— 发码看得见，又不会在无人点确定时把门卡死。
    Dim noPop As Long
    noPop = 0
    If noPop = 1 Then dl1.ShowOpen
    If noPop = 1 Then dl1.ShowSave
    If noPop = 1 Then dl1.ShowColor
    If noPop = 1 Then dl1.ShowFont
    If noPop = 1 Then dl1.ShowPrinter
    If noPop = 1 Then dl1.ShowAbout
    Debug.Print "DL10=" & TF(noPop = 0)

    ' --- 11..14 C29-9b: 真弹框 + 起窗自关探针。只在 C3_CDPROBE=1 时走这条路 —— 不设
    '     环境变量时这四行**根本不打印**，所以 C29-9 那 10 条读数的形状与顺序逐字不变。
    '     探针在 RTL 侧(vb6forms_ctrl.c)：一次性线程只认本线程创建的 #32770，发
    '     WM_COMMAND/IDCANCEL，等价于"用户点了取消"。于是断点就是取消出口本身 ——
    '     框没出现 ⇒ 没人取消 ⇒ 32755 不会报。
    Dim probe As Long
    probe = 0
    If Environ("C3_CDPROBE") = "1" Then probe = 1

    Dim dp As Long, el As Long, t0 As Long, silent As Long
    dp = 0
    el = 0
    silent = 0
    If probe = 1 Then
        dl2.FileName = "C:\keep.txt"           ' 取消不该动已有读数
        t0 = GetTickCount()
        dl1.ShowOpen                           ' dl1 的 CancelError 设计期就是 True
        If Err.Number = 32755 Then dp = 1
        el = GetTickCount() - t0
        Err.Clear
        Debug.Print "DL11=" & TF(dp = 1)                         ' 取消出口真触发
        Debug.Print "DL12=" & TF(dl2.FileName = "C:\keep.txt")  ' 取消不改进数
        Debug.Print "DL13=" & TF(el >= 30)                       ' 模态循环真跑过

        dl2.CancelError = False                ' 这一枚不报 CancelError ⇒ 静默返回
        dl2.ShowSave
        If Err.Number = 0 Then silent = 1
        Err.Clear
        Debug.Print "DL14=" & TF(silent = 1)
    End If
    Debug.Print "CTRLDLG-DONE"
    Unload Me
End Sub
