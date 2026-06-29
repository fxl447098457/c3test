' P14.1.2 + P14.1.3: Resume and Error statement test
' Tests: On Error GoTo + Resume Next, On Error Resume Next, Error stmt, Resume label
' Note: hardware exceptions (div by zero) not caught by setjmp/longjmp
' Only vb6_RaiseError (via Error stmt or RTL) can be caught

Sub Main()
    Dim result As Long
    
    ' Test 1: On Error GoTo + Resume Next
    ' Resume Next goes to the statement AFTER the error-causing statement
    result = 0
    On Error GoTo Handler1
    Error 11                 ' error-causing stmt; result=0 here
    result = result + 1      ' Resume Next goes here; result becomes (handler value + 1)
    GoTo Done1
Handler1:
    result = 10              ' handler: set result=10
    Resume Next              ' go to statement after Error 11 -> result = result + 1
Done1:
    Debug.Print result       ' expect: 11 (10 from handler + 1 from Resume Next target)
    
    ' Test 2: On Error Resume Next
    On Error Resume Next
    Error 13                 ' error silently ignored, continue to next stmt
    result = 100             ' this should execute normally
    Debug.Print result       ' expect: 100
    
    ' Test 3: On Error GoTo + Resume label
    ' Resume label jumps directly to the specified label
    Dim errCaught As Long
    errCaught = 0
    On Error GoTo Handler3
    Error 55                 ' raise error 55
    errCaught = -1           ' should NOT execute (Resume label skips past it)
    GoTo Done3
Handler3:
    errCaught = 1            ' caught the error
    Resume Done3             ' jump directly to Done3, skipping errCaught = -1
Done3:
    Debug.Print errCaught    ' expect: 1 (Resume label skips errCaught = -1)
    
    Debug.Print 9999         ' sentinel: all tests passed
End Sub
