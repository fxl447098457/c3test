Attribute VB_Name = "TestGenerics"
' 泛型 (tB 扩展) 单模块回归: 泛型 UDT (含成员数组/嵌套) + 泛型 Function/Sub
' (显式实例化 + 裸调类型推断) + 零参特化 + 体内给函数名赋值改写.
' 期望输出由 run_tests.ps1 按标记断言 (GENERICS-DONE 收尾).
Option Explicit

Type Box(Of T)
    Val As T
    Nums(2) As T
    Name As String
End Type

Type Pair(Of A, B)
    L As A
    R As B
End Type

Function First(Of T)(arr() As T) As T
    First = arr(LBound(arr))       ' 给函数名赋值: 特化克隆须改写成扁名
End Function

Function Item(Of T)(arr() As T, idx As Long) As T
    Item = arr(LBound(arr) + idx)  ' 具体形参 idx 与 T() 混合绑定
End Function

Sub ShowLen(Of T)(arr() As T)
    Debug.Print "LEN="; (UBound(arr) - LBound(arr) + 1)
End Sub

Function Ret(Of T)() As T
    Ret = 0                        ' 零实参显式特化 (arity 1 类型参数, 0 过程参数)
End Function

Sub Main()
    ' --- 泛型 UDT (显式) ---
    Dim b As Box(Of Long)
    b.Val = 7
    b.Nums(1) = 5
    b.Name = "x"
    Debug.Print "G-A="; b.Val + b.Nums(1)          ' 12
    Dim s As Box(Of String)
    s.Val = "hi"
    Debug.Print "G-B="; s.Val                      ' hi

    ' --- 嵌套泛型 UDT (依赖序注入) ---
    Dim pr As Pair(Of Long, String)
    pr.L = 20
    pr.R = "abc"
    Dim bp As Box(Of Pair(Of Long, String))
    bp.Val.L = pr.L + 1                            ' 21
    bp.Nums(2).R = pr.R & "!"
    Debug.Print "G-C="; bp.Val.L; " "; bp.Nums(2).R

    ' --- 泛型 Function: 显式实例化 ---
    Dim nums(2) As Long
    nums(0) = 10
    nums(1) = 20
    nums(2) = 30
    Dim strs(1) As String
    strs(0) = "ab"
    strs(1) = "cd"
    Dim a As Long
    a = First(Of Long)(nums)
    Debug.Print "G-D="; a                          ' 10

    ' --- 泛型 Function/Sub: 裸调类型推断 ---
    Dim e As Long
    e = First(nums)
    Debug.Print "G-E="; e                          ' 10
    Debug.Print "G-F="; First(strs)                ' ab
    Debug.Print "G-G="; Item(nums, 1)              ' 20
    ShowLen strs                                   ' LEN=2

    ' --- 零实参特化 ---
    Debug.Print "G-H="; Ret(Of Long)()             ' 0

    ' --- 同一实例化跨语句去重 (两条调用物化同一特化) ---
    Debug.Print "G-I="; First(nums) + First(Of Long)(nums)  ' 20

    Debug.Print "GENERICS-DONE"
End Sub
