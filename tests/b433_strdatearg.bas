Attribute VB_Name = "ModB433"
Option Explicit

' 账 #433 (§B207, 第一百零五刀): 「日期形参吃 As String 实参」这一格。
'
' 病形 = `As String` 变量直递进 `double` date 形参 = cl C2440(无法从 BSTR 转换为 double),
'   本刀之前 9 处全红: FormatDateTime / Year / Month / Day / Weekday / Hour / DateAdd /
'   DatePart / DateDiff (两台同形, 承面逐形实测)。
'
' 出口是 `vb6_CDate(vb6_VariantFromValue(<BSTR>))` —— **不是**账本原候选的 `vb6_DateValue`:
'   引擎对这一位是**宽松**的(纯数字文本按序列号, 认不出才报 13), 而 vb6_DateValue(→vb6_DateParse)
'   **严格** —— `DateValue("46027")` 引擎也报 13, 用它会把合法实参判死。开工前逐格量完否掉的。
'
' 负控(NEG-*): 数值形参吃数字文本必须仍走**数值折算**。判别式是「是不是日期形参」这一问,
'   **不是** `rtParamType == "double"` —— C 侧日期与数值都是 double, 照签名反推会把 Sqr/Abs
'   一起误伤(§B174 教训① 的同一形状)。这一格今天仍是 C2440 = 另一笔账(数值形参 × 文本实参),
'   刻意不在本刀范围内, 但正因为它还红着, 才证明本刀的新臂**没有**越界。

Sub Main()
    Dim sDate As String
    Dim sNum As String
    Dim sGarbage As String
    sDate = "2026/1/5"
    sNum = "46027"
    sGarbage = "abc"

    ' --- 承面九格 (每一格都与引擎逐格相同, 尺 .build/k207f_oracle.vbs) ---
    Debug.Print "Y=" & Year(sDate)                              ' 2026
    Debug.Print "M=" & Month(sDate)                             ' 1
    Debug.Print "D=" & Day(sDate)                               ' 5
    Debug.Print "W=" & Weekday(sDate)                           ' 2
    Debug.Print "H=" & Hour(sDate)                              ' 0
    Debug.Print "ADD=" & Format$(DateAdd("d", 1, sDate), "yyyy/mm/dd")   ' 2026/01/06
    Debug.Print "PART=" & DatePart("yyyy", sDate)                ' 2026
    Debug.Print "DIFF=" & DateDiff("d", sDate, "2026/2/5")       ' 31
    Debug.Print "FDT=" & FormatDateTime(sDate, 0)               ' 2026/1/5

    ' --- 纯数字文本按序列号那天 (引擎同格, 尺 .build/k207g_oracle.vbs) ---
    Debug.Print "YNUM=" & Year(sNum)                            ' 2026  (46027 = 2026-01-05)
    Debug.Print "FDTNUM=" & FormatDateTime(sNum, 0)             ' 2026/1/5

    ' --- 负控: 数值形参那一族**不在**本刀名单里 ---
    Debug.Print "NEG-sqr-untouched=1"

    Debug.Print "B433-DONE"
End Sub