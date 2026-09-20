VERSION 5.00
Begin VB.Form Form1 
   Caption         =   "P7.5 Control Property Test"
   ClientHeight    =   4000
   ClientLeft      =   60
   ClientTop       =   60
   ClientWidth     =   6000
   StartUpPosition =   3  'Windows Default
   Begin VB.TextBox txtName 
      Height          =   315
      Left            =   120
      TabIndex        =   0
      Text            =   "Hello"
      Top             =   120
      Width           =   2000
   End
   Begin VB.Label lblGreeting 
      Caption         =   "Greeting:"
      Height          =   255
      Left            =   120
      TabIndex        =   1
      Top             =   520
      Width           =   2000
   End
   Begin VB.CommandButton cmdClick 
      Caption         =   "Click Me"
      Height          =   375
      Left            =   120
      TabIndex        =   2
      Top             =   880
      Width           =   1500
   End
   Begin VB.CheckBox chkOption 
      Caption         =   "Enable Feature"
      Height          =   255
      Left            =   120
      TabIndex        =   3
      Top             =   1360
      Value           =   1  'Checked
      Width           =   2000
   End
   Begin VB.OptionButton optChoice 
      Caption         =   "Choice A"
      Height          =   255
      Left            =   120
      TabIndex        =   4
      Top             =   1720
      Value           =   -1  'True
      Width           =   2000
   End
End
Attribute VB_Name = "Form1"
Option Explicit

Private Sub Form_Load()
    Dim s As String
    Dim v As Integer
    
    ' Read TextBox.Text
    s = txtName.Text
    
    ' Read Label.Caption
    s = lblGreeting.Caption
    
    ' Read CommandButton.Caption
    s = cmdClick.Caption
    
    ' Read CheckBox.Value
    v = chkOption.Value
    
    ' Read OptionButton.Value
    v = optChoice.Value
    
    ' Write TextBox.Text
    txtName.Text = "World"
    
    ' Write Label.Caption
    lblGreeting.Caption = "Hello World"
    
    ' Write CommandButton.Caption
    cmdClick.Caption = "Pressed"
    
    ' Write CheckBox.Value
    chkOption.Value = 0
    
    ' Write OptionButton.Value
    optChoice.Value = 0
    
    ' Form.Caption read/write
    s = Form1.Caption
    Form1.Caption = "P7.5 Test Done"
    
    ' Visible property
    txtName.Visible = -1
    
    ' Enabled property
    cmdClick.Enabled = -1
End Sub
Private Sub cmdClick_Click()
End Sub

Private Sub chkOption_Click()
End Sub

Private Sub optChoice_Click()
End Sub
