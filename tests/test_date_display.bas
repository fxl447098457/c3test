Option Explicit

' ============================================================
'  test_date_display.bas - Fix 175: Date 的类型可见性 + CStr(Date) 口径
'
'  C 后端把 `As Date` 与 `As Double` 都 emit 成 double, 注册表按 C 类型串分派,
'  于是 inferExprType 不知道谁是 Date: 打印/拼接一律走 vb6_CStrDbl →
'  VBFlexGridDemo 的日期列显示 46023 而不是 2026/1/1。另一半是 vb6_CStrDate
'  借用了 Format(v, NULL), 而那条分支无条件拼 "短日期 + 时间" → 会变成
'  "2026/1/1 0:00:00"。VB6 的规则是时间分量为 0 时只给日期。
'  短日期串的具体形态取决于系统 locale (DATE_SHORTDATE), 所以断言只问
'  "有没有裸序列号 / 有没有年月日", 不钉死分隔符。
' ============================================================

Private Function TF(ByVal b As Boolean) As String
    If b Then TF = "Y" Else TF = "N"
End Function

Private Function ShowStr(ByVal s As String) As String
    ShowStr = s
End Function

Sub Main()
    Dim d As Date
    Dim n As Long
    Dim s As String
    d = DateSerial(2026, 1, 1)
    n = 42

    s = d & ""
    Debug.Print "D1-noserial=" & TF(InStr(s, "46023") = 0)
    Debug.Print "D2-year=" & TF(InStr(s, "2026") > 0)
    Debug.Print "D2b-notime=" & TF(InStr(s, ":") = 0)

    s = (d + 1) & ""
    Debug.Print "D3-nextday=" & TF(InStr(s, "46024") = 0)
    Debug.Print "D4-nextyear=" & TF(InStr(s, "2026") > 0)

    Debug.Print "D5-diff0=" & TF((d - d) = 0)
    Debug.Print "D6-param=" & TF(InStr(ShowStr(d), "2026") > 0)
    Debug.Print "D7-longparam=" & TF(InStr(ShowStr(n), "42") > 0)
    Debug.Print "D8-cstr=" & TF(InStr(CStr(d), "46023") = 0)
    Debug.Print "D9-format=" & TF(InStr(Format(d, "yyyy/mm/dd"), "2026/01/01") > 0)
    Debug.Print "DATE-DONE"
End Sub
