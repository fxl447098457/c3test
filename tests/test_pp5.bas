' Test 5: #If with Sub in active branch + Win32 Declare
#Const DEBUG = -1

#If DEBUG Then
    Sub DebugPrint(msg As String)
        Print "DEBUG: "; msg
    End Sub
#Else
    Sub DebugPrint(msg As String)
    End Sub
#End If

#If Win32 Then
    Declare Function GetTickCount Lib "kernel32" () As Long
#End If

Sub Main()
End Sub
