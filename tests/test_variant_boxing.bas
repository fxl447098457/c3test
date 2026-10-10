' <vbeclipse>: Variant **装箱表本身**的哨兵夹具。
' 为什么单独有这一条: 装箱档位以前有两份并行表 (wrapVariantValue 的 switch 与
' boxToVariant), 账 #123 只在其中一份修了 Byte、Fix 198 只在另一份修了 Boolean,
' 于是"某一档对不对"取决于表达式走哪条路 —— DT43 (dt1.CheckBox 的 VarType) 就是在
' 一个和装箱毫无关系的控件应用上偶然暴露的。这里把三条真实路线各钉一遍:
'   (1) 内置函数实参装箱   VarType(m_b)        —— DT43 断的那条
'   (2) Variant 赋值装箱   v = m_b             —— wrapVariantValue 那条
'   (3) 过程形参装箱       ReportV ..., m_b    —— packLetValueArg 那条
' 以及模块级/局部/函数返回三种来源 (账 #123 的原始形态就是"模块级 Byte 错、局部对")。
' Date/Single 两档本批已修进权威表也进了 needle (修前两者都读成 5); Currency 仍只打印
' 不钉 —— 它与 VB6 的 VT_CY=6 的差距没有真 VB6 读数可依据, 已在汇报里记成待办。
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
    ' Date 用序列号给值: CDate("2026-01-01") 在本仓现在是 0 (日期串解析是另一格问题,
    ' 已单独记), 那会让本夹具的 clng/cdbl 两行读成 0, 分不清是装箱还是解析。
    m_d = 46023

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

    ' 新档位的**反向**也要钉: 装成 VT_DATE / VT_R4 之后, 数值提取端与 IsDate 还得答对
    ' (RTL 里 vb6_vtDate 早有 TypeName/Format 档, 缺的是 VariantToLong/ToDouble 那一侧)。
    Dim vd As Variant
    vd = m_d
    Dim vs As Variant
    vs = m_sin
    Debug.Print "VB-date-clng="; CLng(vd)
    Debug.Print "VB-date-cdbl="; CDbl(vd)
    Debug.Print "VB-date-isdate="; IsDate(vd)
    Debug.Print "VB-date-cdate="; (CDate(vd) = m_d)
    Debug.Print "VB-sin-cdbl="; CDbl(vs)
    Debug.Print "VB-sin-clng="; CLng(vs)
    ' 账 #305 (§B138 尾巴): §B138 把 VarPtr/StrPtr/ObjPtr 的类型答案换成 LongPtr 之后, 同一枚地址
    ' 在 x64 装成 VT_I8(20)、在 x86 装成 VT_I4(3)。VarType 因此**不能钉**(它本来就该随位数变);
    ' 钉的是那四问在两台机器上必须给同一个答案 —— 改之前 x64 读出来是 Variant / False。
    ' 刻意不钉 CLng(vp): x64 那个地址放不下 Long, 报 6 (Overflow) 才是 VB6 的正确答案。
    Dim vp As Variant
    Dim vs2 As Variant
    Dim hx As Long
    hx = 7
    vp = VarPtr(hx)
    vs2 = StrPtr("box")
    Debug.Print "VB-ptr-tn="; TypeName(vp)
    Debug.Print "VB-ptr-isnum="; IsNumeric(vp)
    Debug.Print "VB-ptr-cbool="; CBool(vp)
    Debug.Print "VB-ptr-selfeq="; (vp = vp)
    Debug.Print "VB-str-tn="; TypeName(vs2)
    ' 账 #305 (§B141): CDec 造出来的那一档 (VT_DECIMAL=14) 从来没人在「往外读」这一侧答过 ——
    ' 两张表原先都落进 default: vb6_TypeName 答 "Variant"、vb6_IsNumeric 答假, 而且两架构同错
    ' (这一档不像 VT_I8, 它不是位数问题, 是那张表根本没有这一格)。本刀只补这两张表。
    ' 只钉两条: CStr/Format 那两条读数照打但**不进针面** —— 今天两台都打空(vb6_Format 里那一
    ' 格没补); 把读侧那臂补上之后实测 x64 对 CDec(整数) 交 0、x86 把 DECIMAL 读成堆地址
    ' ⇒ 罪在写侧/所有权, 另立 §B142(候选臂与四台读数留在 .build/b990_format_arm_candidate.inc)
    ' 刻意不钉 CDec 的算术/比较面: 那要另一族出口 (VarDec* 一族), 与本格的「读出来」无关。
    Dim vd2 As Variant
    Dim vd3 As Variant
    vd2 = CDec(5)
    vd3 = CDec("1.25")
    Debug.Print "VB-dec-tn="; TypeName(vd2)
    Debug.Print "VB-dec-cstr=[" & CStr(vd2) & "]"
    Debug.Print "VB-dec2-cstr=[" & CStr(vd3) & "]"
    Debug.Print "VB-dec-isnum="; IsNumeric(vd2)
    Debug.Print "VB-dec-fmt=[" & Format(vd2, "0.00") & "]"
    Debug.Print "VB-DONE"
End Sub
