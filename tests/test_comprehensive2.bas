Option Explicit

Public Sub Test()
    Dim x As Long
    Dim y As Long
    Dim arr() As Long
    Dim obj As Object
    
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
    
    Select Case y
        Case 1
            x = 10
        Case 2, 3
            x = 20
        Case Else
            x = 0
    End Select
    
    On Error GoTo ErrorHandler
    On Error Resume Next
    On Error GoTo 0
    
    Set obj = Nothing
    Let x = 5
    Erase arr
    
    Exit Sub
    
ErrorHandler:
    Debug.Print "Error occurred"
End Sub
