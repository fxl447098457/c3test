VERSION 5.00
Begin VB.Form MfForm 
   Caption         =   "MfForm"
   ClientHeight    =   2200
   ClientLeft      =   120
   ClientTop       =   465
   ClientWidth     =   4800
   LinkTopic       =   "MfForm"
   ScaleHeight     =   2200
   ScaleWidth      =   4800
   Begin VB.Toolbar tb1 
      Height          =   300
      Left            =   240
      TabIndex        =   0
      Top             =   240
      Width           =   3600
      Buttons(1)      =   "One"
         .Key        =   "one"
         .Style      =   0
      End
   End
End
Attribute VB_Name = "MfForm"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit

' ai/029 C29-M 的夹具：工程自带一份 .res（ResFile32="no_manifest.res"，里面只有一条对话框
' 模板，**没有** RT_MANIFEST）。旧判据只看"给没给 ResFile"，于是这种 VB6 里很常见的工程连
' 内置那份 comctl v6 清单一起让掉 ⇒ 产物静默退回 v5.82，而编译、链接、退出码全都好看。
'
' 主判据在**产物资源面**：run_tests.ps1 的 Test-ProductManifest 数产物里有几份清单、核里面
' 是哪一份（改后 = 1 份、内置那份；BASE 编译器编同一件夹具 = **0 份**，实测翻红）。
' 本夹具只钉"清单这件事没把控件面带歪"。
'
' 记两条试过的运行期读法，都不通（细节与结论在 ai/029 §九 的 C29-M 那格，两条各立了账）：
'   · `Declare Function DllGetVersion Lib "comctl32.dll" (ByRef pdwVer As Long) As Long`
'     回来 hr=0x80070057 (E_INVALIDARG) 且出参没被写；
'   · 换 `GetModuleHandleA` + `GetModuleFileNameA`（`ByVal buf As String` 当缓冲）问"进程实际
'     加载的是哪一份 comctl32"，产物直接崩；而 tests/test_declare.bas 里 GetUserName 用的正是
'     这个形态且是绿的 ⇒ 差异没定位，不在这一格里追。

Private Function TF(ByVal ok As Boolean) As String
    If ok Then TF = "Y" Else TF = "N"
End Function

Private Sub Form_Load()
    Debug.Print "CM1=" & TF(tb1.Buttons.Count = 1)
    Debug.Print "CM2=" & TF(tb1.ShowTips <> 0)
    Debug.Print "CM3=" & TF(tb1.Width = 3600 And tb1.Height = 300)

    Debug.Print "CTRLMANIFEST-DONE"
    Unload Me
End Sub
