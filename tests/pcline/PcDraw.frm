VERSION 5.00
Begin VB.Form PcDrawForm 
   Caption         =   "PcDraw"
   ClientHeight    =   3200
   ClientLeft      =   120
   ClientTop       =   465
   ClientWidth     =   6400
   ScaleHeight     =   3200
   ScaleWidth      =   6400
   StartUpPosition =   3  '窗口缺省
   Begin VB.PictureBox picC 
      BackColor       =   &H00FFFFFF&
      Height          =   1200
      Left            =   120
      ScaleHeight     =   1140
      ScaleMode       =   3  'Pixel
      ScaleWidth      =   3000
      TabIndex        =   2
      Top             =   120
      Width           =   4500
   End
   Begin VB.Timer tmrP 
      Interval        =   150
      Left            =   120
      Top             =   1560
   End
End
Attribute VB_Name = "PcDrawForm"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit
' 账 #221 = C29-PL-a 的运行期判据。这条方法的缺陷天生**只有画完再问像素才看得见**：
' 改前那 8 处 Line 全发成 `ComGetObjectProp(hwnd, L"Line")` 再对它取 `Item`，两跳都在
' RTL 登记过的"认识但什么都不做"里空转 ⇒ 编得过、跑得起、一笔不画、一条诊断也不打。
' 四形每形钉**两头**：画过的那一枚像素必须等于交进去的颜色，没画过的那一枚必须不等于它
' —— 只钉前者会放过"整片刷成红"那种假绿；只钉后者会放过"根本没落笔"。
' 画与问都放在 Timer 第一拍: 窗口问题要在窗口活着的时候问 (dcsurf 那条口径) ——
' 先试在 Form_Load 里画+问, 实测 GetPixel 一律回 -1 (CLR_INVALID), 因为那时候句柄还归不出 DC。
' picC 按 ScaleMode=3(像素) 走 ⇒ 探针坐标就是设备像素, 不随 DPI 抖; 缇那一档已由
' #196/#197/#198 三格钉着, 这一格不重复问单位。椭圆按"边上有、中心没有"判:
' 切点取整在 ±1 内, 所以对 y 开一个小窗口找, 不钉死一个坐标。
Private Declare PtrSafe Function GetPixel Lib "gdi32" (ByVal hdc As LongPtr, ByVal x As Long, ByVal y As Long) As Long
' DC 取法照 tests/pbsub/PbForm.frm: GetDC(控件 hwnd)。picC.hDC 在这枚夹具上读出 0,
' 那是 #196 那一族的另一问, 不在本刀里改。
Private Declare PtrSafe Function GetDC Lib "user32" (ByVal hwnd As LongPtr) As LongPtr
Private Declare PtrSafe Function ReleaseDC Lib "user32" (ByVal hwnd As LongPtr, ByVal hdc As LongPtr) As Long

Private Sub tmrP_Timer()
    Dim d As LongPtr
    Dim r As Long
    Dim i As Long
    Dim hitEdge As Boolean
    Dim okLine As Boolean, okBox As Boolean, okFill As Boolean, okCircle As Boolean

    tmrP.Enabled = False

    ' 四形都画在 picC 里, 各自占一段 x 区间, 互不重叠
    picC.Line (0, 0)-(40, 40), vbRed              ' 默认: 线段
    picC.Line (60, 0)-(100, 40), vbBlue, B        ' B: 空心框
    picC.Line (120, 0)-(160, 40), vbGreen, BF     ' BF: 实心框
    picC.Line (180, 0)-(220, 40), vbRed, C        ' C: 椭圆(不填)

    d = GetDC(picC.hwnd)
    ' 线段: (20,20) 正落在 0,0 -> 40,40 那条对角线上; (20,5) 在旁边
    okLine = (GetPixel(d, 20, 20) = vbRed) And (GetPixel(d, 20, 5) <> vbRed)
    ' 空心框: 左边框 x=60 那一条必须是蓝, 正中心 (80,20) 必须**不是**蓝(没填)
    okBox = (GetPixel(d, 60, 20) = vbBlue) And (GetPixel(d, 80, 20) <> vbBlue)
    ' 实心框: 正中心 (140,20) 必须是绿 —— 这一条同时证明 BF 与 B 折出来不是同一个数
    okFill = (GetPixel(d, 140, 20) = vbGreen)
    ' 椭圆: 左边切点 x=180 那一列在 y=15..25 之间必须有一枚红; 中心 (200,20) 必须不是红
    hitEdge = False
    For i = 15 To 25
        If GetPixel(d, 180, i) = vbRed Then hitEdge = True
    Next
    okCircle = hitEdge And (GetPixel(d, 200, 20) <> vbRed)

    Debug.Print "PL00-DC=" & CStr(d)
    Debug.Print "PL01-LINE=" & CStr(okLine)
    Debug.Print "PL02-BOX=" & CStr(okBox)
    Debug.Print "PL03-FILL=" & CStr(okFill)
    Debug.Print "PL04-CIRCLE=" & CStr(okCircle)
    Debug.Print "PL05-RAW diag=" & CStr(GetPixel(d, 20, 20))
    Debug.Print "PL06-RAW boxedge=" & CStr(GetPixel(d, 60, 20)) & " boxmid=" & CStr(GetPixel(d, 80, 20))
    Debug.Print "PL07-RAW fillmid=" & CStr(GetPixel(d, 140, 20)) & " circletop=" & CStr(GetPixel(d, 200, 0))
    Debug.Print "PL-DONE"
    r = ReleaseDC(picC.hwnd, d)
    Unload Me
End Sub
