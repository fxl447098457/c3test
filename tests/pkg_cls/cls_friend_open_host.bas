Attribute VB_Name = "FrOpenHost"
Option Explicit

' 084a M3 正例: 清单 Friend=True 时包导出类的 Friend 成员对宿主可见
Sub Main()
    Dim t As FrThing
    Set t = New FrThing
    t.Setup 5
    t.Bump
    Debug.Print "FR-OPEN:" & CStr(t.Value())
End Sub
