Option Explicit

' R2 的钉子: 孔里的表达式就是普通表达式, 撞不上作用域里的名字 ⇒ 编译期是响的,
' 不是运行期静默换掉。
Sub Main()
    Dim s As String
    s = `v=${nopeHere}`
    Debug.Print s
End Sub
