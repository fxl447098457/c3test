Attribute VB_Name = "ActMain"
Option Explicit

' ai/022 B11/C05 = the in-project activation of a CoClass name: `As Circle`, `New Circle`
' and `CreateObject("ActApp.Circle")` all bind to the block's [Implementation] class.
' CC2/CC3 put the group name in a field, a parameter and a return type as well, because
' those are separate positions in the AST that the rewrite has to reach.

Private m_saved As Circle

Function MakeCircle() As Circle
    Dim t As Circle
    Set t = New Circle
    Set MakeCircle = t
End Function

Sub UseCircle(c As Circle)
    c.Move 1
End Sub

Sub Main()
    Dim c As Circle
    Set c = New Circle
    c.Move 2
    If c.Area() = 6# Then
        Debug.Print "CC1:OK"
    Else
        Debug.Print "CC1:FAIL area=" & c.Area()
    End If

    Set m_saved = MakeCircle()
    m_saved.Move 4
    UseCircle m_saved
    If m_saved.Area() = 12# Then
        Debug.Print "CC2:OK"
    Else
        Debug.Print "CC2:FAIL area=" & m_saved.Area()
    End If

    Dim c2 As Circle
    Set c2 = c
    c2.Move 10
    If c.Area() = 26# Then
        Debug.Print "CC3:OK"
    Else
        Debug.Print "CC3:FAIL area=" & c.Area()
    End If

    Dim c4 As Circle
    Set c4 = CreateObject("ActApp.Circle")
    c4.Move 5
    If c4.Area() = 12# Then
        Debug.Print "CC4:OK"
    Else
        Debug.Print "CC4:FAIL area=" & c4.Area()
    End If

    Dim o5 As Object
    Set o5 = CreateObject("ActApp.Ring")
    If o5 Is Nothing Then
        Debug.Print "CC5:FAIL nothing"
    Else
        Debug.Print "CC5:OK"
    End If

    Dim r As Ring
    Set r = New Ring
    r.Move 1
    If r.Area() = -1# Then
        Debug.Print "CC6:OK"
    Else
        Debug.Print "CC6:FAIL area=" & r.Area()
    End If

    Dim c7 As Circle
    Set c7 = CreateObject("actapp.circle")
    c7.Move 1
    If c7.Area() = 4# Then
        Debug.Print "CC7:OK"
    Else
        Debug.Print "CC7:FAIL area=" & c7.Area()
    End If

    If c.Area() = 26# Then
        Debug.Print "CC8:OK"
    Else
        Debug.Print "CC8:FAIL area=" & c.Area()
    End If

    Dim s As ShapeAct
    Set s = c
    If s.Area() = 26# And s.Side() = 13# Then
        Debug.Print "CC9:OK"
    Else
        Debug.Print "CC9:FAIL area=" & s.Area() & " side=" & s.Side()
    End If

    Debug.Print "CC-DONE"
End Sub
