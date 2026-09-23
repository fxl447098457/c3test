Attribute VB_Name = "GmodB"
Option Explicit

Sub Main()
    ' 泛型 UDT 跨模块 (显式)
    Dim g As GBox(Of Long)
    g.V = 33
    g.Tag = "xm"
    Debug.Print "XM1="; g.V + Len(g.Tag)           ' 35
    Dim s As GBox(Of String)
    s.V = "ab"
    Debug.Print "XM2="; s.V                        ' ab

    ' 泛型 Function 跨模块裸调 (推断)
    Dim n(2) As Long
    n(0) = 5
    n(1) = 12
    n(2) = 99
    Dim st(1) As String
    st(0) = "hi"
    st(1) = "yo"
    Debug.Print "XM3="; Pick(n, 1)                 ' 12 (T=Long)
    Debug.Print "XM4="; Pick(st, 0)                ' hi  (T=String)
    Debug.Print "XM5="; Sum2(3, 4)                 ' 7   (标量绑定)
    Debug.Print "XM6="; Pick(Of Long)(n, 2)        ' 99  (显式)
    ShowLen st                                     ' XLEN=2

    ' 泛型 UDT 嵌套使用 (Box 里装 Box)
    Dim outer As GBox(Of Long)
    outer.V = g.V + 1                              ' 复用显式特化 (去重)
    Debug.Print "XM7="; outer.V                    ' 34
    Debug.Print "XMOD-DONE"
End Sub
