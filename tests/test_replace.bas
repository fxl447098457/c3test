Option Explicit

' Fix 195 回归: Replace 必须返回**完整**结果。
' 原实现无条件用手工 malloc + "字符数"长度前缀构造结果 BSTR, 而 Windows 下
' vb6_BSTR_Len 走 SysStringLen (BSTR 前缀是**字节数**) —— 于是任何替换成功的
' 结果都被读成一半长度: Replace("abc","b","") 得 "a" 而不是 "ac",
' Replace("alpha beta"," ","|") 得 "alpha" 而不是 "alpha|beta"。
' 更糟的是这块内存随后会被 SysFreeString 释放, 不是 OLE 分配的指针 → 堆损坏。
' 本用例把返回值**逐字**比掉, 旧实现必挂。

Sub Main()
    chk "R1", Replace("alpha beta", " ", "|"), "alpha|beta"
    chk "R2", Replace("aXbXc", "X", "YY"), "aYYbYYc"
    chk "R3", Replace("abcdef", "cd", ""), "abef"
    chk "R4", Replace("aaa", "a", "bb"), "bbbbbb"
    chk "R5", Replace("abc", "b", ""), "ac"
    chk "R6", Replace("hello", "z", "!"), "hello"
    chk "R7", Replace("a" & vbCrLf & "b", vbCrLf, "|"), "a|b"
    chk "R8", CStr(Len(Replace("abcdef", "cd", ""))), "4"
    chk "R9", Replace("aaa", "a", "b", 2), "abb"
    chk "R10", Replace("aaaa", "a", "b", 1, 2), "bbaa"
    Debug.Print "REPLACE-DONE"
End Sub

Private Sub chk(n As String, got As String, want As String)
    If got = want Then
        Debug.Print n & "=OK"
    Else
        Debug.Print n & "=BAD got=[" & got & "] want=[" & want & "]"
    End If
End Sub
