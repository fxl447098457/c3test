VERSION 5.00
Begin VB.Form Form1 
   Caption         =   "MouseHover Test"
   ClientHeight    =   3000
   ClientLeft      =   60
   ClientTop       =   450
   ClientWidth     =   4000
   Begin VB.CommandButton Command1 
      Caption         =   "Hover Me"
      Height          =   495
      Left            =   120
      TabIndex        =   0
      Top             =   120
      Width           =   1215
   End
End
Attribute VB_Name = "Form1"
Option Explicit

Private Sub Command1_MouseHover()
    Command1.Caption = "Hovered!"
End Sub