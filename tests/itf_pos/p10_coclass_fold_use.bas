Attribute VB_Name = "P10FoldUse"
Option Explicit

' ai/022 B11/C05 (D54-3): the name a folded record carries is ALWAYS a class module name, so
' handing it to the type path would put two rules on the same name at once. The alias table
' refuses folded records, which is why `As FoldBase` / `New FoldBase` below still mean the
' class exactly as they did before CoClass existed -- asserted by the absence of an
' "activated in-project" line for that name.

Sub UseFolded()
    Dim f As FoldBase
    Set f = New FoldBase
    f.Spin 3
End Sub
