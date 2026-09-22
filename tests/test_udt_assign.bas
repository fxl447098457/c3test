Option Explicit

' ============================================================
'  test_udt_assign.bas - Fix 178: 含所有权成员 (String / 动态数组) 的 UDT 赋值
'
'  VB6 里 `b = a` 与 `LSet b = a` 对这类 UDT 走类型描述符**深拷贝**: 赋值后两侧各
'  持有自己的字符串/子数组。C 后端此前落成结构体赋值或 memcpy (浅拷贝) → 两侧共享
'  同一 BSTR 与同一数组载体, 于是 VBFlexGridDemo 里 `LSet Rows(i) = 模板` 把行 2..149
'  的 Cols 全别名到行 0 (写一行、整表变)。
'  本用例只断言"改副本不能影响原件", 不依赖任何具体释放时机。
' ============================================================

Private Type TOwned
    A() As Long
    S As String
    N As Long
End Type

Private Type THost
    Items() As TOwned
    K As Long
End Type

Sub Main()
    Dim x As TOwned, y As TOwned
    ReDim x.A(0 To 2)
    x.A(0) = 1
    x.A(1) = 2
    x.A(2) = 3
    x.S = "hello"
    x.N = 42

    y = x
    y.A(0) = 99
    y.S = "world"
    y.N = 7
    Debug.Print "A1=" & x.A(0) & ";S1=" & x.S & ";N1=" & x.N
    Debug.Print "A2=" & y.A(0) & ";S2=" & y.S & ";N2=" & y.N

    Dim h1 As THost, h2 As THost
    ReDim h1.Items(0 To 1) As TOwned
    ReDim h1.Items(0).A(0 To 1) As Long
    h1.Items(0).A(0) = 11
    h1.Items(0).S = "alpha"
    h1.K = 3
    h2 = h1
    h2.Items(0).A(0) = 22
    h2.Items(0).S = "beta"
    Debug.Print "H1=" & h1.Items(0).A(0) & ";HS1=" & h1.Items(0).S

    Dim arr(0 To 1) As TOwned
    ReDim arr(0).A(0 To 0)
    arr(0).A(0) = 5
    arr(0).S = "five"
    arr(1) = arr(0)
    arr(1).A(0) = 6
    arr(1).S = "six"
    Debug.Print "E1=" & arr(0).A(0) & ";ES1=" & arr(0).S

    Dim z As TOwned
    LSet z = x
    z.A(1) = 77
    z.S = "LSET"
    Debug.Print "L1=" & x.A(1) & ";LS1=" & x.S
    Debug.Print "UDT-ASSIGN-DONE"
End Sub
