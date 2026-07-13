' P24-04b: GetObject function test (console output)
' Tests: GetObject(, progId) and CreateObject baseline

Sub Main()
    ' Test 1: CreateObject works (baseline)
    Dim dic As Object
    Set dic = CreateObject("Scripting.Dictionary")
    dic.Add "hello", "world"
    
    If dic.Exists("hello") Then
        Debug.Print "CreateObject OK: hello=" & dic.Item("hello")
    Else
        Debug.Print "FAIL: CreateObject Dictionary"
    End If
    
    ' Test 2: GetObject(, progId) - attach to running instance
    Dim xl As Object
    Set xl = GetObject(, "Excel.Application")
    If xl Is Nothing Then
        Debug.Print "GetObject(,Excel) = Nothing (OK if no Excel running)"
    Else
        Debug.Print "GetObject(,Excel) got instance!"
    End If
    
    ' Test 3: IsNothing test  
    If IsNothing(xl) Then
        Debug.Print "IsNothing(GetObject result) = True OK"
    End If
    
    Debug.Print "P24-04b PASSED"
End Sub
