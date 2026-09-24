VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "FoldVbp"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = True
Attribute VB_PredeclaredId = False
Attribute VB_Exposed = False
Option Explicit

' ai/022 B11/C04: this class writes no CoClass block. Its CLSID lives in the .vbp as the
' VB6 three-part entry (Class=FoldVbp; FoldVbp.cls; {7777...}); the header attributes above
' are the rest of its identity. Folding puts both in front of the SAME resolver, so the vbp
' tier has to light up for a module that never mentions CoClass -- the proof that there is
' only one identity channel left in the compiler (D47's rule, D52-1's target).
Private mTag As String

Public Property Get Tag() As String
    Tag = mTag
End Property

Public Property Let Tag(ByVal v As String)
    mTag = v
End Property
