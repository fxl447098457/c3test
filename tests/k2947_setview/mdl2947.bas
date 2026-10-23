Attribute VB_Name = "mdl2947"
Option Explicit

' 账 #305 站F 写侧 (第一百八十五/一百八十六刀): Set 与多入口参数写在视图上都走写槽。
Sub Main()
    Dim x As IK2947
    Dim s As String
    Set x = New H2947
    Set x.Owner = New O2947
    Debug.Print "K2947-a=" & x.Has()
    Set x.Owner = Nothing
    Debug.Print "K2947-b=" & x.Has()
    x.Item(3) = "a"
    s = x.Item(3)
    Debug.Print "K2947-i=" & s
    Debug.Print "K2947-DONE"
End Sub
