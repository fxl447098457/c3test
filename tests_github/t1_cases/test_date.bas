' test_date.bas - P8.6 Year/Month/Day parameter test
Sub Main()
    ' Test: Year/Month/Day should use the date parameter, not current time
    ' 2025-01-15 = Excel serial 45672
    Dim d As Double
    d = 45672
    
    If Year(d) = 2025 Then
        Debug.Print "PASS_Year"
    Else
        Debug.Print "FAIL_Year"
    End If
    
    If Month(d) = 1 Then
        Debug.Print "PASS_Month"
    Else
        Debug.Print "FAIL_Month"
    End If
    
    If Day(d) = 15 Then
        Debug.Print "PASS_Day"
    Else
        Debug.Print "FAIL_Day"
    End If
    
    ' Test with Now (should work correctly)
    Dim n As Double
    n = Now
    If Year(n) > 2020 Then
        Debug.Print "PASS_NowYear"
    Else
        Debug.Print "FAIL_NowYear"
    End If
    
    Debug.Print "Done"
End Sub
