' P6.13 TypeLib test - verify TypeLib can be loaded from DLL
Option Explicit

Public Sub Main()
    Dim calc As Object
    Set calc = CreateObject("TestAXDLL.Calc")
    
    Dim r As Long
    r = calc.Add(10, 20)
    Print r
    
    r = calc.Multiply(5, 6)
    Print r
    
    calc.SetValue 42
    r = calc.GetValue()
    Print r
    
    Print "P6.13 PASSED"
End Sub
