VERSION 5.00
Begin VB.Form frmWebView 
   Caption         =   "P7.9 WebView Test"
   ClientHeight    =   6000
   ClientLeft      =   60
   ClientTop       =   450
   ClientWidth     =   8000
   ScaleHeight     =   6000
   ScaleWidth      =   8000
   Begin VB.CommandButton cmdNavigate 
      Caption         =   "Navigate"
      Height          =   375
      Left            =   120
      TabIndex        =   0
      Top             =   120
      Width           =   1455
   End
   Begin VB.TextBox txtUrl 
      Height          =   375
      Left            =   1680
      TabIndex        =   1
      Text            =   "https://www.baidu.com"
      Top             =   120
      Width           =   6255
   End
   Begin SHDocVw.WebBrowser WebBrowser1 
      Height          =   5000
      Left            =   120
      TabIndex        =   2
      Top             =   600
      Width           =   7800
   End
End
Attribute VB_Name = "frmWebView"
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False

Private Sub cmdNavigate_Click()
    WebBrowser1.Navigate txtUrl.Text
End Sub

Private Sub Form_Load()
    Dim x As Integer
    x = 42
End Sub
