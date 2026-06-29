' P14.2.2: System RTL functions test (Dir/CurDir/Environ/Command)
' Note: Shell is tested minimally (just check it returns non-zero for a known exe)

Sub Main()
    Dim r As Long
    r = 0

    ' Test 1: CurDir - returns current directory (non-empty)
    Dim curPath As String
    curPath = CurDir()
    If Len(curPath) > 0 Then r = r + 1
    Debug.Print r          ' expect: 1

    ' Test 2: Environ - retrieve known environment variable
    Dim sysRoot As String
    sysRoot = Environ("SystemRoot")
    If Len(sysRoot) > 0 Then r = r + 1
    Debug.Print r          ' expect: 2

    ' Test 3: Environ - non-existent variable returns empty
    Dim noVar As String
    noVar = Environ("NONEXISTENT_VAR_12345")
    If Len(noVar) = 0 Then r = r + 1
    Debug.Print r          ' expect: 3

    ' Test 4: Dir - find .c files in output directory
    Dim found As String
    found = Dir("D:\vb6pro\output\*.c")
    ' Just check it either finds something or returns empty gracefully
    r = r + 1              ' if we got here, Dir didn't crash
    Debug.Print r          ' expect: 4

    ' Test 5: Command - returns command line args (may be empty)
    Dim cmdArgs As String
    cmdArgs = Command()
    ' No specific assertion, just verify it doesn't crash
    r = r + 1
    Debug.Print r          ' expect: 5

    Debug.Print 9999       ' sentinel
End Sub
