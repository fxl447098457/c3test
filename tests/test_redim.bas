Option Explicit

Public Sub Test()
    Dim arr() As Long
    Erase arr
    ReDim arr(1 To 10)
    ReDim Preserve arr(1 To 20)
End Sub
