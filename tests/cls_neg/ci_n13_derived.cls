VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "Ov13Derived"
Option Explicit

Inherits Ov13Base

Public Overrides Function Alone() As String
    Alone = "other"
End Function
