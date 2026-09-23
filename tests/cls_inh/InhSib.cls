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
