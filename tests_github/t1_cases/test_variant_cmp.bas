' test_variant_cmp.bas - Bug 2: Variant numeric comparison
Option Explicit

Sub Main()
    Dim v As Variant
    v = 42
    
    ' Test 1: Variant > Long comparison
    If v > 0 Then
        Debug.Print "VC-1:OK v>0"
    End If
    
    ' Test 2: Variant = Long comparison
    If v = 42 Then
        Debug.Print "VC-2:OK v=42"
    End If
    
    ' Test 3: Long > Variant comparison
    If 100 > v Then
        Debug.Print "VC-3:OK 100>v"
    End If
    
    ' Test 4: Variant string comparison
    v = "hello"
    If v = "hello" Then
        Debug.Print "VC-4:OK v=hello"
    End If
    
    Debug.Print "VC:4/4"
End Sub