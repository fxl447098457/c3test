Attribute VB_Name = "ArrClsMain"
Option Explicit

' Fix 192: 元素类型为**项目类**的数组 —— 六种形状的成员访问。
'   坏的时候全部落在同一处: 接收者 arr(i) 推断不出类 → 通用成员访问发
'   `VB6_SA_AT(void*, arr, i).Move(...)` → C2224 "左侧必须具有结构/联合类型"。
'   修法 = 登记元素类名 (arrayClassElemTypes_) + inferClassTypeOfExpr 认它。
' 注意: 缺口**不是 ReDim 专属** —— AC1/AC2 是静态数组, 一样坏。

Private m_items() As ArrClsShape

Sub Main()
    ' AC1: 静态数组 Dim s(1) As C
    Dim s(1) As ArrClsShape
    Set s(0) = New ArrClsShape
    s(0).Move 2
    If s(0).Side() = 3# Then
        Debug.Print "AC1:OK"
    Else
        Debug.Print "AC1:FAIL side=" & s(0).Side()
    End If

    ' AC2: 静态数组 + 变量下标 + 非 0 下界
    Dim t(0 To 2) As ArrClsShape
    Dim i As Long
    i = 1
    Set t(i) = New ArrClsShape
    t(i).Move 4
    If t(i).Side() = 5# Then
        Debug.Print "AC2:OK"
    Else
        Debug.Print "AC2:FAIL side=" & t(i).Side()
    End If

    ' AC3: 动态数组 + ReDim As C (文档里最初登记的形状)
    Dim a() As ArrClsShape
    ReDim a(1) As ArrClsShape
    Set a(0) = New ArrClsShape
    a(0).Move 6
    If a(0).Side() = 7# Then
        Debug.Print "AC3:OK"
    Else
        Debug.Print "AC3:FAIL side=" & a(0).Side()
    End If

    ' AC4: 动态数组 + ReDim 不带 As (元素类型来自 Dim 声明)
    Dim b() As ArrClsShape
    ReDim b(1)
    Set b(0) = New ArrClsShape
    b(0).Move 8
    If b(0).Side() = 9# Then
        Debug.Print "AC4:OK"
    Else
        Debug.Print "AC4:FAIL side=" & b(0).Side()
    End If

    ' AC5: ReDim Preserve —— 已有槽的实例与状态都还在, 新槽可用
    Dim p() As ArrClsShape
    ReDim p(1)
    Set p(0) = New ArrClsShape
    Set p(1) = New ArrClsShape
    p(0).Move 10
    p(1).Move 100
    ReDim Preserve p(3)
    Set p(2) = New ArrClsShape
    p(2).Move 20
    If p(0).Side() = 11# And p(1).Side() = 101# And p(2).Side() = 21# Then
        Debug.Print "AC5:OK"
    Else
        Debug.Print "AC5:FAIL " & p(0).Side() & "/" & p(1).Side() & "/" & p(2).Side()
    End If

    ' AC6: 模块级动态数组 (跨过程: 声明登记在模块作用域, 访问在 Main 里)
    ReDim m_items(1)
    Set m_items(0) = New ArrClsShape
    m_items(0).Move 12
    If m_items(0).Side() = 13# Then
        Debug.Print "AC6:OK"
    Else
        Debug.Print "AC6:FAIL side=" & m_items(0).Side()
    End If

    Debug.Print "ARRCLS-DONE"
End Sub
