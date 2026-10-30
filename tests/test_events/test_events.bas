Option Explicit

Dim WithEvents btn As Button

Dim g_ClickX As Long
Dim g_ClickY As Long
Dim g_ResizeW As Long
Dim g_ResizeH As Long

Sub Main()
    Set btn = New Button
    
    btn.SetCaption "TestButton"
    
    btn.DoClick 100, 200
    btn.DoResize 800, 600
    
    Debug.Print "ClickX="; g_ClickX
    Debug.Print "ClickY="; g_ClickY
    Debug.Print "ResizeW="; g_ResizeW
    Debug.Print "ResizeH="; g_ResizeH
    
    If g_ClickX = 100 And g_ClickY = 200 And g_ResizeW = 800 And g_ResizeH = 600 Then
        Debug.Print "Events test PASSED"
    Else
        Debug.Print "Events test FAILED"
    End If
    Dim M As String
    M = btn.RenderMarkup()
    Debug.Print "Markup="; M
    If M = "NO ITEMS|CENTERED" Then
        Debug.Print "EvtRef test PASSED"
    Else
        Debug.Print "EvtRef test FAILED"
    End If
End Sub

Sub btn_Click(ByVal X As Long, ByVal Y As Long)
    g_ClickX = X
    g_ClickY = Y
End Sub

Sub btn_Resize(ByVal Width As Long, ByVal Height As Long)
    g_ResizeW = Width
    g_ResizeH = Height
End Sub
' 账 #305 256: 处理器写两个出参 —— 值要回到 Button.RenderMarkup 里那句 RaiseEvent 之后。
Sub btn_GetEmptyMarkup(ByRef Text As String, ByRef Center As Boolean)
    Text = "NO ITEMS"
    Center = True
End Sub
