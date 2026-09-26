VERSION 5.00
Begin VB.Form Form1
   Caption         =   "OLE容器判据"
   ClientHeight    =   3600
   ClientLeft      =   60
   ClientTop       =   345
   ClientWidth     =   6480
   Begin VB.OLE OLE1
      Class           =   "Package"
      OLETypeAllowed  =   2
      SizeMode        =   1
      DisplayAsIcon   =   0
      AutoActivate    =   2
      AutoVerbMenu    =   -1
      Height          =   1215
      Left            =   240
      Top             =   240
      Width           =   3735
   End
   Begin VB.CommandButton Command1
      Caption         =   "GO"
      Height          =   375
      Left            =   240
      Top             =   2880
      Width           =   1335
   End
End
Attribute VB_Name = "Form1"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Private Sub Form_Load()
    ' 设计期属性读回 (CL = Class, 其余为 Long 语义)
    Debug.Print "OC1-CLASS=" & OLE1.Class
    Debug.Print "OC2-ALLOWED=" & OLE1.OLETypeAllowed
    Debug.Print "OC3-SIZEMODE=" & OLE1.SizeMode
    Debug.Print "OC4-DISPICON=" & OLE1.DisplayAsIcon
    Debug.Print "OC5-AUTOACT=" & OLE1.AutoActivate
    Debug.Print "OC6-AUTOVERB=" & OLE1.AutoVerbMenu
    Debug.Print "OC7-OLETYPE=" & OLE1.OLEType
    ' 运行期方法面: **由文件创建**嵌入 (OleCreateFromFile, 不触发服务器 UI)。
    ' ⚠ CreateEmbed("") (按 Class 新建) 会拉起 packager 的编辑会话等 UI —— 无头环境挂死,
    ' 本地实测过才把判据改成文件路径。
    Dim ok As Integer
    ok = OLE1.CreateEmbed(App.Path & "\oc_src.bin")
    Debug.Print "OC8-EMBED=" & IIf(ok, "Y", "N")
    Debug.Print "OC9-OLETYPE2=" & OLE1.OLEType   ' 嵌入后 = vbOLEEmbedded(1)
    ' 存取往返
    OLE1.SaveToFile "oc_roundtrip.bin"
    OLE1.Close
    Debug.Print "OC10-SAVED=" & IIf(Dir("oc_roundtrip.bin") <> "", "Y", "N")
    If OLE1.ReadFromFile("oc_roundtrip.bin") Then
        Debug.Print "OC11-READ=Y"
        Debug.Print "OC12-OLETYPE3=" & OLE1.OLEType
    Else
        Debug.Print "OC11-READ=N"
    End If
    Unload Me
End Sub

Private Sub Command1_Click()
    Dim s As String
    ' 嵌入一个空包 (按 Class 新建)
    If OLE1.CreateEmbed("") Then
        s = "OC8-EMBED=Y"
    Else
        s = "OC8-EMBED=N"
    End If
    Debug.Print s
    Debug.Print "OC9-OLETYPE2=" & OLE1.OLEType
    OLE1.Close
End Sub
