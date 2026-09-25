VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "ProtPos2Derived"
Option Explicit

Inherits ProtPos2Base

Public Function Lift(ByVal o As ProtPos2Base) As Long
    Lift = o.m_secret
End Function

Public Function Mine() As Long
    Mine = Me.m_secret
End Function
