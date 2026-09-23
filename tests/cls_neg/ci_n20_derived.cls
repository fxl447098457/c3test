VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "Vt20Derived"
Option Explicit

Inherits Vt20Base

Public Overrides Function Pick() As String
    Pick = "derived"
End Function
