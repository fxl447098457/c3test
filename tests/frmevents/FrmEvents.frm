VERSION 5.00
Begin VB.Form FrmEvents
   Caption         =   "FrmEvents"
   ClientHeight    =   2400
   ClientLeft      =   120
   ClientTop       =   465
   ClientWidth     =   4800
   LinkTopic       =   "Form1"
   ScaleHeight     =   2400
   ScaleWidth      =   4800
   StartUpPosition =   3  '窗口缺省
   Begin VB.Timer tmrDrive
      Interval        =   150
      Enabled         =   -1  'True
   End
End
Attribute VB_Name = "FrmEvents"
Option Explicit

' P20-43: 窗体事件面补齐的实测夹具。
' 事件能不能"被调用"只有在运行期才知道, 所以用 Timer 驱动一轮"事件采样":
' 驱动鼠标/键盘/焦点/Paint 需要真实输入, 这里能自动驱动的就断言,
' 不能自动驱动的 (鼠标/键盘/焦点/Paint/拖放/DDE) 只验证"链接进去了" —— 也就是
' 编译期就发对了调用 (缺了会 LNK2019), 运行期由交互式使用兜底。
' 自动驱动路径: Initialize → Load → Resize → Activate → (Timer) → QueryUnload
'             → Unload → Terminate。

Private Sub Form_Initialize()
    Debug.Print "EV01-INIT"
End Sub

Private Sub Form_Load()
    Debug.Print "EV02-LOAD"
End Sub

Private Sub Form_Resize()
    ' 首次 Resize 在 Load 完成后才补发 (Fix 112), 所以这里必然在 LOAD 之后
    Debug.Print "EV03-RESIZE"
End Sub

Private Sub Form_Activate()
    Debug.Print "EV04-ACTIVATE"
End Sub

Private Sub Form_Paint()
    ' WM_PAINT 可能来多次, 只打一次标记避免刷屏
    Static done As Integer
    If done = 0 Then
        done = 1
        Debug.Print "EV05-PAINT"
    End If
End Sub

Private Sub Form_GotFocus()
    Static done As Integer
    If done = 0 Then
        done = 1
        Debug.Print "EV06-GOTFOCUS"
    End If
End Sub

Private Sub Form_Click()
    Debug.Print "EV07-CLICK"
End Sub

Private Sub Form_DblClick()
    Static done As Integer
    If done = 0 Then
        done = 1
        Debug.Print "EV08-DBLCLICK-LINKED"
    End If
End Sub

Private Sub Form_MouseDown(Button As Integer, Shift As Integer, X As Single, Y As Single)
    Static done As Integer
    If done = 0 Then
        done = 1
        Debug.Print "EV09-MOUSEDOWN-LINKED"
    End If
End Sub

Private Sub Form_MouseMove(Button As Integer, Shift As Integer, X As Single, Y As Single)
    Static done As Integer
    If done = 0 Then
        done = 1
        Debug.Print "EV10-MOUSEMOVE-LINKED"
    End If
End Sub

Private Sub Form_MouseUp(Button As Integer, Shift As Integer, X As Single, Y As Single)
    Static done As Integer
    If done = 0 Then
        done = 1
        Debug.Print "EV11-MOUSEUP-LINKED"
    End If
End Sub

Private Sub Form_KeyDown(KeyCode As Integer, Shift As Integer)
    Static done As Integer
    If done = 0 Then
        done = 1
        Debug.Print "EV12-KEYDOWN-LINKED"
    End If
End Sub

Private Sub Form_KeyPress(KeyAscii As Integer)
    Static done As Integer
    If done = 0 Then
        done = 1
        Debug.Print "EV13-KEYPRESS-LINKED"
    End If
End Sub

Private Sub Form_KeyUp(KeyCode As Integer, Shift As Integer)
    Static done As Integer
    If done = 0 Then
        done = 1
        Debug.Print "EV14-KEYUP-LINKED"
    End If
End Sub

Private Sub Form_Deactivate()
    Static done As Integer
    If done = 0 Then
        done = 1
        Debug.Print "EV15-DEACTIVATE-LINKED"
    End If
End Sub

Private Sub Form_LostFocus()
    Static done As Integer
    If done = 0 Then
        done = 1
        Debug.Print "EV16-LOSTFOCUS-LINKED"
    End If
End Sub

Private Sub Form_DragDrop(Source As Control, X As Single, Y As Single)
    Static done As Integer
    If done = 0 Then
        done = 1
        Debug.Print "EV17-DRAGDROP-LINKED"
    End If
End Sub

Private Sub Form_DragOver(Source As Control, X As Single, Y As Single, State As Integer)
    Static done As Integer
    If done = 0 Then
        done = 1
        Debug.Print "EV18-DRAGOVER-LINKED"
    End If
End Sub

Private Sub tmrDrive_Timer()
    Static ticks As Integer
    ticks = ticks + 1
    If ticks = 1 Then
        Debug.Print "EV19-TIMER-FIRED"
    ElseIf ticks >= 2 Then
        tmrDrive.Enabled = False
        Debug.Print "EV20-LOOPDONE"
        Unload Me
    End If
End Sub

Private Sub Form_QueryUnload(Cancel As Integer, UnloadMode As Integer)
    Debug.Print "EV21-QUERYUNLOAD"
End Sub

Private Sub Form_Unload(Cancel As Integer)
    Debug.Print "EV22-UNLOAD"
End Sub

Private Sub Form_OLEDragDrop(Data As DataObject, Effect As Long, Button As Integer, Shift As Integer, X As Single, Y As Single)
    Debug.Print "OLE-DROP"
End Sub

Private Sub Form_Terminate()
    Debug.Print "EV23-TERMINATE"
End Sub
