VERSION 5.00
Begin VB.Form frmUnits 
   ClientHeight    =   3000
   ClientLeft      =   0
   ClientTop       =   0
   ClientWidth     =   6000
   ScaleHeight     =   3000
   ScaleMode       =   1  'Twips
   ScaleWidth      =   6000
   Begin VeUnits.ucUnitTwip uTw 
      Height          =   1200
      Left            =   120
      TabIndex        =   0
      Top             =   120
      Width           =   2400
   End
   Begin VeUnits.ucUnitPix uPix 
      Height          =   1200
      Left            =   120
      TabIndex        =   1
      Top             =   1440
      Width           =   2400
   End
End
Attribute VB_Name = "frmUnits"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit
' 两枚控件同尺寸 (2400 缇 = 160 像素 @96dpi), 只差 .ctl 声明的 ScaleMode。
' 判据写成**同一枚字体量出来的两个数之比**, 于是与 DPI 无关:
'   缇型控件的 TextWidth / ScaleWidth 必须 = 像素型的 x Screen.TwipsPerPixelX
Private Sub Form_Load()
    Dim tpp As Long
    Dim twT As Long, twP As Long
    Dim thT As Long, thP As Long
    Dim swT As Long, swP As Long
    tpp = Screen.TwipsPerPixelX
    twT = uTw.TW("MMMM")
    twP = uPix.TW("MMMM")
    thT = uTw.TH("MMMM")
    thP = uPix.TH("MMMM")
    swT = uTw.SW()
    swP = uPix.SW()
    Debug.Print "U-RAW tw=" & twT & " pix=" & twP & " swT=" & swT & " swP=" & swP
    Debug.Print "U-SW=" & CStr(Abs(swT - swP * tpp) <= 2)
    Debug.Print "U-TW=" & CStr(Abs(twT - twP * tpp) <= 2)
    Debug.Print "U-TH=" & CStr(Abs(thT - thP * tpp) <= 2)
    ' 账 #179: 早绑定直调也要跑在控件自己的宿主上下文里 (发码在每个 .ctl 实例方法
    ' 体首 vb6_UC_PushInstance(me)、统一出口尾 vb6_UC_PopInstance)。判据只取
    ' ScaleMode 一位数字 ⇒ 与 DPI/尺寸无关: 缇型那枚必须报 1, 像素型那枚必须报 3。
    ' (不用 Left$(cm,1) 是因为台账 #68: 窗体模块里 Left(...) 会被控件属性抢走。)
    Dim cm As String, cp As String
    cm = uTw.Ctx()
    cp = uPix.Ctx()
    Debug.Print "U-CTX-RAW twip=" & cm & " pix=" & cp
    Debug.Print "U-CTX=" & CStr(InStr(cm, "1/") = 1 And InStr(cp, "3/") = 1)
    ' 账 #159: UserControl.hWnd 是 void* 全局, 此前类型 oracle 认不得它 ⇒ 答 Variant
    ' ⇒ 比较走装箱那一路 (把全局的地址当 vb6_VARIANT* 递进去) ⇒ 恒假。两头判据:
    ' 控件里直接比 0 (HwOk) + 与另一枚句柄成员比 (HwVs), 两边都必须 True。
    Dim hoT As String, hvT As String, hoP As String, hvP As String
    hoT = uTw.HwOk()
    hvT = uTw.HwVs()
    hoP = uPix.HwOk()
    hvP = uPix.HwVs()
    Debug.Print "U-HW-RAW twip=" & hoT & "/" & hvT & " pix=" & hoP & "/" & hvP
    Debug.Print "U-HW=" & CStr(hoT = "True" And hvT = "True" And hoP = "True" And hvP = "True")
    ' 账 #180: 容器句柄的契约读数 (两枚控件的容器都必须是同一个非零窗口, 且不是自己)。
    Dim ck1 As String, ck2 As String, cs1 As String
    ck1 = uTw.CntOk()
    ck2 = uPix.CntOk()
    cs1 = uTw.CntStr()
    Debug.Print "U-CNT-RAW ok=" & ck1 & "/" & ck2 & " cnt=" & cs1
    Debug.Print "U-CNT=" & CStr(ck1 = "True" And ck2 = "True" And cs1 <> "")
    Debug.Print "U-DONE"
    Unload Me
End Sub
