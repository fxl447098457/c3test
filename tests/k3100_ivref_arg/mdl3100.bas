Attribute VB_Name = "mdl3100"
Option Explicit

Private g_val As Long
Private g_skip As Long

Private Sub Take(ByRef x As IK3100)
    g_val = g_val + 1
End Sub

Private Sub TakeVal(ByVal x As IK3100)
    g_val = 1
End Sub

Private Function MakeI() As IK3100
    Set MakeI = New H3100
End Function

Public Sub Main()
    Dim c As H3100
    Set c = New H3100
    Take MakeI()
    TakeVal c
    If g_skip Then Take c
    Debug.Print "K3100-h=" & c.Hits
    Debug.Print "K3100-v=" & g_val
    Debug.Print "K3100-DONE"
End Sub
