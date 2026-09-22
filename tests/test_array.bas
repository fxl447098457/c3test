' P4 Array Test - SAFEARRAY support
Option Explicit

Public Sub Main()
    Debug.Print "=== Array Tests ==="
    
    ' 静态数组 Dim arr(5) As Long
    Dim arr(5) As Long
    
    ' 赋值
    Dim i As Long
    For i = 0 To 5
        arr(i) = i * 10
    Next i
    
    ' 读取
    Debug.Print "arr(0)="; arr(0)
    Debug.Print "arr(3)="; arr(3)
    Debug.Print "arr(5)="; arr(5)
    Debug.Print "UBound="; UBound(arr)
    Debug.Print "LBound="; LBound(arr)
    
    ' 指定下界的数组
    Dim scores(1 To 3) As Long
    scores(1) = 85
    scores(2) = 92
    scores(3) = 78
    Debug.Print "scores(2)="; scores(2)
    Debug.Print "scoresUB="; UBound(scores)
    Debug.Print "scoresLB="; LBound(scores)
    
    ' 字符串数组
    Dim names(2) As String
    names(0) = "Alice"
    names(1) = "Bob"
    names(2) = "Charlie"
    Debug.Print "names(1)="; names(1)
    
    ' Fix 170: VB6 **整体数组引用** `A()` (空括号) —— 左值赋整个数组, 右值取整个数组
    ' 原缺一维空括号 = VB6_SA_AT(elem, A, 0): 未 ReDim 时 data==NULL → 写 0 号元素
    ' 就是解引用 NULL (VBFlexGridDemo Common.bas:1479 `B() = Text` 启动崩溃)。
    Dim src() As Long
    ReDim src(1 To 3)
    src(1) = 11
    src(2) = 22
    src(3) = 33
    Dim dst() As Long
    dst() = src()
    src(2) = 999          ' 深拷贝: 之后改 src 不应影响 dst
    Debug.Print "wa-clone="; dst(2)
    Debug.Print "wa-ub="; UBound(dst())
    Dim s As String
    s = "AB"
    Dim bb() As Byte
    bb() = s              ' String -> Byte() (原始 UTF-16LE 字节)
    Debug.Print "wa-str="; bb(0)
    Dim v As Variant
    v = bb()              ' Byte() -> Variant (持有整个数组)
    Dim bb2() As Byte
    bb2() = v             ' Variant -> Byte()
    Debug.Print "wa-rt="; bb2(0)

    Debug.Print "=== Array Tests PASSED ==="
End Sub
