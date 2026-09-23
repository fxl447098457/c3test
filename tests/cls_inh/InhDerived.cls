VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "InhDerived"
Option Explicit

Inherits InhBase

Public Function Name2() As String
    Name2 = "derived"
End Function
