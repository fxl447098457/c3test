VERSION 5.00
Begin VB.Form Form1 
   Caption         =   "P17.1 With Test"
   ClientHeight    =   3090
   ClientLeft      =   60
   ClientTop       =   450
   ClientWidth     =   4680
   LinkTopic       =   "Form1"
   ScaleHeight     =   3090
   ScaleWidth      =   4680
   StartUpPosition =   3  'Windows Default
   Begin VB.CommandButton Command1 
      Caption         =   "Click"
      Height          =   375
      Left            =   120
      TabIndex        =   2
      Top             =   120
      Width           =   1335
   End
   Begin VB.TextBox Text1 
      Height          =   375
      Left            =   120
      TabIndex        =   0
      Top             =   600
      Width           =   3015
   End
   Begin VB.Label Label1 
      Caption         =   "Label1"
      Height          =   375
      Left            =   120
      TabIndex        =   1
      Top             =   1080
      Width           =   3015
   End
End
Attribute VB_Name = "Form1"
Option Explicit

Private Sub Command1_Click()
    ' P17.1: With TextBox
    With Text1
        .Text = "Hello With"
        Debug.Print .Text
    End With
    
    ' P17.1: With Label
    With Label1
        .Caption = "With Caption"
        Debug.Print .Caption
    End With
    
    ' P17.1: With CommandButton
    With Command1
        .Caption = "With Btn"
        Debug.Print .Caption
    End With
    
    ' P17.1: Default property in With context
    Dim s As String
    With Text1
        s = .Text
    End With
    Debug.Print s
    
    ' P17.1: Nested With
    With Text1
        .Text = "Outer"
        With Label1
            .Caption = "Inner"
        End With
    End With
    Debug.Print Text1.Text
    Debug.Print Label1.Caption
    
    Unload Me
End Sub
