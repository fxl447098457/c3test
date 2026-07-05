VERSION 5.00
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
   StartUpPosition =   2  '屏幕中心
   Begin VB.Timer Timer1 
      Interval        =   1000
      Left            =   6360
      Top             =   5880
   End
   Begin VB.PictureBox Picture2 
      Height          =   855
      Left            =   7080
      ScaleHeight     =   795
      ScaleWidth      =   915
      TabIndex        =   9
      Top             =   4200
      Width           =   975
   End
   Begin VB.TextBox Text1 
      Height          =   975
      Left            =   6000
      MultiLine       =   -1  'True
      ScrollBars      =   3  'Both
      TabIndex        =   8
      Text            =   "Form1.frx":1A89
      Top             =   3000
      Width           =   1575
   End
   Begin VB.ListBox List1 
      Height          =   2400
      ItemData        =   "Form1.frx":1AA3
      Left            =   6480
      List            =   "Form1.frx":1AB0
      TabIndex        =   7
      Top             =   120
      Width           =   4455
   End
   Begin VB.CheckBox Check1 
      Caption         =   "Check1"
      Height          =   735
      Index           =   2
      Left            =   2280
      Picture         =   "Form1.frx":1AC6
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
      Picture         =   "Form1.frx":1DA9
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
      Picture         =   "Form1.frx":2092
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
      Picture         =   "Form1.frx":236B
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
      Picture         =   "Form1.frx":2611
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
      Picture         =   "Form1.frx":28E5
      Style           =   1  'Graphical
      TabIndex        =   1
      Top             =   4200
      Width           =   3615
   End
   Begin VB.PictureBox Picture1 
      Height          =   615
      Left            =   5760
      Picture         =   "Form1.frx":2BAA
      ScaleHeight     =   555
      ScaleWidth      =   555
      TabIndex        =   0
      Top             =   120
      Width           =   615
   End
   Begin VB.Image Image1 
      Height          =   600
      Left            =   5760
      Picture         =   "Form1.frx":2DC5
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

Dim dic As New Scripting.Dictionary

Private Sub Command1_Click()
    Dim v As Variant
    dic.Add "hello", "world"
    v = dic.Item("hello")
    MsgBox v
End Sub

Private Sub Form_Load()
    Dim a As String * 10, i As Long, b
'    List1.AddItem "图片：" & ImageList1.ListImages.Count
    For i = 0 To 10
        LSet a = "等等"
        List1.AddItem a & Now
        ' dic.Add removed for testing
    Next
'    For Each b In ImageList1.ListImages
'        dic.Add CStr(b.Index), b.Picture
'    Next
'    Me.Caption = "Power by vbman - " & VBMAN.Version()
End Sub

Private Sub Option1_Click(Index As Integer)
    Command1.Enabled = CBool(Index)
End Sub

Private Sub Timer1_Timer()
    Static i As Long
    If i = 0 Or i = 6 Then i = 1
'    Picture2.Picture = ImageList1.ListImages(i).Picture
    i = i + 1
End Sub
