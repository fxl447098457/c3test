Option Explicit

Public Sub TestSelect()
    Dim x As Long
    Dim y As Long
    
    Select Case y
        Case 1
            x = 10
        Case 2, 3
            x = 20
        Case Else
            x = 0
    End Select
End Sub
