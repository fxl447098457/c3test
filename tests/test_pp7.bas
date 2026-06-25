' Test 7: nested #If + all features
#Const DEBUG = -1
#Const VERSION = 2

#If DEBUG Then
    Sub DebugPrint(msg As String)
        Print "DEBUG: "; msg
    End Sub
#Else
    Sub DebugPrint(msg As String)
    End Sub
#End If

#If VERSION = 1 Then
    Const AppName = "App V1"
#ElseIf VERSION = 2 Then
    Const AppName = "App V2"
#Else
    Const AppName = "App V3+"
#End If

#If DEBUG Then
    #If VERSION > 1 Then
        Sub DebugV2()
        End Sub
    #End If
#End If

#If Win32 Then
    Declare Function GetTickCount Lib "kernel32" () As Long
#End If

#If DEBUG And Win32 Then
    Sub DebugWin32()
    End Sub
#End If

Sub Main()
    Dim count As Long
    count = 10
    If count > 0 Then
        Print "Hello"
    End If
End Sub
