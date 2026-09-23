Option Explicit
Public Function Pick(ByVal A As Long) As Long
    Pick = 100 + A
End Function
Public Function Pick(ByVal A As String) As Long
    Pick = 200 + Len(A)
End Function
Public Sub Show(ByVal M As Long)
    Debug.Print "XM3=SUB"; M
End Sub
Public Sub Show(ByVal M As String)
    Debug.Print "XM3=STR"; M
End Sub
