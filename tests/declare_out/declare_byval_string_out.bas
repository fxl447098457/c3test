' Fix 161b-decl-out 夹具: Declare A 版 **ByVal String 出参**(作可写缓冲)。
' 判据: GetUserName 写入后须能回读 (name=[Administrator]); 修复前 BSTR 一字未动 → name=[]。

Option Explicit

' 路径1: ByVal buf As String 当缓冲 (GetUserName 形态, 文档说绿的)
Declare Function GetUserName Lib "advapi32" Alias "GetUserNameA" _
    (ByVal lpBuffer As String, ByRef nSize As Long) As Long

Public Sub Main()
    Dim buf As String
    Dim n As Long
    buf = String$(256, 0)
    n = 256
    GetUserName buf, n
    Debug.Print "len="; n
    Debug.Print "name=["; Left$(buf, n - 1); "]"
End Sub
