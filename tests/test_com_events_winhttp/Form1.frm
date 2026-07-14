VERSION 5.00
Begin VB.Form Form1
   Caption         =   "WinHTTP COM Event Test"
   ClientHeight    =   3195
   ClientLeft      =   120
   ClientTop       =   465
   ClientWidth     =   4680
   LinkTopic       =   "Form1"
   ScaleHeight     =   3195
   ScaleWidth      =   4680
   StartUpPosition =   3  '窗口缺省
End
Attribute VB_Name = "Form1"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit

' P24-12: 第三方 COM 事件触发回归测试
' 使用 Windows 内置 WinHTTP 组件验证 WithEvents / 异步事件是否被正确触发

Dim WithEvents Http As WinHttpRequest
Attribute Http.VB_VarHelpID = -1

Private m_LogFile As String

Private Sub Form_Load()
    m_LogFile = App.Path & "\winhttp_event_log.txt"
    On Error Resume Next: Kill m_LogFile: On Error GoTo 0

    LogWrite "=== WinHTTP COM Event Test ==="
    LogWrite "Start: " & Now

    Set Http = New WinHttpRequest

    Http.Open "GET", "http://www.example.com/", True
    LogWrite "Request opened"
    Http.Send
    LogWrite "Request sent, waiting for events..."
End Sub

Private Sub Http_OnResponseStart(ByVal Status As Long, ByVal ContentType As String)
    LogWrite "OnResponseStart Status=" & Status & " ContentType=" & ContentType
End Sub

Private Sub Http_OnResponseDataAvailable(Data() As Byte)
    LogWrite "OnResponseDataAvailable"
End Sub

Private Sub Http_OnResponseFinished()
    LogWrite "OnResponseFinished Status=" & Http.Status
    LogWrite "Done: " & Now
    LogWrite "PASS"
    End
End Sub

Private Sub Http_OnError(ByVal ErrorNumber As Long, ByVal ErrorDescription As String)
    LogWrite "OnError " & ErrorNumber & ": " & ErrorDescription
    LogWrite "Done: " & Now
    LogWrite "FAIL"
    End
End Sub

Private Sub Http_OnTimeout()
    LogWrite "OnTimeout"
    LogWrite "Done: " & Now
    LogWrite "FAIL"
    End
End Sub

Private Sub LogWrite(ByVal Msg As String)
    Dim f As Integer
    f = FreeFile
    Open m_LogFile For Append As #f
    Print #f, Msg
    Close #f
End Sub
