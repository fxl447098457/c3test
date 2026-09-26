VERSION 5.00
Begin VB.Form Form1
   Caption         =   "Data控件判据"
   ClientHeight    =   3000
   ClientLeft      =   60
   ClientTop       =   345
   ClientWidth     =   6360
   Begin VB.Data Data1
      Caption         =   "Data1"
      Connect         =   ""
      DatabaseName    =   "C:\Users\Administrator\Documents\c3.vb6.pro\tests\c29data\data"
      RecordSource    =   "SELECT id, name, dept FROM [people.csv] ORDER BY id"
      Height          =   375
      Left            =   240
      Top             =   2520
      Width           =   5880
   End
   Begin VB.TextBox Text1
      DataSource      =   "Data1"
      DataField       =   "name"
      Height          =   300
      Left            =   240
      Top             =   360
      Width           =   2400
   End
   Begin VB.CommandButton Command1
      Caption         =   "GO"
      Height          =   375
      Left            =   240
      Top             =   1800
      Width           =   1335
   End
   Begin VB.Timer Timer1
      Interval        =   200
      Left            =   5400
      Top             =   360
   End
End
Attribute VB_Name = "Form1"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit

Private Sub Form_Load()
    ' Refresh 拉数据 (ODBC 懒连接 —— Refresh 才真开)
    Data1.Recordset.Refresh
    Debug.Print "DT1-RECORDCOUNT=" & Data1.Recordset.RecordCount
    Debug.Print "DT2-ROW-BEFORE=" & Data1.Recordset.CurrentRow
End Sub

Private Sub Timer1_Timer()
    Static done As Integer
    If done Then Exit Sub
    done = 1
    ' 遍历 (MoveFirst → MoveNext 到 EOF); 每行读 Fields("id").Value / 绑定 Text1 联动
    Dim acc As String
    Data1.Recordset.MoveFirst
    Dim guard As Long
    guard = 0
    Do While guard < 10
        guard = guard + 1
        If guard > Data1.Recordset.RecordCount Then Exit Do
        Dim v1 As String, v2 As String
        v1 = Data1.Recordset.Fields("id").Value
        v2 = Data1.Recordset.Fields("name").Value
        Debug.Print "LOOPV1=" & v1
        Debug.Print "LOOPV2=" & v2
        acc = acc & v1 & ":" & v2 & ","
        Data1.Recordset.MoveNext
    Loop
    Debug.Print "DT3-FOREACH=" & acc
    Debug.Print "DT5-ROW-AFTERMOVEFIRST=" & Data1.Recordset.CurrentRow
    ' Reposition 事件计数在事件里累计, 这里只读结果
    Debug.Print "DT6-TEXT1-BOUND=" & Text1.Text
    Unload Me
End Sub

Private Sub Data1_Reposition()
    Static cnt As Long
    cnt = cnt + 1
    Debug.Print "EVT1-REPOSITION#" & cnt
End Sub
