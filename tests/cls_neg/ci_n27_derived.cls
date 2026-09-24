VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "Mb27Derived"
Option Explicit

Inherits Mb27Base

' The pair partner for ci_n27: a normal derived class with no MyBase use, so the only
' diagnostic in this project is the base class's own VB3028.
Public Function Shadow() As String
    Shadow = Me.Speak() & "/derived"
End Function
