VERSION 5.00
Begin VB.Form frmM8Test 
   Caption         =   "M8 Comprehensive Test"
   ClientHeight    =   6000
   ClientLeft      =   60
   ClientTop       =   450
   ClientWidth     =   8000
   Begin VB.TextBox txtInput 
      Height          =   375
      Left            =   120
      Text            =   "Hello M8"
      Top             =   120
      Width           =   2895
   End
   Begin VB.Label lblOutput 
      Caption         =   "Output:"
      Height          =   255
      Left            =   120
      Top             =   600
      Width           =   2895
   End
   Begin VB.CommandButton cmdAction 
      Caption         =   "Action"
      Height          =   375
      Left            =   120
      Top             =   960
      Width           =   1455
   End
   Begin VB.CommandButton cmdNavigate 
      Caption         =   "Navigate"
      Height          =   375
      Left            =   1680
      Top             =   960
      Width           =   1335
   End
   Begin VB.CheckBox chkOption 
      Caption         =   "Enable Logging"
      Height          =   255
      Left            =   120
      Top             =   1440
      Width           =   2895
   End
   Begin VB.OptionButton optMode 
      Caption         =   "Mode A"
      Height          =   255
      Index           =   0
      Left            =   120
      Top             =   1800
      Width           =   1335
   End
   Begin VB.OptionButton optMode 
      Caption         =   "Mode B"
      Height          =   255
      Index           =   1
      Left            =   1560
      Top             =   1800
      Width           =   1335
   End
   Begin VB.CommandButton cmdGrid 
      Caption         =   "Grid 0"
      Height          =   375
      Index           =   0
      Left            =   120
      Top             =   2160
      Width           =   975
   End
   Begin VB.CommandButton cmdGrid 
      Caption         =   "Grid 1"
      Height          =   375
      Index           =   1
      Left            =   1200
      Top             =   2160
      Width           =   975
   End
   Begin VB.CommandButton cmdGrid 
      Caption         =   "Grid 2"
      Height          =   375
      Index           =   2
      Left            =   2280
      Top             =   2160
      Width           =   975
   End
   Begin VB.TextBox txtUrl 
      Height          =   375
      Left            =   120
      Text            =   "https://www.example.com"
      Top             =   2640
      Width           =   3135
   End
   Begin SHDocVw.WebBrowser WebBrowser1 
      Height          =   5200
      Left            =   3360
      Top            =   120
      Width          =   4400
   End
   Begin VB.Menu mnuFile 
      Caption         =   "File"
      Begin VB.Menu mnuFileNew 
         Caption         =   "New"
      End
      Begin VB.Menu mnuFileSep1 
         Caption         =   "-"
      End
      Begin VB.Menu mnuFileExit 
         Caption         =   "Exit"
      End
   End
   Begin VB.Menu mnuEdit 
      Caption         =   "Edit"
      Begin VB.Menu mnuEditCopy 
         Caption         =   "Copy"
      End
      Begin VB.Menu mnuEditPaste 
         Caption         =   "Paste"
         Enabled         =   0
      End
   End
End
Attribute VB_Name = "frmM8Test"
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False

' M8 Comprehensive: P7.5 Controls + P7.6 Arrays + P7.8 Menus + P7.9 WebView

Private Sub Form_Load()
    Dim s As String
    s = txtInput.Text
    lblOutput.Caption = s
    
    Dim chkVal As Integer
    chkVal = chkOption.Value
End Sub

Private Sub cmdAction_Click()
    Dim s As String
    s = txtInput.Text
    lblOutput.Caption = s
End Sub

Private Sub cmdNavigate_Click()
    WebBrowser1.Navigate txtUrl.Text
End Sub

Private Sub cmdGrid_Click(Index As Integer)
    lblOutput.Caption = "Grid"
End Sub

Private Sub mnuFileNew_Click()
    lblOutput.Caption = "New"
End Sub

Private Sub mnuFileExit_Click()
    lblOutput.Caption = "Exit"
End Sub

Private Sub mnuEditCopy_Click()
    lblOutput.Caption = "Copy"
End Sub
Private Sub chkOption_Click()
    ' CheckBox click handler
End Sub

Private Sub optMode_Click(Index As Integer)
    ' OptionButton array click handler
End Sub

Private Sub mnuEditPaste_Click()
    ' Disabled menu click handler (VB6 allows calling disabled handlers)
End Sub