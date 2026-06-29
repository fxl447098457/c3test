' P14.2.1: Like operator test
' Tests: ?, *, #, [charlist], [!charlist]

Sub Main()
    Dim r As Long

    ' Test 1: * wildcard
    r = 0
    If "Hello World" Like "H*" Then r = r + 1
    If "Hello" Like "*" Then r = r + 1
    If "" Like "*" Then r = r + 1
    If "abc" Like "a*c" Then r = r + 1
    Debug.Print r         ' expect: 4

    ' Test 2: ? wildcard
    r = 0
    If "a" Like "?" Then r = r + 1
    If "ab" Like "?b" Then r = r + 1
    If "abc" Like "a?c" Then r = r + 1
    If "" Like "?" Then r = r + 1   ' expect: fail (? needs 1 char)
    If "ab" Like "??" Then r = r + 1
    Debug.Print r         ' expect: 4

    ' Test 3: # wildcard (single digit)
    r = 0
    If "5" Like "#" Then r = r + 1
    If "a" Like "#" Then r = r + 1   ' not a digit
    If "42" Like "##" Then r = r + 1
    If "3a" Like "#a" Then r = r + 1
    Debug.Print r         ' expect: 3

    ' Test 4: [charlist]
    r = 0
    If "a" Like "[abc]" Then r = r + 1
    If "d" Like "[abc]" Then r = r + 1   ' not in list
    If "m" Like "[a-z]" Then r = r + 1
    If "5" Like "[a-z]" Then r = r + 1   ' not a letter
    Debug.Print r         ' expect: 2

    ' Test 5: [!charlist] (negation)
    r = 0
    If "d" Like "[!abc]" Then r = r + 1
    If "a" Like "[!abc]" Then r = r + 1   ' in list, should fail
    If "x" Like "[!0-9]" Then r = r + 1
    If "5" Like "[!0-9]" Then r = r + 1   ' digit, should fail
    Debug.Print r         ' expect: 2

    ' Test 6: No match cases
    r = 0
    If "hello" Like "world" Then r = r + 1
    If "abc" Like "ab" Then r = r + 1
    If "ab" Like "abc" Then r = r + 1
    Debug.Print r         ' expect: 0

    Debug.Print 9999      ' sentinel
End Sub
