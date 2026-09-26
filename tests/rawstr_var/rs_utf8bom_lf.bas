Option Explicit

' ai/028 V1: 同一份内容存成 4 种 (编码 x 行尾) —— 读回的字符串必须逐字相同。
' 前一对量解码 (GBK / UTF-8 BOM), 后一对量"行界归一成 CRLF"这条口径。

Sub Main()
    Dim a As String
    a = `姓名: 张三
tag = "vip"`
    chk "RV1", a, "姓名: 张三" & vbCrLf & "tag = ""vip"""
    chk "RV2", CStr(Len(a)), "19"
    chk "RV3", CStr(InStr(a, vbCrLf)), "7"
    Debug.Print "RV-DONE"
End Sub

Private Sub chk(n As String, got As String, want As String)
    If got = want Then
        Debug.Print n & "=OK"
    Else
        Debug.Print n & "=BAD got=[" & got & "] want=[" & want & "]"
    End If
End Sub
