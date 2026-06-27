VERSION 5.00
Begin VB.Form frmMenuTest 
   Caption         =   "P7.8 Menu Test"
   ClientHeight    =   3000
   ClientLeft      =   60
   ClientTop       =   450
   ClientWidth     =   4000
   ScaleHeight     =   3000
   ScaleWidth      =   4000
   Begin VB.TextBox txtOutput 
      Height          =   375
      Left            =   120
      TabIndex        =   0
      Text            =   "Ready"
      Top             =   2400
      Width           =   3735
   End
   Begin VB.Menu mnuFile 
      Caption         =   "&File"
      Begin VB.Menu mnuFileNew 
         Caption         =   "&New"
      End
      Begin VB.Menu mnuFileSep1 
         Caption         =   "-"
      End
      Begin VB.Menu mnuFileOpen 
         Caption         =   "&Open"
         Enabled         =   0
      End
      Begin VB.Menu mnuFileSave 
         Caption         =   "&Save"
         Checked         =   -1
      End
      Begin VB.Menu mnuFileSep2 
         Caption         =   "-"
      End
      Begin VB.Menu mnuFileExit 
         Caption         =   "E&xit"
      End
   End
   Begin VB.Menu mnuEdit 
      Caption         =   "&Edit"
      Begin VB.Menu mnuEditCut 
         Caption         =   "Cu&t"
      End
      Begin VB.Menu mnuEditCopy 
         Caption         =   "&Copy"
      End
      Begin VB.Menu mnuEditPaste 
         Caption         =   "&Paste"
         Visible         =   0
      End
   End
   Begin VB.Menu mnuHelp 
      Caption         =   "&Help"
      Begin VB.Menu mnuHelpAbout 
         Caption         =   "&About"
      End
   End
End
Attribute VB_Name = "frmMenuTest"
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False

Private Sub mnuFileNew_Click()
    Dim x As Integer
    x = 1
End Sub

Private Sub mnuFileOpen_Click()
    Dim x As Integer
    x = 0
End Sub

Private Sub mnuFileSave_Click()
    Dim x As Integer
    x = 2
End Sub

Private Sub mnuFileExit_Click()
    Dim x As Integer
    x = 3
End Sub

Private Sub mnuEditCut_Click()
    Dim x As Integer
    x = 4
End Sub

Private Sub mnuEditCopy_Click()
    Dim x As Integer
    x = 5
End Sub

Private Sub mnuHelpAbout_Click()
    Dim x As Integer
    x = 6
End Sub
