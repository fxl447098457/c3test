' P14.2.4: DateAdd/DateDiff/DatePart test

Sub Main()
    Dim r As Long
    r = 0

    ' Test 1: DateAdd - add 1 year
    Dim d1 As Double
    d1 = DateSerial(2024, 6, 15)
    Dim d2 As Double
    d2 = DateAdd("yyyy", 1, d1)
    If Year(d2) = 2025 Then r = r + 1
    Debug.Print r          ' expect: 1

    ' Test 2: DateAdd - add 3 months
    Dim d3 As Double
    d3 = DateAdd("m", 3, d1)
    If Month(d3) = 9 Then r = r + 1
    Debug.Print r          ' expect: 2

    ' Test 3: DateDiff - diff in days
    Dim d4 As Double
    d4 = DateSerial(2024, 6, 20)
    Dim diff As Long
    diff = DateDiff("d", d1, d4)
    If diff = 5 Then r = r + 1
    Debug.Print r          ' expect: 3

    ' Test 4: DatePart - get quarter
    Dim q As Long
    q = DatePart("q", d1)
    If q = 2 Then r = r + 1
    Debug.Print r          ' expect: 4

    ' Test 5: DatePart - get weekday (June 15, 2024 is Saturday = 7)
    Dim wd As Long
    wd = DatePart("w", d1)
    If wd > 0 Then r = r + 1
    Debug.Print r          ' expect: 5

    Debug.Print 9999       ' sentinel
End Sub
