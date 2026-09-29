' <vbeclipse>: Variant **装箱表本身**的哨兵夹具。
' 为什么单独有这一条: 装箱档位以前有两份并行表 (wrapVariantValue 的 switch 与
' boxToVariant), 账 #123 只在其中一份修了 Byte、Fix 198 只在另一份修了 Boolean,
' 于是"某一档对不对"取决于表达式走哪条路 —— DT43 (dt1.CheckBox 的 VarType) 就是在
' 一个和装箱毫无关系的控件应用上偶然暴露的。这里把三条真实路线各钉一遍:
'   (1) 内置函数实参装箱   VarType(m_b)        —— DT43 断的那条
'   (2) Variant 赋值装箱   v = m_b             —— wrapVariantValue 那条
'   (3) 过程形参装箱       ReportV ..., m_b    —— packLetValueArg 那条
' 以及模块级/局部/函数返回三种来源 (账 #123 的原始形态就是"模块级 Byte 错、局部对")。
' Date/Currency 两档也打出来但不进 needle: 见本批汇报 (C3 现在装箱走 VT_R8, VB6 是 7/6)。
Option Explicit

Private m_b As Boolean
Private m_by As Byte
Private m_i As Integer
Private m_l As Long
Private m_s As String
Private m_sin As Single
Private m_dbl As Double
Private m_d As Date

Private Sub ReportV(ByVal tag As String, ByVal v As Variant)
    Debug.Print tag; VarType(v); "/"; TypeName(v)
End Sub

Private Function RetB() As Boolean
    RetB = True
End Function

Public Sub Main()
    Dim lb As Boolean
    Dim lby As Byte
    lb = True
    lby = 67
    m_b = True
    m_by = 67
    m_i = 7
    m_l = 9
    m_s = "x"
    m_sin = 1.5
    m_dbl = 2.5
    m_d = CDate("2026-01-01")

    Debug.Print "VB-mod-bool="; VarType(m_b); "/"; TypeName(m_b)
    Debug.Print "VB-mod-byte="; VarType(m_by); "/"; TypeName(m_by)
    Debug.Print "VB-loc-bool="; VarType(lb); "/"; TypeName(lb)
    Debug.Print "VB-loc-byte="; VarType(lby); "/"; TypeName(lby)
    Debug.Print "VB-func-bool="; VarType(RetB()); "/"; TypeName(RetB())
    Debug.Print "VB-expr-bool="; VarType(lb And True)
    Debug.Print "VB-mod-long="; VarType(m_l)
    Debug.Print "VB-mod-int="; VarType(m_i)
    Debug.Print "VB-mod-str="; VarType(m_s)
    Debug.Print "VB-mod-sin="; VarType(m_sin)
    Debug.Print "VB-mod-dbl="; VarType(m_dbl)
    Debug.Print "VB-mod-date="; VarType(m_d)

    Dim v As Variant
    v = m_b
    Debug.Print "VB-asg-bool="; VarType(v); "/"; TypeName(v)
    v = m_by
    Debug.Print "VB-asg-byte="; VarType(v); "/"; TypeName(v)
    v = lb
    Debug.Print "VB-asg-loc-bool="; VarType(v)
    v = m_l
    Debug.Print "VB-asg-long="; VarType(v)
    v = m_d
    Debug.Print "VB-asg-date="; VarType(v)

    ReportV "VB-call-bool=", m_b
    ReportV "VB-call-byte=", m_by
    ReportV "VB-call-long=", m_l
    ReportV "VB-call-date=", m_d
    Debug.Print "VB-DONE"
End Sub
