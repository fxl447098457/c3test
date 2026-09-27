VERSION 5.00
Begin VB.Form PropForm 
   Caption         =   "PropForm"
   ClientHeight    =   3200
   ClientLeft      =   120
   ClientTop       =   465
   ClientWidth     =   6000
   LinkTopic       =   "PropForm"
   ScaleHeight     =   3200
   ScaleWidth      =   6000
   Begin VB.TreeView tv1 
      Height          =   1500
      Left            =   240
      TabIndex        =   0
      Top             =   240
      Width           =   2100
   End
   Begin VB.Timer tmr 
      Interval        =   200
      Left            =   240
      Top             =   1920
   End
End
Attribute VB_Name = "PropForm"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit

' ai/029:429 那条"未登记的控件属性按数值读会漏裸指针"的定靶夹具 (Fix 161d)。
'
' 形态来源:
'   ① Select Case —— cgen_select.cpp 用 resolveComValue() **无参**(默认 "BSTR"),
'      而 tempType 对非字符串测试是 int32_t ⇒ `int32_t v = (wchar_t*)ptr`。
'   ② Not —— cgen_expr.cpp 的 UnaryOp::Not 落 `(int32_t)(operand)`; Fix 092r 当时
'      只补了 Negate(取负), Not 这刀漏了。真实工程命中: Charts 2020 的
'      ppProgressCircular.pag:536 `If Not oPC.ShowAnimation Then`。
'   同族的另两处早已修过: Fix 092n(For 的 start/end/step)、Fix 092r(Negate)。
'
' 只有**未登记**属性中招: 已登记的走 getControlPropReadFn 的专属 getter (返回 int32_t),
' 根本不进 isComMarker_ 那个兜底分支。
'
' ⚠ 靶子为什么选 Caption 而不是 Style:
'   判据要能红, 前提是基线上那个 `(int32_t)(指针)` 截出来的值**非 0**。
'   - Style: 宿主侧答不出 (vb6_Host_GetProp 落"未知属性→Empty") ⇒ StringProp 返回 NULL
'     ⇒ 截断成 0 ⇒ 与修复后同值, **判据假绿** (实测踩过)。
'   - Caption: 宿主侧**答得出值** (走 GetWindowTextW, vb6_ho_setVariantBstr 给真 BSTR,
'     非 NULL) ⇒ 截出来是指针低位(非 0) ⇒ Case 0 不命中 ⇒ 能红。
'   TreeView 的 Caption 未登记 (读表只登记 5 条标量 + Visible/Enabled)。

Private Function TF(ByVal ok As Boolean) As String
    If ok Then TF = "Y" Else TF = "N"
End Function

Private Sub tmr_Timer()
    Static done As Integer
    If done Then Exit Sub
    done = 1

    Dim v As Long

    ' 诊断: 未登记属性按 Long 读 (这条上下文一直是对的, 两种编译器都给 0)。
    v = tv1.Caption
    Debug.Print "P-VAL=" & v

    ' ① Select Case + 未登记属性 (数值语义)。
    ' 修复前: int32_t temp = (wchar_t*)真BSTR指针 ⇒ temp 是指针低位(非 0) ⇒ Case 0 不命中。
    ' 修复后: ComGetIntProp ⇒ 空串转数值 = 0 ⇒ Case 0 命中。
    Select Case tv1.Caption
        Case 0
            Debug.Print "P-SEL0=Y"
        Case Else
            Debug.Print "P-SEL0=N"
    End Select

    ' ② Not + 未登记属性 —— 断言 **Not 的结果值**, 不是"真假"。
    '    ⚠ 判据为什么不能写成 `If Not x Then`: 基线的 `~(int32_t)(指针低位)` 结果
    '    几乎总是非 0 ⇒ 也是"真" ⇒ 两种编译器都进 Then, **判据不红**(实测踩过)。
    '    VB6 语义: Not 0 = -1 (而 Not -1 = 0)。未登记属性修复后读回 0 ⇒ r 必须是 -1。
    '    基线: r = ~(指针低位) ⇒ 非 -1。
    Dim r As Long
    r = Not tv1.Caption
    Debug.Print "P-NOTVAL=" & r
    If r = -1 Then
        Debug.Print "P-NOT=Y"
    Else
        Debug.Print "P-NOT=N"
    End If

    ' ③ 对照: **已登记**属性的同一批形态 —— 它们一直是对的, 用来证明
    '    本修复没有把已登记路径带歪 (CheckBoxes 走 vb6_TreeView_GetCheckBoxes)。
    '    设计期没写过 CheckBoxes ⇒ VB6 默认 False(0)。故: Case True 不命中、Not False 为真。
    Select Case tv1.CheckBoxes
        Case False
            Debug.Print "P-REG-SEL=Y"
        Case Else
            Debug.Print "P-REG-SEL=N"
    End Select
    If Not tv1.CheckBoxes Then
        Debug.Print "P-REG-NOT=Y"
    Else
        Debug.Print "P-REG-NOT=N"
    End If

    ' ④ 对照: 通用 Long 属性 (Left 经 controlPropType 登记成 Long)。
    Select Case tv1.Left
        Case Is > 0
            Debug.Print "P-GEN-SEL=Y"
        Case Else
            Debug.Print "P-GEN-SEL=N"
    End Select

    Debug.Print "CTRLPROP-DONE"
    Unload Me
End Sub
