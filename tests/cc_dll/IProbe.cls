VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "IProbe"
Option Explicit

Interface IProbe
    Sub Ping(ByVal n As Long)
    Property Get Got() As Long
End Interface
