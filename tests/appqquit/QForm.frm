VERSION 5.00
Begin VB.Form QForm 
   Caption         =   "QForm"
   ClientHeight    =   1200
   ClientLeft      =   120
   ClientTop       =   465
   ClientWidth     =   2400
   ScaleHeight     =   1200
   ScaleWidth      =   2400
   Begin VB.Timer t1 
      Enabled         =   -1   'True
      Interval        =   40
      Left            =   120
      Top             =   120
   End
End
Attribute VB_Name = "QForm"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = False
Attribute VB_Exposed = False
Option Explicit

' Ledger 259. VB6 ends the process when the last form unloads. The RTL does that with ONE
' WM_QUIT, posted by vb6_Forms_Unregister, and a thread only ever gets one: whoever dequeues
' it owns the exit. An inner pump that takes it (DoEvents, or a modal Show loop) leaves the
' outer GetMessage waiting forever with no window left, so the process just sits there.
'
' Not theoretical: gate #395's only red was combofocus_x86 writing all seven judgment lines
' and then never exiting (60 s, 62 ms CPU, no window). On a starved machine tick 6's DoEvents
' pulls the already-queued WM_TIMER for tick 7, so tick 7 -- the one that unloads -- runs
' INSIDE tick 6's pump, and the quit posted there is eaten by that same pump.
'
' This fixture makes that shape deterministic instead of waiting for a loaded machine: every
' tick sleeps 80 ms while the interval is 40 ms, so a WM_TIMER is always already queued when
' DoEvents starts, which puts the unloading tick inside an inner pump.
'
'   Q-DONE                         the judgments were all written
'   Q-ORDER=unload-inside-doevents  witness that the unload really happened inside the pump
'                                   (without it a build that never reaches the hole would
'                                    still pass)
'   the process exiting on its own  is the actual judgment -- run_tests.ps1 fails a case on a
'                                   run timeout, so a swallowed quit is red by itself
' ASCII only.

Private Declare PtrSafe Sub Sleep Lib "kernel32" (ByVal dwMilliseconds As Long)

Private gTick As Long
Private gUnloaded As Long

Private Sub t1_Timer()
    Dim my As Long
    my = gTick + 1
    gTick = my
    If my <= 6 Then
        Sleep 80
        DoEvents
        If my = 6 Then
            If gUnloaded = 1 Then
                Debug.Print "Q-ORDER=unload-inside-doevents"
            Else
                Debug.Print "Q-ORDER=unload-outside"
            End If
        End If
        Exit Sub
    End If
    If my = 7 Then
        Debug.Print "Q-DONE"
        t1.Enabled = False
        gUnloaded = 1
        Unload Me
    End If
End Sub
