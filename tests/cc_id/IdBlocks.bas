Attribute VB_Name = "IdBlocks"
Option Explicit

' ai/022 B11/C02: one block per identity tier (ai/026 section 3), all three resolved by the
' single function in src/semantics/coclass_identity.cpp.

' Tier 1 -- every identity written out explicitly, so nothing is derived from the project.
CoClass CCCircle
    [CoClassId("{11111111-1111-1111-1111-111111111111}")]
    [ProgId("Shapes.Circle")]
    [ComCreatable(True)]
    [Implementation("CircleImpl")]
    Interface IShape
    [Default] Interface ITagged
End CoClass

' Tier 2 -- CLSID comes from the vbp three-part entry that names the implementation module.
CoClass CCVbp
    [Implementation("VbpImpl")]
    Interface IShape
    [Default] Interface INamed
End CoClass

' Tier 3 -- nothing written: both GUIDs are minted from the project name.
CoClass CCMint
    Interface IShape
    [Default] Interface INamed
End CoClass
