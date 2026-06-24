Option Explicit

Public Sub TestOnError()
    On Error GoTo ErrorHandler
    On Error Resume Next
    On Error GoTo 0
    
ErrorHandler:
    Debug.Print "Error"
End Sub
