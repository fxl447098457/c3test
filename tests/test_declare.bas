' VB6 Declare Statement Test
' Tests: Declare Function, Declare Sub, Lib, Alias, parameters

Option Explicit

' --- Declare Function (stdcall, no params) ---
Declare Function GetTickCount Lib "kernel32" () As Long

' --- Declare Sub (stdcall, with params) ---
Declare Sub Sleep Lib "kernel32" (ByVal dwMilliseconds As Long)

' --- Declare with Alias (A-suffix function) ---
Declare Function GetUserName Lib "advapi32" Alias "GetUserNameA" (ByVal lpBuffer As String, ByRef nSize As Long) As Long

Public Sub Main()
    Dim tick As Long
    
    ' Test 1: Call declare function
    tick = GetTickCount()
    Debug.Print "Declare test OK"
    Debug.Print tick
End Sub
