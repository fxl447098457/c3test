Option Explicit

Sub Main()
    Dim n As Long
    n = 7
    Dim s As String
    s = `one ${n}
two ${alsoNope} three`
    Debug.Print s
End Sub
