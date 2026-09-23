VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "EvtBase"
Option Explicit

Event Boom(ByVal v As Long)

Public Sub Raise()
    RaiseEvent Boom(1)
End Sub
