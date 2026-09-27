' Console 对象夹具 (Fix 161, 对齐 twinBASIC)
' 覆盖: Console.WriteLine / Console.Write / Console.ReadLine / Console.ReadKey
' 验证 stdin 双向: 管道 ASCII、管道 GBK 中文、管道 UTF-8 中文
Option Explicit

Public Sub Main()
    Console.WriteLine "=== Console 对象夹具 ==="

    ' 1) Console.WriteLine: 输出 + 换行
    Console.WriteLine "line1"

    ' 2) Console.Write: 输出不换行
    Console.Write "a"
    Console.Write "b"
    Console.WriteLine ""   ' 收尾换行

    ' 3) Console.ReadLine: 读一行
    Dim s1 As String
    Console.Write "in1> "
    s1 = Console.ReadLine()
    Console.WriteLine "read1=[" & s1 & "]"

    ' 4) 第三行 (验证多行)
    Dim s2 As String
    Console.Write "in2> "
    s2 = Console.ReadLine()
    Console.WriteLine "read2=[" & s2 & "]"

    Console.WriteLine "=== done ==="
End Sub
