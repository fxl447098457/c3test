VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "ProtDerived"
Option Explicit

Inherits ProtBase

' 084a: 家族内访问 Protected 成员 (Me. 与 obj. 皆合法)
Public Sub Touch()
    Me.Tag "fam"
    Dim b As ProtBase
    Set b = New ProtBase
    b.Tag "fam2"
End Sub
