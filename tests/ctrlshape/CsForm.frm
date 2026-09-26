VERSION 5.00
Begin VB.Form CsForm 
   Caption         =   "CsForm"
   ClientHeight    =   3200
   ClientLeft      =   120
   ClientTop       =   465
   ClientWidth     =   6000
   LinkTopic       =   "CsForm"
   ScaleHeight     =   3200
   ScaleWidth      =   6000
   Begin VB.Shape shpRect 
      BorderStyle     =   1
      BorderWidth     =   2
      Height          =   600
      Left            =   240
      Shape           =   0
      Top             =   240
      Width           =   900
   End
   Begin VB.Shape shpCircle 
      FillColor       =   255
      FillStyle       =   0
      Height          =   600
      Left            =   1440
      Shape           =   3
      Top             =   240
      Width           =   600
   End
   Begin VB.Line lnH 
      BorderStyle     =   2
      Height          =   0
      Width           =   0
      X1              =   0
      X2              =   3600
      Y1              =   1500
      Y2              =   1500
   End
   Begin VB.Line lnV 
      X1              =   600
      X2              =   600
      Y1              =   1800
      Y2              =   2400
   End
   Begin VB.Frame frmBox 
      Caption         =   "box"
      Height          =   1500
      Left            =   240
      TabIndex        =   5
      Top             =   1680
      Width           =   2400
      Begin VB.Shape shpInFrame 
         FillColor       =   65280
         FillStyle       =   0
         Height          =   300
         Left            =   240
         Shape           =   2
         Top             =   360
         Width           =   600
      End
      Begin VB.Line lnInFrame 
         X1              =   1200
         X2              =   1800
         Y1              =   240
         Y2              =   840
      End
   End
   Begin VB.Shape lamp 
      Index           =   0
      Height          =   240
      Left            =   3000
      Shape           =   3
      Top             =   240
      Width           =   240
   End
   Begin VB.Shape lamp 
      Index           =   1
      Height          =   240
      Left            =   3360
      Shape           =   3
      Top             =   240
      Width           =   240
   End
End
Attribute VB_Name = "CsForm"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit

' ai/029 C29-1a: Shape / Line 的"创建那一刀"接上后的判据。
' 改之前 controlTypeToWin32Class 对这两个类型返回 nullptr, 创建流程把控件当
' "不可见控件"跳过 —— 句柄永远是 NULL, 属性读写打在空窗口上, 屏幕上什么都没有。
' 几何读数 (Left/Top/Width/Height) 必须打到活句柄上才有值, 所以 CS1-CS4 就是
' "窗口真的建出来了"的证明, 不需要外部探针也不需要新声明 API。
' 单位口径: 四个端点与位置尺寸都是容器缇值; Line 的窗口矩形 = 端点包围盒。

Private Function TF(ByVal ok As Boolean) As String
    If ok Then TF = "Y" Else TF = "N"
End Function

Private Sub Form_Load()

    ' --- 1. 窗口真的建出来了 ---
    Debug.Print "CS1=" & TF(shpRect.Width = 900 And shpRect.Height = 600)
    Debug.Print "CS2=" & TF(shpCircle.Left = 1440 And shpCircle.Top = 240)
    Debug.Print "CS3=" & TF(lnH.Left = 0 And lnH.Top = 1500 And lnH.Width = 3600)
    Debug.Print "CS4=" & TF(lnV.Left = 600 And lnV.Top = 1800 And lnV.Height = 600)

    ' --- 2. 设计期整数属性落到位 ---
    Debug.Print "CS5=" & TF(shpRect.Shape = 0 And shpCircle.Shape = 3)
    Debug.Print "CS6=" & TF(shpCircle.FillStyle = 0 And shpCircle.FillColor = 255)
    Debug.Print "CS7=" & TF(shpRect.BorderWidth = 2 And lnH.BorderStyle = 2)

    ' --- 3. 运行期读写回路 (空句柄时 setter 什么都不做、getter 回默认值) ---
    shpRect.Shape = 2
    Debug.Print "CS8=" & TF(shpRect.Shape = 2)
    shpRect.FillColor = 16711680
    Debug.Print "CS9=" & TF(shpRect.FillColor = 16711680)
    lnV.BorderColor = 255
    Debug.Print "CS10=" & TF(lnV.BorderColor = 255)

    ' --- 4. Line 改端点要连窗口一起搬 ---
    lnH.X1 = 600
    Debug.Print "CS11=" & TF(lnH.X1 = 600 And lnH.Left = 600 And lnH.Width = 3000)
    lnH.X2 = 6000
    Debug.Print "CS12=" & TF(lnH.Left = 600 And lnH.Width = 5400)
    lnH.Y2 = 2400
    Debug.Print "CS13=" & TF(lnH.Top = 1500 And lnH.Y2 = 2400)
    lnV.Y1 = 1200
    Debug.Print "CS14=" & TF(lnV.Top = 1200 And lnV.Height = 1200)

    ' --- 5. 手册那条规则: 笔宽不是 1 时, 虚线族 (2..5) 被强制回 1=Solid ---
    shpRect.BorderStyle = 2
    shpRect.BorderWidth = 3
    Debug.Print "CS15=" & TF(shpRect.BorderStyle = 1)
    shpRect.BorderWidth = 1
    shpRect.BorderStyle = 3
    Debug.Print "CS16=" & TF(shpRect.BorderStyle = 3 And shpRect.BorderWidth = 1)

    ' --- 6. 容器 (Frame) 里的 Shape/Line —— 那是另一条创建路, 以前一条属性都不发 ---
    Debug.Print "CS17=" & TF(shpInFrame.Shape = 2 And shpInFrame.FillColor = 65280)
    Debug.Print "CS18=" & TF(shpInFrame.Left = 240 And shpInFrame.Width = 600)
    Debug.Print "CS19=" & TF(lnInFrame.Left = 1200 And lnInFrame.Top = 240 And lnInFrame.Width = 600 And lnInFrame.Height = 600)

    ' --- 7. 控件数组 (指示灯那种用法): 设计期与运行期都按槽位走 ---
    Debug.Print "CS20=" & TF(lamp(0).Shape = 3 And lamp(1).Left = 3360)
    lamp(1).FillColor = 255
    Debug.Print "CS21=" & TF(lamp(1).FillColor = 255 And lamp(0).FillColor <> 255)

    Debug.Print "CTRLSHAPE-DONE"
    Unload Me
End Sub
