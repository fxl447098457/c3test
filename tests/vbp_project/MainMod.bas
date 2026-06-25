' MainMod.bas - 主模块
Option Explicit

Public Sub Main()
    Dim result As Long
    result = Add(10, 20)
    Debug.Print "Add(10, 20) ="
    Debug.Print result
    result = Multiply(5, 6)
    Debug.Print "Multiply(5, 6) ="
    Debug.Print result
End Sub
