Option Explicit

' ai/028 V2 回归: 反引号串里的 ${expr} / ${expr:fmt} 插值。
' 读数全部钉在"与手写的 & CStr() / Format$ 逐项相同"上 —— 插值在词法出口就展开成普通
' token, 下游看到的正是手写形 (计划书 R2/R4: 不许有第二种字符串)。

Private Function Wrap(ByVal s As String) As String
    Wrap = "<" & s & ">"
End Function

Sub Main()
    Dim n As Long
    Dim v As Variant
    Dim d As Date
    n = 1234
    v = 45.5
    d = #1/2/2020#

    ' 默认语义: ${e} 与 & CStr(e) 同读数 (Long / Variant / Date / 表达式 / 函数调用)
    chk "RI01", `n=${n}`, "n=" & CStr(n)
    chk "RI02", `v=${v}`, "v=" & CStr(v)
    chk "RI03", `d=${d}`, "d=" & CStr(d)
    chk "RI04", `sum=${n + 1}`, "sum=" & CStr(n + 1)
    chk "RI05", `up=${UCase$("abc")}`, "up=ABC"
    ' 定界: 孔里的串带 '}' 也不算闭合 (闭孔由"跳过串"的扫描决定, 不靠数花括号)
    chk "RI06", `w=${Wrap("a}b")}`, "w=<a}b>"
    ' 格式段 ≡ Format$(e, "fmt")。#,##0 里的 '#' 在表达式位另有身份, 故格式段按纯文本读。
    chk "RI07", `m=${n:#,##0}`, "m=" & Format$(n, "#,##0")
    chk "RI08", `dt=${d:yyyy-MM-dd}`, "dt=" & Format$(d, "yyyy-MM-dd")
    chk "RI09", `two=${n}|${v}`, "two=" & CStr(n) & "|" & CStr(v)
    ' JSON 安全: 裸的花括号与冒号是文本, 只有 '${' 才开孔
    Dim j As String
    j = `json={"k": "v", "arr": [1,2], "price": ${n}}`
    chk "RI10", j, "json={""k"": ""v"", ""arr"": [1,2], ""price"": " & CStr(n) & "}"
    chk "RI11", `dollar=$5 and ${n}`, "dollar=$5 and " & CStr(n)
    ' 字面 '${' 的出口 = $${
    chk "RI12", `echo $${HOME} stays`, "echo ${HOME} stays"
    ' 多行串与插值共存: 行界仍是 CRLF
    Dim s As String
    s = `line1 ${n}
line2 "${v}"`
    chk "RI13", s, "line1 " & CStr(n) & vbCrLf & "line2 """ & CStr(v) & """"
    chk "RI14", CStr(Len(s)), "24"
    ' 展开后的优先级与手写同级 (外面那一层括号是设计的一部分)
    chk "RI15", `x${n}` & "y", "x" & CStr(n) & "y"
    ' 嵌套调用 + 空格 + 非 ASCII
    chk "RI16", `nested=${ Wrap( CStr(n * 2) ) }`, "nested=<2468>"
    chk "RI17", `cn=姓名${n}`, "cn=姓名" & CStr(n)
    ' 无孔的串走 V1 出口, 一字不变; 裸 '{' '}' 也不是孔
    chk "RI18", `plain, no hole`, "plain, no hole"
    chk "RI19", `brace{not}=a hole`, "brace{not}=a hole"
    ' R1 的另一半: 单个 $ 与 $$ 都不是孔的开头, 串里写 Chr(96) 也不算开孔
    chk "RI20", `100$ and $$ and Chr(96)`, "100$ and $$ and Chr(96)"
    ' 普通的 "..." 永不插值 (这条判据钉的是"新语法不吃老语法")
    Dim lit As String
    lit = "a${n}b"
    chk "RI21", lit & "|" & CStr(Len(lit)), "a${n}b|6"
    ' 直接进 Print 出口 (整串就是表达式)
    Debug.Print `p=${n}`
    Debug.Print "INTERP-DONE"
End Sub

Private Sub chk(n As String, got As String, want As String)
    If got = want Then
        Debug.Print n & "=OK"
    Else
        Debug.Print n & "=BAD got=[" & got & "] want=[" & want & "]"
    End If
End Sub
