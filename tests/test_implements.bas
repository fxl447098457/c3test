Option Explicit

Sub Main()
    Dim r As CRectangle
    Set r = New CRectangle
    r.Width = 3#
    r.Height = 4#

    Dim s As IShape
    Set s = r

    Debug.Print "Area = "; s.Area()
    Debug.Print "Perimeter = "; s.Perimeter()
    s.Describe()

    Dim a As Double
    a = s.Area()
    If a = 12# Then
        Debug.Print "IMPL1:OK"
    Else
        Debug.Print "IMPL1:FAIL"
    End If

    Dim p As Double
    p = s.Perimeter()
    If p = 14# Then
        Debug.Print "IMPL2:OK"
    Else
        Debug.Print "IMPL2:FAIL"
    End If

    Debug.Print "Implements test PASSED"
End Sub