VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "ShapeAct"
Option Explicit

' ai/022 B11/C05: the implementation class behind CoClass Circle. It does NOT write
' `Implements IShapeAct` -- a base that implements a new-style Interface cannot be
' inherited at all today (VB3022), and the contract check only needs the members
' (p08_coclass_base.cls established the same shape).

Private m_x As Double

Private Sub Class_Initialize()
    m_x = 1
End Sub

Public Overridable Sub Move(dx As Double)
    m_x = m_x + dx
End Sub

Public Overridable Function Area() As Double
    Area = m_x * 2#
End Function

Public Function Side() As Double
    Side = m_x
End Function
