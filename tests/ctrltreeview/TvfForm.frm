VERSION 5.00
Begin VB.Form TvfForm 
   Caption         =   "TvfForm"
   ClientHeight    =   3200
   ClientLeft      =   120
   ClientTop       =   465
   ClientWidth     =   6000
   LinkTopic       =   "TvfForm"
   ScaleHeight     =   3200
   ScaleWidth      =   6000
   Begin VB.TreeView tv1 
      CheckBoxes      =   -1  'True
      Height          =   1500
      HideSelection   =   0   'False
      HotTracking     =   -1  'True
      Indentation     =   300
      Left            =   240
      LineStyle       =   1   'RootLines
      TabIndex        =   0
      Top             =   240
      Width           =   2100
   End
   Begin VB.TreeView tv2 
      Height          =   1500
      Left            =   2640
      TabIndex        =   1
      Top             =   240
      Width           =   2100
   End
End
Attribute VB_Name = "TvfForm"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit

' ai/029 C29-8a: TreeView 的标量属性面走原生 SysTreeView32（不加载 MSCOMCTL.OCX）。
' 改之前这枚控件的**窗口本来就建得出来**（controlTypeToWin32Class 早就有格），缺的是属性面：
' cgen 的读写表里 TreeView 零格 ⇒ 属性一律落通用兜底 vb6_ComGetStringProp(裸 HWND, "…")，
' 实测读回空串、写进去静默丢（029 §九 那四条前置读数）。本批补五条：
'   LineStyle / Indentation / CheckBoxes / HotTracking / HideSelection
' 判据口径：这四条样式类的属性**真值就是窗口样式位**（RTL getter 读 GWL_STYLE），所以
' "设计期初值落位"、"运行期可逆赋值"、"默认值"三类读数都问的是同一个窗口；
' Indentation 的默认那条（TV7）问的是 TVM_GETINDENT —— 句柄为空只会读到 0，读到正数
' 就是"消息真打进了控件"，与本线"窗口存在性靠读数证"的口径一致。
' Nodes / Style / LabelEdit / Sorted / 事件不在本批：等 C29-3 那套成员对象机制。

Private Function TF(ByVal ok As Boolean) As String
    If ok Then TF = "Y" Else TF = "N"
End Function

Private Sub Form_Load()

    ' --- 1. 设计期五值落位 (tv1 的 .frm 里全写过) ---
    Debug.Print "TV1=" & TF(tv1.CheckBoxes <> 0)
    Debug.Print "TV2=" & TF(tv1.HotTracking <> 0)
    Debug.Print "TV3=" & TF(tv1.LineStyle = 1)
    Debug.Print "TV4=" & TF(tv1.Indentation = 300)
    Debug.Print "TV5=" & TF(tv1.HideSelection = 0)

    ' --- 2. 没写过的控件走 VB6 默认 (复选框/跟踪关、根层不画线、失焦藏选中) ---
    Debug.Print "TV6=" & TF(tv2.CheckBoxes = 0 And tv2.HotTracking = 0 _
                            And tv2.LineStyle = 0 And tv2.HideSelection <> 0)

    ' --- 3. 默认缩进是从真窗口问出来的 (TVM_GETINDENT) ---
    Debug.Print "TV7=" & TF(tv2.Indentation > 0)

    ' --- 4. 运行期赋值 + 反向可逆 ---
    tv2.CheckBoxes = True
    tv2.LineStyle = 1
    tv2.HotTracking = True
    Debug.Print "TV8=" & TF(tv2.CheckBoxes <> 0 And tv2.LineStyle = 1 And tv2.HotTracking <> 0)
    tv2.CheckBoxes = False
    tv2.LineStyle = 0
    tv2.HotTracking = False
    tv2.HideSelection = False
    Debug.Print "TV9=" & TF(tv2.CheckBoxes = 0 And tv2.LineStyle = 0 _
                            And tv2.HotTracking = 0 And tv2.HideSelection = 0)

    ' --- 5. Indentation 的 VB6 侧口径是缇: 往返要原样读回, 超界也不回吐像素值 ---
    tv2.Indentation = 400
    Debug.Print "TV10=" & TF(tv2.Indentation = 400)
    tv2.Indentation = 100000
    Debug.Print "TV11=" & TF(tv2.Indentation = 100000)

    ' --- 6. 通用属性面没被这五条抢走 ---
    tv1.Visible = False
    Debug.Print "TV12=" & TF(tv1.Visible = False And tv1.Height = 1500)

    Debug.Print "TREEVIEW-DONE"
    Unload Me
End Sub
