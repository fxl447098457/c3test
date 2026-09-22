Attribute VB_Name = "GmodA"
Option Explicit

' 跨模块泛型 (gen_xmod): A 模块导出泛型 UDT + 泛型 Function/Sub,
' B 模块用显式实例化与裸调推断两种方式使用 (fixpoint 物化 + 跨模块注入).

Public Type GBox(Of T)
    V As T
    Tag As String
End Type

Public Function Pick(Of T)(arr() As T, idx As Long) As T
    Pick = arr(LBound(arr) + idx)
End Function

Public Function Sum2(Of T)(a As T, b As T) As T
    Sum2 = a + b
End Function

Public Sub ShowLen(Of T)(arr() As T)
    Debug.Print "XLEN="; (UBound(arr) - LBound(arr) + 1)
End Sub
