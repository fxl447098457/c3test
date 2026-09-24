VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "FoldWins"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = True
Attribute VB_PredeclaredId = False
Attribute VB_Exposed = False
Option Explicit

' ai/022 B11/C04 (026 section 6): a module that carries BOTH shapes. The hand-written block
' wins and the legacy lines are reported as not folded -- "silently pick one" is the failure
' mode this case exists to catch. Note the contrast the two lines draw:
'   Attribute VB_Creatable = True   <- ignored, because a block was written
'   [ComCreatable(False)]           <- this is what reaches the identity record
CoClass FoldWins
    [ComCreatable(False)]
End CoClass
