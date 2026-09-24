VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "FoldBase"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = True
Attribute VB_PredeclaredId = False
Attribute VB_Exposed = False
Option Explicit

' ai/022 B11/C04 (ai/026 section 6, D52): a VB6 class module that never mentions CoClass.
' The four header attribute lines below are the only identity information in the file, and
' they must fold into exactly one CoClass record -- which Pass E then solves, so legacy
' projects and hand-written blocks share a single identity outlet.
'
' Two things this fixture pins down the hard way:
'  * VB_Creatable = True in an EXE project must stay SILENT. Hand-written blocks get
'    VB3033 for that (C03a), but folded records are read-only information (D52-2) --
'    refusing here would kill projects that compiled fine before this batch existed.
'  * the dotted lines are member-scope metadata, not header attributes. They must never
'    be folded (the corpus has 40+ of those names).
Attribute mRpm.VB_Description = "rotations per minute"
Attribute FoldBase.VB_VarHelpID = -1

Private mRpm As Long

Public Sub Spin(rpm As Long)
    mRpm = rpm
End Sub
