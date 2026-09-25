VERSION 1.0 CLASS
BEGIN
  MultiUse = -1
END
Attribute VB_Name = "PkgThing"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = True
Attribute VB_PredeclaredId = False
Option Explicit

Private mVal As Long

Public Sub Setup(ByVal v As Long)
    mVal = v
End Sub

Public Property Get Value() As Long
    Value = mVal * 2
End Property
