VERSION 5.00
Begin VB.Form frmChild 
   Caption         =   "MDI Child"
   ClientHeight    =   4000
   ClientLeft      =   60
   ClientTop       =   450
   ClientWidth     =   5000
   LinkTopic       =   "frmChild"
   MDIChild        =   -1  'True
   StartUpPosition =   3  'Windows Default
   Begin VB.CommandButton cmdClose 
      Caption         =   "Close"
      Height          =   375
      Left            =   1800
      TabIndex        =   0
      Top             =   1800
      Width           =   1215
   End
   Begin VB.Label lblInfo 
      Caption         =   "I am an MDI Child window"
      Height          =   255
      Left            =   1200
      Top             =   600
      Width           =   2415
   End
End
Attribute VB_Name = "frmChild"

Private Sub cmdClose_Click()
    Dim x As Integer
    x = 0
End Sub

Private Sub Form_Load()
    Dim y As Integer
    y = 99
End Sub
