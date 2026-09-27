' Fix 161b-decl-out 夹具: Declare A 版 ByVal String 出参之二 (GetModuleFileNameA)。
' 判据: path=[<完整 exe 路径>]; 修复前只回读到空 (n 有值但 buf 未回写)。

Option Explicit

Declare Function GetModuleHandleA Lib "kernel32" (ByVal lpModuleName As String) As Long
Declare Function GetModuleFileName Lib "kernel32" Alias "GetModuleFileNameA" _
    (ByVal hModule As Long, ByVal lpFilename As String, ByVal nSize As Long) As Long

Public Sub Main()
    Dim buf As String
    Dim n As Long
    buf = String$(260, 0)
    n = GetModuleFileName(0, buf, 260)
    Debug.Print "n="; n
    Debug.Print "path=["; Left$(buf, n); "]"
End Sub
