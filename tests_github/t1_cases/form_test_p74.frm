VERSION 5.00
Begin VB.Form FormTest
   Caption         =   "P7.4 Test - DoEvents & Beep"
   ClientHeight    =   3600
   ClientLeft      =   60
   ClientTop       =   390
   ClientWidth     =   4800
   StartUpPosition =   3
   Begin VB.CommandButton cmdDoEvents
      Caption         =   "DoEvents Test"
      Height          =   495
      Left            =   480
      Top             =   360
      Width           =   2175
   End
   Begin VB.CommandButton cmdBeep
      Caption         =   "Beep"
      Height          =   495
      Left            =   2880
      Top             =   360
      Width           =   1455
   End
   Begin VB.Label lblInfo
      Caption         =   "Click buttons to test"
      Height          =   375
      Left            =   480
      Top             =   1200
      Width           =   3855
   End
End
Attribute VB_Name = "FormTest"

Dim clickCount As Long

Private Sub Form_Load()
    clickCount = 0
End Sub

Private Sub cmdDoEvents_Click()
    Dim i As Long
    For i = 1 To 5
        DoEvents
    Next i
    clickCount = clickCount + 1
End Sub

Private Sub cmdBeep_Click()
    Beep
End Sub

Private Sub Form_Unload(Cancel As Integer)
    Cancel = 0
End Sub
