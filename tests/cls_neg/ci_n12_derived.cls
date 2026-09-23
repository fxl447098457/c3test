VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "Ov12Derived"
Option Explicit

Inherits Ov12Base

Public Overrides Function Ghost() As String
    Ghost = "ghost"
End Function
