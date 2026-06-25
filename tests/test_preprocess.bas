' test_preprocess.bas - 条件编译测试
' 测试 #Const, #If...#ElseIf...#Else...#End If

Option Explicit

' 定义条件编译常量
#Const DEBUG = -1
#Const VERSION = 2

' #If 基本用法
#If DEBUG Then
    Sub DebugPrint(msg As String)
        Print "DEBUG: "; msg
    End Sub
#Else
    Sub DebugPrint(msg As String)
        ' 空实现
    End Sub
#End If

' #If...#ElseIf...#Else...#End If
#If VERSION = 1 Then
    Const AppName = "App V1"
#ElseIf VERSION = 2 Then
    Const AppName = "App V2"
#Else
    Const AppName = "App V3+"
#End If

' 嵌套条件编译
#If DEBUG Then
    #If VERSION > 1 Then
        Sub DebugV2()
            Print "Debug V2 mode"
        End Sub
    #End If
#End If

' 使用内置常量 Win32
#If Win32 Then
    Declare Function GetTickCount Lib "kernel32" () As Long
#Else
    Declare Sub DummyGetTick Lib "legacy" ()
#End If

' 逻辑运算
#If DEBUG And Win32 Then
    Sub DebugWin32()
        Print "Debug on Win32"
    End Sub
#End If

Sub Main()
    Dim count As Long
    count = 10
    If count > 0 Then
        Print "Hello"
    End If
End Sub
