Attribute VB_Name = "IdIfaces"
Option Explicit

' The three contracts the CoClass blocks below reference. ITagged carries an explicit
' [InterfaceId] so the IID of a CoClass that defaults to it comes from the declaration
' rather than from the deterministic mint.

[InterfaceId("{22222222-3333-4444-5555-666666666666}")]
Interface ITagged
    Sub Paint(mode As Long)
End Interface

Interface IShape
    Sub Move(dx As Long, dy As Long)
End Interface

Interface INamed
    Property Get Label() As String
End Interface
