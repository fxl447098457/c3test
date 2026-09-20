Option Explicit

Public Sub Test()
    Dim x As Long
    GoSub MyLabel
    Exit Sub
MyLabel:
    x = x + 1
    Return
End Sub
