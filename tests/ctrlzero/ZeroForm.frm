VERSION 5.00
Begin VB.Form ZeroForm 
   Caption         =   "ZeroForm"
   ClientHeight    =   3000
   ClientLeft      =   120
   ClientTop       =   465
   ClientWidth     =   4680
   LinkTopic       =   "Form1"
   ScaleHeight     =   3000
   ScaleWidth      =   4680
   Begin VB.ListBox List1 
      Height          =   600
      Left            =   120
      TabIndex        =   2
      Top             =   120
      Width           =   1200
   End
   Begin VB.ComboBox Combo1 
      Height          =   300
      Left            =   120
      TabIndex        =   1
      Top             =   840
      Width           =   1200
   End
   Begin VB.TextBox Text1 
      Height          =   300
      Left            =   1560
      TabIndex        =   3
      Text            =   "x"
      Top             =   120
      Width           =   1200
   End
   Begin VB.PictureBox Pic1 
      Height          =   600
      Left            =   1560
      ScaleHeight     =   540
      ScaleWidth      =   1140
      TabIndex        =   4
      Top             =   600
      Width           =   1200
   End
End
Attribute VB_Name = "ZeroForm"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit
' ledger 278 knife 12 (B72): the control-method tables answer BOTH questions - which C exit
' and how many arguments that exit takes. These four call shapes are the ones the statement /
' expression / With docks emit today; the sentinel compares the emitted call arity with the
' table and with the RTL prototype, so a future param added to an RTL exit cannot leave the
' codegen shipping the old count (locally that is only warning C4020, on the newer cl it is
' error C2197 -- the shape that already cost gate #371 once).
Private Sub DoZero()
    List1.Clear
    Combo1.Clear
    Text1.SetFocus
End Sub
Private Function Sizes() As Long
    Sizes = Pic1.TextHeight("ab") + Pic1.TextWidth("cd")
End Function
Private Function Conv() As Long
    Conv = ScaleX(1440, 1, 3) + ScaleY(1440, 1, 3)
End Function
