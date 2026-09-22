Option Explicit

' ============================================================
'  test_overload.bas - tB式过程重载 (Overloading) 运行门禁
'
'  口径 (tB 文档未定死部分由本测试钉住):
'   - 同名 Sub/Function 不同签名自动成组, 无关键字
'   - 逐参打分: 4=类型精确 / 3=数值域内widening / 2=隐式可转 / 1=Variant(形或实) / 0=淘汰
'   - 总分最高且唯一者胜; 平手报"歧义", 无匹配报"没有与实参匹配的重载版本"
'   - ParamArray 过程与类模块成员不进重载组 (v1); 跨模块重载属 O3
'  负例 (歧义/同签名重复/无匹配) 为一次性探针验证, 见提交说明.
' ============================================================

Private Delegate Function Operation(ByVal A As Long, ByVal B As Long) As Long

Private Sub Foo(ByVal A As Long)
    Debug.Print "F-L"; A
End Sub

Private Sub Foo(ByVal A As String)
    Debug.Print "F-S"; A
End Sub

Private Function Add2(ByVal A As Long, ByVal B As Long) As Long
    Add2 = A + B
End Function

Private Function Add2(ByVal A As String, ByVal B As String) As Long
    Add2 = Len(A) + Len(B)
End Function

' 参数个数区分 (含 Optional 可行域: () 与 (a[,b]) 两候选)
Private Sub Opt1()
    Debug.Print "O0"
End Sub

Private Sub Opt1(ByVal A As Long, Optional ByVal B As Long)
    Debug.Print "O1"; A
End Sub

' Variant 形参单独存在时接住 Variant 实参
Private Function Pick(ByVal A As Variant) As Long
    If VarType(A) = 8 Then Pick = 21 Else Pick = 22
End Function

Private Function Calc(ByVal A As Long, ByVal B As Long) As Long
    Calc = A + B
End Function

Private Function Calc(ByVal A As String, ByVal B As String) As Long
    Calc = 999
End Function

Sub Main()
    ' (1) 数值字面量优先落 Long 形 (Integer→Long 档3 胜 →String 档2)
    Foo 5
    ' (2) 字符串实参落 String 形
    Foo "hi"
    ' (3) Call 语句形态与无括号裸调用同规则
    Call Foo(6)
    ' (4) 双参类型分流
    Debug.Print Add2(1, 2)
    Debug.Print Add2("abc", "de")
    ' (5) Optional 可行域: 0参落 () 候选, 1/2参落 (a,b?) 候选
    Opt1
    Opt1 7
    Opt1 7, 8
    ' (6) Variant 实参 → Variant 形参 (4 精确)
    Dim v As Variant
    v = "s"
    Debug.Print Pick(v)
    ' (7) 重载组内 AddressOf/Delegate 绑定按签名选变体 (落 Long 版, 非 999)
    Dim op As Operation
    op = AddressOf Calc
    Debug.Print op(2, 3)
    Debug.Print "OVERLOAD-DONE"
End Sub
