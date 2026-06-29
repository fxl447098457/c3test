' P14.2.3: Split/Join test

Sub Main()
    Dim r As Long
    r = 0

    ' Test 1: Split with comma delimiter
    Dim parts() As String
    parts = Split("a,b,c", ",")
    If UBound(parts) = 2 Then r = r + 1
    Debug.Print r          ' expect: 1

    ' Test 2: Join with comma
    Dim joined As String
    joined = Join(parts, ",")
    If Len(joined) > 0 Then r = r + 1
    Debug.Print r          ' expect: 2

    ' Test 3: Split with default (space) delimiter
    Dim words() As String
    words = Split("Hello World")
    If UBound(words) = 1 Then r = r + 1
    Debug.Print r          ' expect: 3

    ' Test 4: Join with default (space) delimiter
    Dim rejoin As String
    rejoin = Join(words)
    If Len(rejoin) > 0 Then r = r + 1
    Debug.Print r          ' expect: 4

    ' Test 5: Split empty string
    Dim emptyResult() As String
    emptyResult = Split("")
    If UBound(emptyResult) = 0 Then r = r + 1
    Debug.Print r          ' expect: 5

    Debug.Print 9999       ' sentinel
End Sub
