Option Explicit

Public Sub TestAll()
    Dim x As Long
    Dim y As Long
    
    If x > 10 Then
        y = 1
    ElseIf x > 5 Then
        y = 2
    Else
        y = 3
    End If
    
    For x = 1 To 10 Step 2
        y = y + x
    Next x
    
    Do While x < 100
        x = x + 1
    Loop
End Sub
