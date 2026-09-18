VERSION 5.00
Begin VB.Form Form1 
   AutoRedraw      =   -1  'True
   BackColor       =   &H00FFFFFF&
   Caption         =   "Form1"
   ClientHeight    =   5280
   ClientLeft      =   60
   ClientTop       =   405
   ClientWidth     =   7950
   LinkTopic       =   "Form1"
   ScaleHeight     =   5280
   ScaleWidth      =   7950
   StartUpPosition =   3  'Windows Default
   Begin Proyecto1.ucChartArea ucChartArea1 
      Height          =   3615
      Left            =   -120
      TabIndex        =   0
      Top             =   360
      Width           =   6975
      _extentx        =   12303
      _extenty        =   6376
      font            =   "Form1.frx":0000
      linescurve      =   -1  'True
      verticallines   =   -1  'True
      titlefont       =   "Form1.frx":0028
      titleforecolor  =   0
   End
End
Attribute VB_Name = "Form1"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False

  
Private Sub Form_Load()
    Dim Value As Collection
    Set Value = New Collection
    
    Value.Add "Enero"
    Value.Add "Febrero"
    Value.Add "Marzo"
    Value.Add "Abril"
    Value.Add "Mayo"
    Value.Add "Junio"
    ucChartArea1.AddAxisItems Value
    
    Set Value = New Collection
    With Value
        .Add 2
        .Add 5
        .Add 7
        .Add -10
        .Add 5
        .Add 10
    End With
    ucChartArea1.AddLineSeries "2007", Value, vbRed
    'Exit Sub
    Set Value = New Collection
    With Value
        .Add 8
        .Add 4
        .Add 45
        .Add -15
        .Add 9
        .Add 14
    End With
    ucChartArea1.AddLineSeries "2008", Value, vbBlue
    Set Value = New Collection
    With Value
        .Add 14
        .Add 8
        .Add 16
        .Add 4
        .Add 24
        .Add 3
    End With
    ucChartArea1.AddLineSeries "2009", Value, vbGreen
End Sub

Private Sub Form_Resize()
    ucChartArea1.Move 0, 0, Me.ScaleWidth, Me.ScaleHeight
End Sub

