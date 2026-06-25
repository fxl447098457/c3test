' Test 12: just Main with code after #If
#Const DEBUG = -1

#If DEBUG Then
    Sub DebugPrint()
    End Sub
#End If

Sub Main()
    Dim count As Long
    count = 10
    If count > 0 Then
        Print "Hello"
    End If
End Sub
