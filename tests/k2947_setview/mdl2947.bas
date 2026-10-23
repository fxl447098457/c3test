Attribute VB_Name = "mdl2947"
Option Explicit

' 账 #305 站F 写侧 (第一百八十五~一百八十七刀): Set / 多入口参数 / 对象属性读 在视图上都走槽表。
Sub Main()
    Dim x As IK2947
    Dim s As String
    Dim o As Object
    Set x = New H2947
    Set x.Owner = New O2947
    Debug.Print "K2947-a=" & x.Has()
    Set o = x.Owner
    Debug.Print "K2947-o=" & (o Is Nothing)
    Set x.Owner = Nothing
    Debug.Print "K2947-b=" & x.Has()
    Set o = x.Owner
    Debug.Print "K2947-p=" & (o Is Nothing)
    x.Item(3) = "a"
    s = x.Item(3)
    Debug.Print "K2947-i=" & s
    Debug.Print "K2947-DONE"
End Sub
