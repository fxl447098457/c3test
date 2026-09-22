VERSION 5.00
Begin VB.Form frmBalloonTooltips 
   BorderStyle     =   1  'Fixed Single
   Caption         =   "Balloon Tooltips"
   ClientHeight    =   3990
   ClientLeft      =   45
   ClientTop       =   390
   ClientWidth     =   6660
   BeginProperty Font 
      Name            =   "Microsoft Sans Serif"
      Size            =   12
      Charset         =   0
      Weight          =   400
      Underline       =   0   'False
      Italic          =   0   'False
      Strikethrough   =   0   'False
   EndProperty
   Icon            =   "frmBalloonTooltips.frx":0000
   LinkTopic       =   "Form1"
   MaxButton       =   0   'False
   ScaleHeight     =   3990
   ScaleWidth      =   6660
   StartUpPosition =   2  'CenterScreen
   Begin VB.PictureBox picBalloonTooltip 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      ForeColor       =   &H80000008&
      Height          =   1215
      Left            =   3360
      ScaleHeight     =   1185
      ScaleWidth      =   3105
      TabIndex        =   6
      Top             =   1800
      Width           =   3135
      Begin VB.CommandButton cmdButtonInPic 
         Caption         =   "Enable/Disable Tooltip"
         BeginProperty Font 
            Name            =   "Microsoft Sans Serif"
            Size            =   9.75
            Charset         =   0
            Weight          =   400
            Underline       =   0   'False
            Italic          =   0   'False
            Strikethrough   =   0   'False
         EndProperty
         Height          =   615
         Left            =   360
         TabIndex        =   7
         Top             =   240
         UseMaskColor    =   -1  'True
         Width           =   2415
      End
   End
   Begin VB.Frame frBalloonTooltip 
      Height          =   1215
      Left            =   120
      TabIndex        =   4
      Top             =   1800
      Width           =   3015
      Begin VB.Label lblInFrame 
         Alignment       =   2  'Center
         Appearance      =   0  'Flat
         BackColor       =   &H80000005&
         BorderStyle     =   1  'Fixed Single
         Caption         =   "Label In Frame Tooltip"
         ForeColor       =   &H80000008&
         Height          =   375
         Left            =   240
         TabIndex        =   5
         Top             =   480
         Width           =   2535
      End
   End
   Begin VB.ComboBox cmbBalloonTooltip 
      BeginProperty Font 
         Name            =   "Microsoft Sans Serif"
         Size            =   9.75
         Charset         =   0
         Weight          =   400
         Underline       =   0   'False
         Italic          =   0   'False
         Strikethrough   =   0   'False
      EndProperty
      Height          =   360
      Left            =   2640
      Style           =   2  'Dropdown List
      TabIndex        =   1
      Top             =   240
      Width           =   3855
   End
   Begin VB.TextBox txtBalloonTooltip 
      Height          =   375
      Left            =   2640
      TabIndex        =   3
      Text            =   "TextBox Tooltip"
      Top             =   960
      Width           =   3855
   End
   Begin VB.CommandButton cmdBalloonTooltip 
      Caption         =   "Change TextBox Tooltip"
      BeginProperty Font 
         Name            =   "Microsoft Sans Serif"
         Size            =   9.75
         Charset         =   0
         Weight          =   400
         Underline       =   0   'False
         Italic          =   0   'False
         Strikethrough   =   0   'False
      EndProperty
      Height          =   615
      Left            =   120
      TabIndex        =   2
      Top             =   960
      UseMaskColor    =   -1  'True
      Width           =   2415
   End
   Begin VB.Label lblBalloonTooltip 
      Alignment       =   2  'Center
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BorderStyle     =   1  'Fixed Single
      Caption         =   "Label Tooltip"
      ForeColor       =   &H80000008&
      Height          =   375
      Left            =   120
      TabIndex        =   0
      Top             =   240
      Width           =   2415
   End
End
Attribute VB_Name = "frmBalloonTooltips"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit

Private Sub cmdBalloonTooltip_Click()
    cTT.CreateToolTip txtBalloonTooltip, "This is the changed balloon text when hovering over the TextBox.", "Changed TextBox Tooltip", TTI_INFO_LARGE, vbBlack, &HE1FFFF, True, False
    cTT.CreateToolTip lblBalloonTooltip, "This is the changed balloon text when hovering over the Label.", "Changed Label Tooltip", TTI_INFO_LARGE, , , True, False
End Sub

Private Sub cmdButtonInPic_Click()
    cTT.EnableTooltips(cmdButtonInPic) = Not cTT.EnableTooltips(cmdButtonInPic)
End Sub

Private Sub Form_Load()
    cmbBalloonTooltip.AddItem "ComboBox Tooltip": cmbBalloonTooltip.ListIndex = 0
    cTT.CreateToolTip cmbBalloonTooltip, "This is the balloon text when hovering over the ComboBox.", cmbBalloonTooltip.List(0), TTI_INFO
    cTT.CreateToolTip cmdBalloonTooltip, "Click to change the appearance of the Label and TextBox tooltips.", cmdBalloonTooltip.Caption, TTI_WARNING_LARGE, vbRed, vbCyan, True
    cTT.CreateToolTip cmdButtonInPic, "Click to enable or disable the tooltip for this button.", cmdButtonInPic.Caption, TTI_WARNING_LARGE, vbMagenta, vbGreen, True
    cTT.CreateToolTip txtBalloonTooltip, "Help, I'm being repressed!", txtBalloonTooltip, TTI_ERROR_LARGE, vbBlue, vbYellow, True
    cTT.CreateToolTip lblBalloonTooltip, "This is the balloon text when hovering over the Label.", lblBalloonTooltip, TTI_WARNING
    cTT.CreateToolTip lblInFrame, "This is the balloon text when hovering over the Label In Frame.", lblInFrame, TTI_INFO
End Sub
