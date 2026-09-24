Attribute VB_Name = "ProtNegMain"
Option Explicit

' 084a 负例: 非家族经 obj. 访问 Protected 成员 → 3023
Sub Main()
    Dim b As ProtBase
    Set b = New ProtBase
    b.Tag "stranger"
    Debug.Print "NEVER"
End Sub
