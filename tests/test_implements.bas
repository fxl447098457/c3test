Option Explicit

Sub Main()
    Dim r As CRectangle
    Set r = New CRectangle
    r.Width = 3#
    r.Height = 4#
    
    ' Use interface reference for polymorphism
    Dim s As IShape
    Set s = r
    
    Debug.Print "Area = "; s.Area()
    Debug.Print "Perimeter = "; s.Perimeter()
    s.Describe()
    
    Debug.Print "Implements test PASSED"
End Sub
