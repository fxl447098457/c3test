' Test 6: #ElseIf + logical operators
#Const DEBUG = -1
#Const VERSION = 2

#If VERSION = 1 Then
    Const AppName = "App V1"
#ElseIf VERSION = 2 Then
    Const AppName = "App V2"
#Else
    Const AppName = "App V3+"
#End If

#If DEBUG And Win32 Then
    Sub DebugWin32()
    End Sub
#End If

Sub Main()
End Sub
