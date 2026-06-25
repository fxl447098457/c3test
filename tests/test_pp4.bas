' Test 4: Win32 builtin + Declare
#If Win32 Then
    Declare Function GetTickCount Lib "kernel32" () As Long
#Else
    Declare Sub DummyGetTick Lib "legacy" ()
#End If

Sub Main()
End Sub
