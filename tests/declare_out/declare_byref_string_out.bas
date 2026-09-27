' Fix 161b-decl-out 夹具: Declare A 版 **ByRef String 出参**。
' 判据: 同 ByVal (name=[Administrator]); 修复前既 LNK2019(vb6_di_ 未定名) 又出参读空。

Option Explicit

' ByRef String 出参形态
Declare Function GetUserNameB Lib "advapi32" Alias "GetUserNameA" _
    (ByRef lpBuffer As String, ByRef nSize As Long) As Long

Public Sub Main()
    Dim buf As String
    Dim n As Long
    buf = String$(256, 0)
    n = 256
    GetUserNameB buf, n
    Debug.Print "len="; n
    Debug.Print "name=["; Left$(buf, n - 1); "]"
    ' 断言用稳定标记 (run_tests 的 -like 断言里 [ ] 是通配符字符集, 不可用括号串)。
    If Len(Left$(buf, n - 1)) > 0 Then
        Debug.Print "byref-name-ok=Y"
    Else
        Debug.Print "byref-name-ok=N"
    End If
End Sub
