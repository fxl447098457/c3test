VERSION 5.00
Object = "{831FDD16-0C5C-11D2-A9FC-0000F8754DA1}#2.0#0"; "MSCOMCTL.OCX"
Begin VB.Form Form1 
   Caption         =   "Form1"
   ClientHeight    =   6945
   ClientLeft      =   120
   ClientTop       =   465
   ClientWidth     =   11055
   Icon            =   "Form1.frx":0000
   LinkTopic       =   "Form1"
   Picture         =   "Form1.frx":10CA
   ScaleHeight     =   6945
   ScaleWidth      =   11055
   StartUpPosition =   3  '窗口缺省
   Begin VB.PictureBox Picture2 
      Height          =   855
      Left            =   7080
      ScaleHeight     =   795
      ScaleWidth      =   915
      TabIndex        =   9
      Top             =   4200
      Width           =   975
   End
   Begin MSComctlLib.ImageList ImageList1 
      Left            =   6120
      Top             =   4560
      _ExtentX        =   1005
      _ExtentY        =   1005
      BackColor       =   -2147483643
      ImageWidth      =   19
      ImageHeight     =   19
      MaskColor       =   12632256
      _Version        =   393216
      BeginProperty Images {2C247F25-8591-11D1-B16A-00C0F0283628} 
         NumListImages   =   5
         BeginProperty ListImage1 {2C247F27-8591-11D1-B16A-00C0F0283628} 
            Picture         =   "Form1.frx":1A89
            Key             =   ""
         EndProperty
         BeginProperty ListImage2 {2C247F27-8591-11D1-B16A-00C0F0283628} 
            Picture         =   "Form1.frx":1D9E
            Key             =   "nuqi"
         EndProperty
         BeginProperty ListImage3 {2C247F27-8591-11D1-B16A-00C0F0283628} 
            Picture         =   "Form1.frx":20DB
            Key             =   ""
         EndProperty
         BeginProperty ListImage4 {2C247F27-8591-11D1-B16A-00C0F0283628} 
            Picture         =   "Form1.frx":232C
            Key             =   "haha"
            Object.Tag             =   "哈哈咖啡"
         EndProperty
         BeginProperty ListImage5 {2C247F27-8591-11D1-B16A-00C0F0283628} 
            Picture         =   "Form1.frx":2572
            Key             =   ""
            Object.Tag             =   "小狗"
         EndProperty
      EndProperty
   End
   Begin VB.TextBox Text1 
      Height          =   975
      Left            =   6000
      MultiLine       =   -1  'True
      ScrollBars      =   3  'Both
      TabIndex        =   8
      Text            =   "Form1.frx":27C4
      Top             =   3000
      Width           =   1575
   End
   Begin VB.ListBox List1 
      Height          =   2400
      ItemData        =   "Form1.frx":27DE
      Left            =   6480
      List            =   "Form1.frx":27EB
      TabIndex        =   7
      Top             =   120
      Width           =   4455
   End
   Begin VB.CheckBox Check1 
      Caption         =   "Check1"
      Height          =   735
      Index           =   2
      Left            =   2280
      Picture         =   "Form1.frx":2801
      Style           =   1  'Graphical
      TabIndex        =   6
      Top             =   5760
      Width           =   975
   End
   Begin VB.CheckBox Check1 
      Caption         =   "Check1"
      Height          =   735
      Index           =   1
      Left            =   1320
      Picture         =   "Form1.frx":2AE4
      Style           =   1  'Graphical
      TabIndex        =   5
      Top             =   5760
      Width           =   975
   End
   Begin VB.OptionButton Option1 
      Caption         =   "启用"
      Height          =   975
      Index           =   1
      Left            =   4080
      Picture         =   "Form1.frx":2DCD
      Style           =   1  'Graphical
      TabIndex        =   4
      Top             =   5520
      Width           =   1695
   End
   Begin VB.CheckBox Check1 
      Caption         =   "Check1"
      Height          =   735
      Index           =   0
      Left            =   360
      Picture         =   "Form1.frx":30A6
      Style           =   1  'Graphical
      TabIndex        =   3
      Top             =   5760
      Width           =   975
   End
   Begin VB.OptionButton Option1 
      Caption         =   "禁用"
      Height          =   1095
      Index           =   0
      Left            =   4080
      Picture         =   "Form1.frx":334C
      Style           =   1  'Graphical
      TabIndex        =   2
      Top             =   4440
      Width           =   1695
   End
   Begin VB.CommandButton Command1 
      Appearance      =   0  'Flat
      Caption         =   "Command1"
      Height          =   1335
      Left            =   240
      Picture         =   "Form1.frx":3620
      Style           =   1  'Graphical
      TabIndex        =   1
      Top             =   4200
      Width           =   3615
   End
   Begin VB.PictureBox Picture1 
      Height          =   615
      Left            =   5760
      Picture         =   "Form1.frx":38E5
      ScaleHeight     =   555
      ScaleWidth      =   555
      TabIndex        =   0
      Top             =   120
      Width           =   615
   End
   Begin VB.Image Image1 
      Height          =   600
      Left            =   5760
      Picture         =   "Form1.frx":3B00
      Top             =   840
      Width           =   600
   End
End
Attribute VB_Name = "Form1"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit

Private Sub Command1_Click()
    Static i As Long
    If i = 0 Or i = 6 Then i = 1
    Picture2.Picture = ImageList1.ListImages(i).Picture
    i = i + 1
End Sub

Private Sub Form_Load()
    Dim a As String * 10, i As Long
    For i = 0 To 10
        LSet a = "等等"
        List1.AddItem a & Now
    Next
End Sub

Private Sub Option1_Click(Index As Integer)
    Command1.Enabled = CBool(Index)
End Sub
