Attribute VB_Name = "XmodPkg"
Option Explicit

Private mCounter As Long

Public Function Add2(ByVal a As Long, ByVal b As Long) As Long
    mCounter = mCounter + 1
    Add2 = a + b + mCounter - 1
End Function

Public Property Get Tag() As String
    Tag = "XM"
End Property

Friend Sub Hidden1()
    Debug.Print "PKG-HIDDEN"
End Sub
