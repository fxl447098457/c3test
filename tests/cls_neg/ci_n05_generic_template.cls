VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "GenCell"
Option Explicit
Class GenCell(Of T)

Inherits PlainBase

Private m_v As T

Public Function Value() As T
    Value = m_v
End Function
