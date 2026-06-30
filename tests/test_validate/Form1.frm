VERSION 5.00
Begin VB.Form Form1 
   Caption         =   "Validate Test"
   ClientHeight    =   3000
   ClientLeft      =   60
   ClientTop       =   450
   ClientWidth     =   4500
   LinkTopic       =   "Form1"
   ScaleHeight     =   3000
   ScaleWidth      =   4500
   StartUpPosition =   3  'Windows Default
   Begin VB.TextBox Text2 
      CausesValidation=   0   'False
      Height          =   285
      Left            =   240
      TabIndex        =   1
      Top             =   720
      Width           =   3975
   End
   Begin VB.TextBox Text1 
      Height          =   285
      Left            =   240
      TabIndex        =   0
      Top             =   240
      Width           =   3975
   End
   Begin VB.CommandButton Command1 
      CausesValidation=   0   'False
      Caption         =   "Cancel"
      Height          =   375
      Left            =   240
      TabIndex        =   2
      Top             =   1200
      Width           =   1215
   End
End
Attribute VB_Name = "Form1"
Private Sub Text1_Validate(Cancel As Boolean)
    If Text1.Text = "" Then
        Cancel = True
    End If
End Sub

Private Sub Command1_Click()
    Unload Me
End Sub

Private Sub Form_Load()
    Text1.Text = "Hello"
    Debug.Print "Form_Load: Text1.Text = "; Text1.Text
    Debug.Print "Text2.CausesValidation = "; Text2.CausesValidation
    Debug.Print "Command1.CausesValidation = "; Command1.CausesValidation
End Sub