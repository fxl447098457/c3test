Attribute VB_Name = "mdl3060"
Option Explicit

' 账 #305 第一百九十四刀: knownClassVars_ 跨过程不清空 —— 同名局部在前一过程是工程类、
' 在后一过程是 Variant 时, Set 会走「工程类槽」那条腿 (__clsold / AddRef / Release) ⇒
' cl C2440/C2172。本条只声明、不 Set (登记发生在声明处), 作为「前一过程」的证人。
Private Sub SeedClassSlot()
    Dim leak As C3060
End Sub

Private Function LateObj() As Object
    Set LateObj = New C3060
End Function

Public Sub Main()
    SeedClassSlot
    ' 同名局部在这里是 Variant: Set 只许装箱 + 裸赋值, 不许出现 vb6_cls_C3060* __clsold。
    Dim leak As Variant
    Set leak = LateObj()
    Debug.Print "K3060-vt=" & VarType(leak)
    Debug.Print "K3060-DONE"
End Sub
