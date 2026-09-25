Attribute VB_Name = "AccNegMain"
Option Explicit

' 084a M2 负例: 标准模块经 obj. 访问 Private 过程 → 3028 硬错
Sub Main()
    Dim t As AccThing
    Set t = New AccThing
    t.Secret
    Debug.Print "NEVER"
End Sub
