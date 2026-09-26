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
' Nodes / Style / LabelEdit / Sorted 与事件不在本批 (8a)：Nodes/Node 那一大半在 8b 接上
' (TV13..TV27)，Style / LabelEdit / Sorted / NodeClick 还欠着。

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

    ' ============================================================
    ' --- 7. C29-8b: Nodes 集合与 Node 对象 (真 IDispatch, 见 vb6forms_memberobj.c) ---
    '     结构 (父子/兄弟) 现问原生树，字符串住这张表 —— 两边都从同一个窗口出发,
    '     所以"读数对"就等于"屏幕上对"。集合序 = 插入序 (029 §九 记的口径)。
    Dim ndA As Object, ndB As Object, ndC As Object
    Set ndA = tv1.Nodes.Add(, , "a", "根甲")
    Set ndB = tv1.Nodes.Add("a", 4, "b", "子乙")      ' relative 按 Key, 4 = tvwChild
    Set ndC = tv1.Nodes.Add(, , "c", "根丙")

    ' --- 8. Count / Add 返回的对象带 Index ---
    Debug.Print "TV13=" & TF(tv1.Nodes.Count = 3)
    Debug.Print "TV14=" & TF(ndA.Index = 1 And ndB.Index = 2 And ndC.Index = 3)

    ' --- 9. 下标取与按 Key 取 (同一条 Item 两种实参形态) ---
    '     字符串成员先取进对象变量再读: 链式 `Nodes(2).Text` 这种"比较左值是链上
    '     字符串成员"的形态, cgen 现在会按数值解包 (vb6_ComGetIntProp) —— 那是
    '     029 台账 #85 同族的独立缺陷, 不在本批范围, 别在判据里踩它。
    Dim ndIx As Object, ndKy As Object
    Set ndIx = tv1.Nodes(2)
    Set ndKy = tv1.Nodes("b")
    Dim sIx As String
    sIx = ndIx.Text
    Debug.Print "TV15=" & TF(sIx = "子乙" And ndKy.Index = 2)

    ' --- 10. 导航读数全问原生树 ---
    Debug.Print "TV16=" & TF(ndA.Child = 2 And ndA.Children = 1)
    Debug.Print "TV17=" & TF(ndB.Parent = 1 And ndB.Next = 0 And ndC.Previous = 1)
    Debug.Print "TV18=" & TF(ndB.Root = 1 And ndC.Root = 3)

    ' --- 11. 写回去：Text 改完两边同步 (表 + TVM_SETITEMW) ---
    ndA.Text = "改名甲"
    ndA.Tag = "标签甲"
    Dim ndReread As Object
    Dim sRe As String, tRe As String
    Set ndReread = tv1.Nodes(1)          ' 换一枚对象再读: 证的是**控件侧**的状态, 不是变量里存的串
    sRe = ndReread.Text
    tRe = ndReread.Tag
    Debug.Print "TV19=" & TF(sRe = "改名甲" And tRe = "标签甲")

    ' --- 12. Checked 住在原生 state image 里 (tv1 设计期就 CheckBoxes=True) ---
    ndA.Checked = True
    ndC.Checked = False
    Debug.Print "TV20=" & TF((ndA.Checked <> 0) And (ndC.Checked = 0))

    ' --- 13. Expanded: 先关后开, 读数问 TVIS_EXPANDED ---
    ndA.Expanded = True
    Debug.Print "TV21=" & TF(ndA.Expanded <> 0)
    ndA.Expanded = False
    Debug.Print "TV22=" & TF(ndA.Expanded = 0)

    ' --- 14. EnsureVisible 把父节点撑开 (TVM_ENSUREVISIBLE 的真行为, 不是表里的位) ---
    ndB.EnsureVisible
    Debug.Print "TV23=" & TF(ndA.Expanded <> 0)

    ' --- 15. For Each 走 _NewEnum, 顺序与集合序同 ---
    Dim e As Object
    Dim acc As String
    acc = ""
    For Each e In tv1.Nodes
        acc = acc & e.Index & ":" & e.Text & "/"
    Next
    Debug.Print "TV24=" & TF(acc = "1:改名甲/2:子乙/3:根丙/")

    ' --- 16. Remove 按 Key 删的是**整棵子树** (原生删父带走子, 表要跟住) ---
    tv1.Nodes.Remove "a"
    Dim ndLeft As Object
    Dim kLeft As String
    Set ndLeft = tv1.Nodes(1)
    kLeft = ndLeft.Key
    Debug.Print "TV25=" & TF(tv1.Nodes.Count = 1 And kLeft = "c")

    ' --- 17. Clear 清表 ---
    tv1.Nodes.Clear
    Debug.Print "TV26=" & TF(tv1.Nodes.Count = 0)

    ' --- 18. 集合是**独立于控件属性面**的一条路: 标量读数没被 Nodes 抢走 ---
    Debug.Print "TV27=" & TF(tv1.CheckBoxes <> 0 And tv2.Nodes.Count = 0)

    Debug.Print "TREEVIEW-DONE"
    Unload Me
End Sub
