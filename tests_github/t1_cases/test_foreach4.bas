' P12.4: For Each with different array types
Sub Main()
    ' Test 1: Long array
    Dim arr(1 To 5) As Long
    Dim n As Long
    Dim total As Long
    
    arr(1) = 10
    arr(2) = 20
    arr(3) = 30
    arr(4) = 40
    arr(5) = 50
    
    total = 0
    For Each n In arr
        total = total + n
    Next
    Debug.Print "Long For Each: "; total
    
    ' Test 2: String array
    Dim sArr(1 To 3) As String
    Dim s As String
    Dim sResult As String
    
    sArr(1) = "Hello"
    sArr(2) = " "
    sArr(3) = "World"
    
    sResult = ""
    For Each s In sArr
        sResult = sResult & s
    Next
    Debug.Print "String For Each: "; sResult
End Sub
