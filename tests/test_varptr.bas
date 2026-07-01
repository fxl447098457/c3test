Sub Main()
    Dim x As Long
    x = 42
    Dim p As Long
    p = VarPtr(x)
    Debug.Print "VarPtr OK"; p
End Sub
