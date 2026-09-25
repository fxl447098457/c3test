VERSION 1.0 CLASS
BEGIN
  MultiUse = -1
END
Attribute VB_Name = "FrThing"
Option Explicit

Private mVal As Long

Public Sub Setup(ByVal v As Long)
    mVal = v
End Sub

Public Property Get Value() As Long
    Value = mVal * 2
End Property

' 084a M3: Friend 成员 —— 包外经 obj. 是否可见取决于清单 [Export] Friend=
Friend Sub Bump()
    mVal = mVal + 1
End Sub
