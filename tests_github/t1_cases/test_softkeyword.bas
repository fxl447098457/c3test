' P12.6: Soft keyword disambiguation test
' Tests that soft keywords can be used as variable names
Sub Main()
    ' Test: Get as variable name (vs Get #1 file I/O)
    Dim Get As Long
    Get = 42
    Debug.Print "Get="; Get
    
    ' Test: Step as variable name (vs For...Step)
    Dim Step As Long
    Step = 5
    Debug.Print "Step="; Step
    
    ' Test: Name as variable name (vs Name old As new)
    Dim Name As String
    Name = "test"
    Debug.Print "Name="; Name
End Sub
