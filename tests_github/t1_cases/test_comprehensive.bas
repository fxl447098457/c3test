' VB6 Comprehensive Parser Test
' Covers: If/For/Do/While/Select/With/GoTo/GoSub/Return/Exit/On Error/Erase/ReDim

Option Explicit

Public Sub TestAll()
    Dim x As Long
    Dim y As Long
    Dim arr() As Long
    
    ' --- If/ElseIf/Else ---
    If x > 10 Then
        y = 1
    ElseIf x > 5 Then
        y = 2
    Else
        y = 3
    End If
    
    ' --- For loop with Step ---
    For x = 1 To 10 Step 2
        y = y + x
    Next x
    
    ' --- Do While...Loop ---
    Do While x < 100
        x = x + 1
    Loop
    
    ' --- Do...Loop Until ---
    Do
        x = x - 1
    Loop Until x <= 50
    
    ' --- While...Wend ---
    While x > 0
        x = x - 1
    Wend
    
    ' --- Select Case ---
    Select Case y
        Case 1
            x = 10
        Case 2, 3
            x = 20
        Case Else
            x = 0
    End Select
    
    ' --- On Error ---
    On Error GoTo ErrorHandler
    On Error Resume Next
    On Error GoTo 0
    
    ' --- Exit ---
    Exit Sub
    
    ' --- GoSub/Return ---
    GoSub MyLabel
    GoTo SkipLabel
    
MyLabel:
    y = y + 1
    Return
    
SkipLabel:
    ' --- Erase ---
    Erase arr
    
    ' --- ReDim ---
    ReDim arr(1 To 10)
    ReDim Preserve arr(1 To 20)
    
    ' --- Set/Let ---
    Dim obj As Object
    Set obj = Nothing
    Let x = 5
    
    ' --- End (terminate program) ---
    ' End  -- commented out to not stop execution
    
ErrorHandler:
    Debug.Print "Error occurred"
End Sub

Private Function Add(ByVal a As Long, ByVal b As Long) As Long
    Add = a + b
End Function
