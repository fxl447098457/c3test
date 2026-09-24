VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "ProtPos2Base"
Option Explicit

Protected m_secret As Long

Public Function PeekOwn(ByVal o As ProtPos2Base) As Long
    PeekOwn = o.m_secret
End Function
