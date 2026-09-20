VERSION 5.00
Begin VB.Form Form1 
   Caption         =   "P7.6 Control Array Test"
   ClientHeight    =   4000
   ClientLeft      =   60
   ClientTop       =   60
   ClientWidth     =   6000
   StartUpPosition =   3  'Windows Default
   Begin VB.CommandButton cmdBtn 
      Caption         =   "Button 0"
      Height          =   375
      Index           =   0
      Left            =   120
      TabIndex        =   0
      Top             =   120
      Width           =   1500
   End
   Begin VB.CommandButton cmdBtn 
      Caption         =   "Button 1"
      Height          =   375
      Index           =   1
      Left            =   120
      TabIndex        =   1
      Top             =   560
      Width           =   1500
   End
   Begin VB.CommandButton cmdBtn 
      Caption         =   "Button 2"
      Height          =   375
      Index           =   2
      Left            =   120
      TabIndex        =   2
      Top             =   1000
      Width           =   1500
   End
   Begin VB.TextBox txtArr 
      Height          =   315
      Index           =   0
      Left            =   1800
      TabIndex        =   3
      Text            =   "Text0"
      Top             =   120
      Width           =   2000
   End
   Begin VB.TextBox txtArr 
      Height          =   315
      Index           =   1
      Left            =   1800
      TabIndex        =   4
      Text            =   "Text1"
      Top             =   560
      Width           =   2000
   End
   Begin VB.CommandButton cmdSingle 
      Caption         =   "Single Button"
      Height          =   375
      Left            =   120
      TabIndex        =   5
      Top             =   1500
      Width           =   1500
   End
End
Attribute VB_Name = "Form1"
Option Explicit

Private Sub Form_Load()
    Dim s As String
    
    ' Read control array property
    s = cmdBtn(0).Caption
    s = txtArr(1).Text
    
    ' Write control array property
    cmdBtn(1).Caption = "Changed"
    txtArr(0).Text = "Hello Array"
    
    ' Non-array control still works
    s = cmdSingle.Caption
    cmdSingle.Caption = "OK"
    
    Form1.Caption = "P7.6 Array Test Done"
End Sub

Private Sub cmdBtn_Click(Index As Integer)
    Dim s As String
    s = cmdBtn(Index).Caption
    cmdBtn(Index).Caption = "Clicked"
End Sub

Private Sub cmdSingle_Click()
End Sub
