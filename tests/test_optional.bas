' P14.1.4: Optional parameter default value test

Sub Main()
    Dim r As Long
    r = 0

    ' Test 1: Optional ByRef with explicit default (VB6 default is ByRef)
    Dim v1 As Long
    v1 = AddNumbers(10)
    If v1 = 30 Then r = r + 1
    Debug.Print r          ' expect: 1

    ' Test 2: Optional with both args
    Dim v2 As Long
    v2 = AddNumbers(10, 5)
    If v2 = 15 Then r = r + 1
    Debug.Print r          ' expect: 2

    ' Test 3: Optional ByVal String with default
    Dim v3 As String
    v3 = Greet("World")
    If Len(v3) > 0 Then r = r + 1
    Debug.Print r          ' expect: 3

    ' Test 4: Optional ByVal Boolean default
    Dim v4 As Long
    v4 = CheckFlag()
    If v4 = 0 Then r = r + 1
    Debug.Print r          ' expect: 4

    ' Test 5: Optional with no explicit default (type zero)
    Dim v5 As Long
    v5 = WithNoDefault(100)
    If v5 = 100 Then r = r + 1
    Debug.Print r          ' expect: 5

    Debug.Print 9999       ' sentinel
End Sub

Function AddNumbers(x As Long, Optional y As Long = 20) As Long
    AddNumbers = x + y
End Function

Function Greet(ByVal name As String, Optional ByVal greeting As String = "Hello") As String
    Greet = greeting
End Function

Function CheckFlag(Optional ByVal flag As Boolean = False) As Long
    If flag Then
        CheckFlag = -1
    Else
        CheckFlag = 0
    End If
End Function

Function WithNoDefault(ByVal x As Long, Optional ByVal y As Long) As Long
    WithNoDefault = x + y
End Function
