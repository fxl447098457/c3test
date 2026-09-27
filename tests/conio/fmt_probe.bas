Option Explicit
Public Sub Main()
    Dim n As Long
    Dim d As Double
    Dim dt As Date
    n = 1234567
    d = 1234.5678
    dt = DateSerial(2026, 9, 27)
    Console.WriteLine `a=${n:#,##0}`
    Console.WriteLine `b=${d:0.000}`
    Console.WriteLine `c=${n:00000000}`
    Console.WriteLine `e=${dt:yyyy-mm-dd}`
    Console.WriteLine `f=${dt:mmm d, yyyy}`
    Console.WriteLine `g=${dt:hh:nn:ss}`
    Console.WriteLine `h=${d:Currency}`
    Console.WriteLine `i=${d:Scientific}`
    Console.WriteLine `j=${d:Percent}`
End Sub
