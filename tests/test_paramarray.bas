' P14.1.5: ParamArray comprehensive test
Sub Sum(ParamArray args())
    Dim total As Long
    Dim i As Long
    total = 0
    For i = 0 To UBound(args)
        total = total + args(i)
    Next i
    Debug.Print total
End Sub

Sub TestUBound(ParamArray args())
    Debug.Print UBound(args)
End Sub

Sub TestMissing(ParamArray args())
    If IsMissing(args) Then
        Debug.Print 0
    Else
        Debug.Print 1
    End If
End Sub

Sub MixedParam(n As Long, ParamArray args())
    Debug.Print n
    If IsMissing(args) Then
        Debug.Print 0
    Else
        Debug.Print UBound(args)
    End If
End Sub

Sub Main()
    ' Test 1: Sum with 3 args -> 6
    Sum 1, 2, 3
    
    ' Test 2: Sum with 4 args -> 100
    Sum 10, 20, 30, 40
    
    ' Test 3: UBound of 3-arg ParamArray -> 2
    TestUBound 10, 20, 30
    
    ' Test 4: IsMissing with no args -> 0 (True)
    TestMissing
    
    ' Test 5: IsMissing with args -> 1 (False)
    TestMissing 42
    
    ' Test 6: Mixed params, n=42, PA has 2 args
    MixedParam 42, 1, 2   ' 42 then 1
    
    ' Test 7: Mixed with no PA args -> 99 then 0
    MixedParam 99         
    
    Debug.Print 9999       ' sentinel
End Sub
