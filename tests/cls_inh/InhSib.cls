VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "InhSib"
Option Explicit

Inherits InhBase

' ai/022 B08d: a SECOND branch off the same base. InhBase's view must stay a prefix of both
' InhSib's and InhDerived's tables, and each instance dispatches on its own table.
Public Overrides Function Speak() As String
    Speak = "sibling"
End Function

' ai/022 B09c: the *deepest* holder of the root's Private UDT (TPoint is declared in InhBase,
' two levels above this class) - B09b injects the type name along the whole chain, so an
' inherited UDT field must be laid out right here too. Unlike InhDerived, this class adds no own
' field, so a wrong stride at that offset shows up as corruption instead of a benign overlap.
Private Type SibWrap
    h As InhBase
End Type

' ai/022 B09c: read/write through a UDT object field whose declared type is an ANCESTOR
' (InhBase) while the instance is this class. Level has no slot on THIS branch (only InhDerived
' overrides it), so both directions bind to the root's accessors -> 4 in, 4 out; Speak is a slot
' this branch overrides, so it dispatches -> "sibling". Both halves must hold for the holder's
' field offset and the per-branch table to be right.
Public Function PropWriteViaUdt(ByVal v As Long) As String
    Dim u As SibWrap
    Set u.h = Me
    u.h.Level = v
    PropWriteViaUdt = CStr(u.h.Level) & "/" & u.h.Speak()
End Function

' ai/022 B09c: MyBase.Class_Initialize on a 2-level chain must run THIS class's immediate base
' (InhBase), not the leaf's own initializer.
Public Sub InitRerun()
    MyBase.Class_Initialize
End Sub

Private Sub Class_Initialize()
    Me.g_init = Me.g_init & "sib;"
End Sub
