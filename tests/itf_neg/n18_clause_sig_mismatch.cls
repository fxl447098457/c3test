VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "WrongSig"
Option Explicit

Interface IShape
    Function Area(ByVal k As Long) As Double
End Interface

Implements IShape

' Explicit binding does not relax the signature comparison
Private Function Cover() As Double Implements IShape.Area
    Cover = 1#
End Function
