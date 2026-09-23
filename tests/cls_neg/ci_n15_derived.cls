VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "Ov15Derived"
Option Explicit

Inherits Ov15Base

Public Overrides Function Pick() As String
    Pick = "derived"
End Function
