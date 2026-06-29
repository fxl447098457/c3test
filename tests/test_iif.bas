' P14.2.4: IIf function test
Sub Main()
    ' IIf with numeric values
    Dim x As Long
    x = IIf(True, 10, 20)
    Debug.Print "IIf(True,10,20)="; x
    
    x = IIf(False, 10, 20)
    Debug.Print "IIf(False,10,20)="; x
    
    ' IIf with string values
    Dim s As String
    s = IIf(x > 15, "Big", "Small")
    Debug.Print "IIf(20>15,Big,Small)="; s
    
    ' IIf with expression condition
    Dim a As Long
    a = 5
    x = IIf(a > 3, a * 2, a + 1)
    Debug.Print "IIf(5>3,10,6)="; x
    
    Debug.Print "IIf test PASSED"
End Sub
