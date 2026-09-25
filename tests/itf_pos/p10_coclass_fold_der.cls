VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "FoldDer"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = False
Attribute VB_Exposed = False
Option Explicit

' ai/022 B11/C04: FoldBase now also exists in coclassIds_ because it was folded. Using it as
' a base class must still resolve as a class -- the VB3020 "that is a CoClass block" wording
' is reserved for hand-written blocks (D52-3), and a folded name must never trigger it.
Inherits FoldBase

Public Sub Run()
    Me.Spin 7
End Sub
