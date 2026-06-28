' test_implements_qi.bas - P12.1 regression test
' Verify that Implements interfaces work with QueryInterface in ActiveX DLL
' (The QI support is in vb6comserver.c, this test validates VB6-side behavior)

Option Explicit

Implements IShape

Public Width As Double
Public Height As Double

Private Function IShape_Area() As Double
    IShape_Area = Width * Height
End Function

Private Function IShape_Perimeter() As Double
    IShape_Perimeter = 2# * (Width + Height)
End Function

Private Sub IShape_Describe()
    Debug.Print "Rectangle"
End Sub

Sub Main()
    Dim r As CRectangle
    Set r = New CRectangle
    r.Width = 3#
    r.Height = 4#

    ' Interface reference
    Dim s As IShape
    Set s = r

    Debug.Print "Area = "; s.Area()
    Debug.Print "Perimeter = "; s.Perimeter()
    s.Describe()

    Debug.Print "Implements QI test PASSED"
End Sub
