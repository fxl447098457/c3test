' Test 9: pp8 + nested #If
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

Sub Main()
End Sub
