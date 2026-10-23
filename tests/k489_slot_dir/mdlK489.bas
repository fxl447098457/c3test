Attribute VB_Name = "mdlK489"
Option Explicit

' Target shape for 489: the interface host declares `Property Let Label` BEFORE
' `Property Get Label`. Before the fix the minted table had ONE slot whose shape came
' from Let while the bound function came from Get (silent mismatch), and the view's
' own read of that name fell back to the late helper = cl C2172.
Sub Main()
    Dim x As IK489
    Dim n As Long
    Dim s As String
    Set x = New K489Holder
    s = x.Label
    n = x.Num
    Debug.Print "K489-s=" & s
    Debug.Print "K489-n=" & n
    Debug.Print "K489-DONE"
End Sub
