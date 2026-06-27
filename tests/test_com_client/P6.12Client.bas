' P6.12Client.bas - 纯VB6客户端COM调用ActiveX DLL
' 通过CreateObject调用test_activex_dll.dll中的对象
Option Explicit

Sub Main()
    Dim passCount As Long
    passCount = 0
    
    Debug.Print "=== P6.12 VB6 Client COM DLL Test ==="
    
    ' --- Calc (Long params/return) ---
    Dim calc As Object
    Set calc = CreateObject("TestAXDLL.Calc")
    
    Dim r1 As Long
    r1 = calc.Add(10, 20)
    If r1 = 30 Then
        passCount = passCount + 1
    End If
    
    Dim r2 As Long
    r2 = calc.Multiply(5, 6)
    If r2 = 30 Then
        passCount = passCount + 1
    End If
    
    calc.SetValue 42
    Dim r3 As Long
    r3 = calc.GetValue()
    If r3 = 42 Then
        passCount = passCount + 1
    End If
    
    ' --- MathLib (Double + recursive) ---
    Dim mlib As Object
    Set mlib = CreateObject("TestAXDLL.MathLib")
    
    mlib.SetPi 3.14159
    Dim piVal As Double
    piVal = mlib.GetPi()
    If piVal > 3.14 And piVal < 3.15 Then
        passCount = passCount + 1
    End If
    
    Dim r5 As Long
    r5 = mlib.Factorial(5)
    If r5 = 120 Then
        passCount = passCount + 1
    End If
    
    ' --- ExtLib (Property Let/Get Long) ---
    Dim ext As Object
    Set ext = CreateObject("TestAXDLL.ExtLib")
    
    ext.Count = 99
    Dim r6 As Long
    r6 = ext.Count
    If r6 = 99 Then
        passCount = passCount + 1
    End If
    
    ' --- 汇总 ---
    Debug.Print passCount; "/6 PASSED"
    
    Set ext = Nothing
    Set mlib = Nothing
    Set calc = Nothing
End Sub
