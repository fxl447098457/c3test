VERSION 5.00
Begin VB.Form Form1 
   Caption         =   "Hello World"
   ClientHeight    =   4500
   ClientLeft      =   60
   ClientTop       =   345
   ClientWidth     =   6000
   LinkTopic       =   "Form1"
   ScaleHeight     =   4500
   ScaleWidth      =   6000
   StartUpPosition =   3  'Windows Default
   Begin VB.CommandButton cmdOK 
      Caption         =   "OK"
      Height          =   375
      Left            =   2400
      TabIndex        =   2
      Top             =   3600
      Width           =   1215
   End
   Begin VB.TextBox txtName 
      Height          =   375
      Left            =   1800
      TabIndex        =   1
      Text            =   "World"
      Top             =   600
      Width           =   3015
   End
   Begin VB.Label lblPrompt 
      Caption         =   "Enter your name:"
      Height          =   375
      Left            =   240
      TabIndex        =   0
      Top             =   600
      Width           =   1455
   End
End
Attribute VB_Name = "Form1"
Option Explicit

Private Sub cmdOK_Click()
    MsgBox "Hello, " & txtName.Text & "!"
End Sub

Private Sub Form_Load()
    txtName.Text = "World"
End Sub
