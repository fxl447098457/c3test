Option Explicit
Public Sub Main()
    Dim s As String
    Dim i As Long
    Console.Write "IN> "
    s = Console.ReadLine()
    For i = 1 To Len(s)
        Console.WriteLine "U+" & Hex$(AscW(Mid$(s, i, 1)))
    Next i
End Sub
