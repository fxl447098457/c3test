Attribute VB_Name = `RsMain`
Option Explicit

' ai/028 V1 批的 R4 验收 (计划书 §三.3 那四处独立折叠, 每处打一个落点):
'   1) 模块头 Attribute 的值 = 反引号串 (parseAttribute 的取值就是 parseExpression)
'      —— 这行必须折成 RsMain, 否则模块名与 .vbp 里的 Module=RsMain 对不上。
'   2) CreateObject(工程内 ProgID) = 反引号串 (coclass_activate 的 unquote 比对)
'      —— 没折对就没有改写, 运行期变成查注册表, 直接 429。
'   3/4) Const 值与 Declare 的 Lib 名在 tests\test_rawstr.bas 里, 同一条队列。

Sub Main()
    On Error Resume Next

    ' 早绑定到工程类: 改写产物就是 New RsGreeter, 直调成员 (晚绑定要点名, 另说)
    Dim o As RsGreeter
    Set o = CreateObject(`RsApp.Greeter`)
    If Err.Number <> 0 Then
        Debug.Print "RP1=BAD err=" & Err.Number
        Exit Sub
    End If
    If o Is Nothing Then
        Debug.Print "RP1=BAD nothing"
        Exit Sub
    End If
    Debug.Print "RP1=OK"

    ' 类模块里返回的原始多行串: 跨模块边界后仍然是 CRLF 界 + 原样的引号
    Dim s As String
    s = o.Note()
    If s = "第一行" & vbCrLf & "第二行 = ""rs""" Then
        Debug.Print "RP2=OK"
    Else
        Debug.Print "RP2=BAD len=" & Len(s)
    End If
    If InStr(s, Chr(96)) = 0 Then
        Debug.Print "RP3=OK"
    Else
        Debug.Print "RP3=BAD stray backtick"
    End If
    If o.Tick() = Chr(82) Then
        Debug.Print "RP4=OK"
    Else
        Debug.Print "RP4=BAD"
    End If

    ' 手写的 "" 与反引号串在同一个表达式里共存, 读数一致
    If o.Note() = `第一行
第二行 = "rs"` Then
        Debug.Print "RP5=OK"
    Else
        Debug.Print "RP5=BAD"
    End If
    Debug.Print "RP-DONE"
End Sub
