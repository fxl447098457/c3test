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
    ' 账 #305 (§B141 + §B142): CDec 造出来的那一档 (VT_DECIMAL=14) 在「往外读」这一侧的判据。
    ' §B141 补的是两张表的格子(TypeName / IsNumeric), 第三十四刀补的是**换算本体与写侧的一枚标签**,
    ' 于是 CStr / CLng / CDbl 一起对上。两处根因都是「格式」不是「数值」:
    ' ① DECIMAL.wReserved 其实是 oleaut32 自己的 vt 标签(它写 14), 手填字段那两路以前留 0 ——
    '    数据全对而 OLE 不认, VariantChangeType 返回 S_OK 却交 0 / 交空串;
    ' ② x86 的 VARIANT 装 VT_DECIMAL 不走 pdecVal 指针, 是整枚 16 字节 DECIMAL 内联盖住 VARIANT
    '    —— 只填指针时 oleaut32 把那枚地址当数值答出来(此前那个「堆地址」读数就是这一条)。
    ' Format 那条读数照打但不进针面: 带小数点的格式串随区域设置变(与 AR-fmt2 同口径)。
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
    Debug.Print "VB-dec-clng="; CLng(vd2)
    Debug.Print "VB-dec-cdbl="; CDbl(vd3)
    ' Acct #305 para-B142 cell 2 (the float arm's tag). Comments here stay ASCII: C3 reads
    ' sources as ANSI/GBK, so UTF-8 Chinese lines round-trip as mojibake in the product.
    ' CDec(2.5) takes the VarDecFromR8 arm, and that DECIMAL comes back with wReserved = 0
    ' (only VarDecFromStr writes 14). Knife 34 tagged just the two hand-filled arms, so on
    ' x64 CStr(CDec(2.5)) printed empty and CDbl gave 0, while x86 was saved by the inline
    ' overlay on the read side -- an arch asymmetry. Deliberately NOT pinning
    ' CLng(CDec(2.5)): oleaut32 rounds 2.5 to 2 (banker's, = VB6), we currently answer 3
    ' (it goes through the double converter), and that half-way caliber is still unowned.
    Dim vd4 As Variant
    vd4 = CDec(2.5)
    Debug.Print "VB-decf-tn="; TypeName(vd4)
    Debug.Print "VB-decf-cstr=[" & CStr(vd4) & "]"
    Debug.Print "VB-decf-cdbl="; CDbl(vd4)
    Debug.Print "VB-DONE"
End Sub
