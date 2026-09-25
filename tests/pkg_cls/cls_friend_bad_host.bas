Attribute VB_Name = "FrBadHost"
Option Explicit

' 084a M3 负例: 宿主经 obj. 访问包导出类的 Friend 成员 (清单 Friend=False) → 7008
Sub Main()
    Dim t As FrThing
    Set t = New FrThing
    t.Setup 5
    t.Bump
    Debug.Print "NEVER:" & CStr(t.Value())
End Sub
