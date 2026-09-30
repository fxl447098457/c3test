VERSION 5.00
Begin VB.Form WalkForm 
   Caption         =   "WalkForm"
   ClientHeight    =   3300
   ClientLeft      =   120
   ClientTop       =   465
   ClientWidth     =   4680
   LinkTopic       =   "WalkForm"
   ScaleHeight     =   3300
   ScaleWidth      =   4680
   Begin VB.Timer tWalk 
      Enabled         =   -1   'True
      Interval        =   50
      Left            =   4320
      Top             =   240
   End
   Begin VB.CommandButton cmdTop2 
      Caption         =   "T2"
      Height          =   400
      Left            =   120
      TabIndex        =   5
      Top             =   2760
      Width           =   900
   End
   Begin VB.Frame optFrame 
      Caption         =   "opts"
      Height          =   900
      Left            =   2400
      TabIndex        =   4
      Top             =   2400
      Width           =   2000
      Begin VB.OptionButton optB 
         Caption         =   "B"
         Height          =   300
         Left            =   1080
         TabIndex        =   1
         TabStop         =   0   'False
         Top             =   360
         Width           =   700
      End
      Begin VB.OptionButton optA 
         Caption         =   "A"
         Height          =   300
         Left            =   120
         TabIndex        =   0
         TabStop         =   0   'False
         Top             =   360
         Value           =   -1   'True
         Width           =   700
      End
   End
   Begin VB.CommandButton cmdTop1 
      Caption         =   "T1"
      Height          =   400
      Left            =   120
      TabIndex        =   1
      Top             =   600
      Width           =   900
   End
   Begin VB.PictureBox Pic1 
      Height          =   1200
      Left            =   2520
      ScaleHeight     =   1140
      ScaleMode       =   3  'String
      ScaleWidth      =   1836
      TabIndex        =   6
      Top             =   240
      Width           =   1900
      Begin VB.CommandButton cmdInPic 
         Caption         =   "P"
         Height          =   360
         Left            =   240
         TabIndex        =   0
         Top             =   480
         Width           =   720
      End
   End
   Begin VB.Frame Frame1 
      Caption         =   "f1"
      Height          =   1560
      Left            =   120
      TabIndex        =   2
      Top             =   1080
      Width           =   2040
      Begin VB.Frame Frame2 
         Caption         =   "f2"
         Height          =   720
         Left            =   120
         TabIndex        =   2
         Top             =   720
         Width           =   1800
         Begin VB.CommandButton cmdDeep 
            Caption         =   "D"
            Height          =   360
            Left            =   960
            TabIndex        =   1
            Top             =   240
            Width           =   720
         End
         Begin VB.PictureBox picDeep 
            Height          =   360
            Left            =   120
            ScaleWidth      =   1680
            TabIndex        =   3
            Top             =   1200
            Width           =   1740
         End
         Begin VB.CommandButton cmdShy 
            Caption         =   "S"
            Enabled         =   0   'False
            Height          =   360
            Left            =   120
            TabIndex        =   0
            Top             =   240
            Width           =   720
         End
      End
      Begin VB.CommandButton cmdIn2 
         Caption         =   "I2"
         Height          =   360
         Left            =   1200
         TabIndex        =   1
         Top             =   240
         Width           =   720
      End
      Begin VB.CommandButton cmdIn1 
         Caption         =   "I1"
         Height          =   360
         Left            =   240
         TabIndex        =   0
         Top             =   240
         Width           =   720
      End
   End
   Begin VB.Label lblHead 
      Caption         =   "tabwalk"
      Height          =   255
      Left            =   120
      TabIndex        =   0
      Top             =   240
      Width           =   1455
   End
End
Attribute VB_Name = "WalkForm"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit
' 账 #83(b2) / #165 的夹具。**先把结论摆正**（这一格的前提被实测翻过两次）：
'   ① 容器（Frame / PictureBox / SSTab）要进对话框管理器的 tab 序，窗口必须挂
'      `WS_EX_CONTROLPARENT` —— 裸 Win32 探针（`.build/cp2`）早就定死这一条。
'      ⚠ 但发码那一位**以前抄错了数**：`controlContainerExStyleBit` 里手写的常量是
'      0x00040000 = `WS_EX_APPWINDOW`，而 SDK 头（winuser.h:2855）里 CONTROLPARENT 是
'      **0x00010000**。改成对数之后，运行期读数 `EX-fr / EX-f2 / EX-pic` 全是 65536，
'      而且**跳格真的能走进容器了** —— 账 #165 就在这一刀结掉。
'   ② #165 之所以被当成「OS 本人不认容器里的兄弟」，是夹具自己的读数窗口太窄：
'      相 2 原来只给 9 拍，而这一圈（7 站）从起点算起要 12 拍以上才走得完，
'      于是 `TW-in2 / TW-deep / TW-inpic` 三条永远读 N。通用式：**「走不到」要先证明
'      走的拍数够绕一圈** —— 现在相 2 的出口改成「七站齐了就收」，拍号只当保险丝（40 拍）。
'      那三条曾被当成「缺陷读数」钉在针面里 ⇒ 修好了测试反而红，这一课也记在 029。
'   ③ 剩下的两条**本批没修**，读数照旧打出来：
'      `TW-seq` 的次序 = z-order（≈创建顺序），不是 VB6 的 TabIndex ⇒ **账 #163**；
'      `AK-*`：optFrame 里两枚 `TabStop = 0` 的 OptionButton，程序化给 optA 焦点后
'      post VK_DOWN ⇒ `down=cmdTop1`、optB 依旧 N ⇒ 组内方向键压根不走 = **账 #168**。
'      同一族的第三条新读数 `TW-orenter`：七站里居然有 optA（`TabStop = 0` 本不该占站）——
'      对话框管理器把容器里的单选组当一站、落在勾选那枚上。这条**只打不钉**：
'      钉成 Y 是把缺陷固化进针面，钉成 N 是现在就会红。
' 夹具还顺手读一件别的事：
'   `TW-picstop` —— VB6 的 PictureBox 拿不到焦点、本该不在 tab 序里，而 `controlTabStopStyleBit`
'      排除表里**以前没有它** ⇒ 它自己在 tab 序里占一站（账 #164，已修，这一条翻绿）。
'      Frame2 里那枚 `picDeep` 是为第二条创建路补的证人：容器里的 PictureBox 同样不能立 `WS_TABSTOP`。
' ⚠ `TW-new`（走到的**新站数**）与 `TW-seq` 的**次序**都不是判据：同一个 exe 连跑三次读过 3、3、4，
'   换架构也会变（与 WS17 那条「条数是时序不是不变量」同一类）。这里只钉「走得到」。
Private gTick As Long
Private gSeen As String
Private gNew As Long
Private gWalked As Boolean
Private gAkPre As String
Private gAkPost As String
Private gAkDue As Boolean
Private gAkPosted As Boolean

Private Declare PtrSafe Function PostMessage Lib "user32" Alias "PostMessageW" (ByVal hWnd As LongPtr, ByVal Msg As Long, ByVal wParam As LongPtr, ByVal lParam As LongPtr) As Long
Private Declare PtrSafe Function GetFocus Lib "user32" () As LongPtr
Private Declare PtrSafe Function GetParent Lib "user32" (ByVal hWnd As LongPtr) As LongPtr
Private Declare PtrSafe Function GetActiveWindow Lib "user32" () As LongPtr
Private Declare PtrSafe Function GetWindowLong Lib "user32" Alias "GetWindowLongW" (ByVal hWnd As LongPtr, ByVal nIndex As Long) As Long
Private Declare PtrSafe Function IsWindowVisible Lib "user32" (ByVal hWnd As LongPtr) As Long
Private Declare PtrSafe Function IsWindowEnabled Lib "user32" (ByVal hWnd As LongPtr) As Long

Private Const WM_KEYDOWN As Long = &H100
Private Const WM_KEYUP As Long = &H101
Private Const VK_TAB As Long = &H9
Private Const VK_DOWN As Long = &H28

Private Function TF(ByVal ok As Boolean) As String
    If ok Then TF = "Y" Else TF = "N"
End Function

Private Function N(ByVal v As LongPtr) As String
    N = CStr(v)
End Function

Private Function HexEq(ByVal a As LongPtr, ByVal b As LongPtr) As String
    If a = b Then HexEq = "Y" Else HexEq = "N(" & CStr(a) & "<>" & CStr(b) & ")"
End Function

Private Sub Log1(ByVal s As String)
    Dim h As Long
    h = FreeFile
    Open App.Path & "\tabwalk.log" For Append As #h
    Print #h, s
    Close #h
    Debug.Print s
End Sub

' 站点名 —— 和 modal 夹具同一套：比对全走 `ByVal ... As LongPtr` 形参，不碰 `.hwnd` 的装箱
' （账 #159 那条坏路）。Frame 拿不到焦点、不在这张表里；**PictureBox 那张 `pic1` 是刻意留的**
' —— 它到底会不会被当成一站，是这一格要照出来的读数（见文件头 `TW-picstop`）。
Private Function WhereIs(ByVal f As LongPtr) As String
    If HexEq(f, cmdTop1.hwnd) = "Y" Then
        WhereIs = "cmdTop1"
    ElseIf HexEq(f, cmdTop2.hwnd) = "Y" Then
        WhereIs = "cmdTop2"
    ElseIf HexEq(f, cmdIn1.hwnd) = "Y" Then
        WhereIs = "cmdIn1"
    ElseIf HexEq(f, cmdIn2.hwnd) = "Y" Then
        WhereIs = "cmdIn2"
    ElseIf HexEq(f, cmdDeep.hwnd) = "Y" Then
        WhereIs = "cmdDeep"
    ElseIf HexEq(f, cmdInPic.hwnd) = "Y" Then
        WhereIs = "cmdInPic"
    ElseIf HexEq(f, Pic1.hwnd) = "Y" Then
        WhereIs = "pic1"
    ElseIf HexEq(f, cmdShy.hwnd) = "Y" Then
        WhereIs = "cmdShy"
    ElseIf HexEq(f, optA.hwnd) = "Y" Then
        WhereIs = "optA"
    ElseIf HexEq(f, optB.hwnd) = "Y" Then
        WhereIs = "optB"
    ElseIf HexEq(f, Me.hwnd) = "Y" Then
        WhereIs = "form"
    Else
        WhereIs = "other(" & CStr(f) & ")"
    End If
End Function

Private Function SeenIt(ByVal st As String) As Boolean
    SeenIt = (InStr(gSeen, st & ",") > 0)
End Function

Private Sub MarkStation(ByVal st As String)
    If st = "form" Then Exit Sub
    If Left(st, 5) = "other" Then Exit Sub
    If Not SeenIt(st) Then
        gSeen = gSeen & st & ","
        gNew = gNew + 1
    End If
End Sub

' 相 2 的出口：窗体级两枚 + 三型容器里的四枚都各站过一次 = 绕完一圈。
' 用它收尾而不是数拍子 —— 缺陷（走不进容器）时它不会满足，就走到保险丝那一步照出 N。
Private Function AllStations() As Boolean
    AllStations = SeenIt("cmdIn1") And SeenIt("cmdIn2") And SeenIt("cmdDeep") And _
                  SeenIt("cmdInPic") And SeenIt("cmdTop1") And SeenIt("cmdTop2")
End Function

Private Sub tWalk_Timer()
    Dim f As LongPtr
    Dim st As String
    gTick = gTick + 1

    ' 相 1：先把焦点交给**容器里**那枚 cmdIn1 —— 起点选在容器内部，这样第一跳就必须
    ' 「走进来又走出去」，不靠 CONTROLPARENT 的那条路会当场卡住。
    If gTick = 1 Then
        cmdIn1.SetFocus
        DoEvents
        gSeen = ""
        gNew = 0      ' 上面那句 DoEvents 会重入本 Timer ⇒ 相 2 可能先记下一站又被这里抹掉，
                     '  计数不清就会和 TW-seq 的条数对不上（TW-new 本来就不是判据，但别让它自相矛盾）
        gNew = 0
        MarkStation(WhereIs(GetFocus()))
        gWalked = True
        Exit Sub
    End If

    ' 相 2：逐拍读落点、再往焦点窗口 post 一对 VK_TAB，直到七站齐或撞上保险丝。
    If Not gAkDue Then
        If gTick > 40 Then gAkDue = True
        If gTick > 40 Then Exit Sub
        f = GetFocus()
        st = WhereIs(f)
        MarkStation st
        If gTick = 2 Then
            ' 运行期问一句：那一位到底挂上了没有、子控件的父窗到底是不是容器 ——
            ' 这一行打印的是**窗口上的实际值**，不是发码想发的数：抄错常量时它照样绿。
            ' GWL_EXSTYLE = -20。父窗比对走 `HexEq`（LongPtr 形参）—— 账 #159 那条
            ' `.hwnd` 直接装箱是坏的，拿 `GetParent(x.hwnd) = y.hwnd` 这种裸表达式比会假红。
            Log1 "EX-fr=" & CStr(GetWindowLong(Frame1.hwnd, -20)) & "/f2=" & CStr(GetWindowLong(Frame2.hwnd, -20)) & "/pic=" & CStr(GetWindowLong(Pic1.hwnd, -20)) & "/form=" & CStr(GetWindowLong(Me.hwnd, -20))
            Log1 "EX-fr1=" & N(Frame1.hwnd) & "/in1=" & N(cmdIn1.hwnd) & "/parIn1=" & N(GetParent(cmdIn1.hwnd)) & "/eq=" & HexEq(GetParent(cmdIn1.hwnd), Frame1.hwnd)
            Log1 "EX-f2=" & N(Frame2.hwnd) & "/parDeep=" & N(GetParent(cmdDeep.hwnd)) & "/eqDeep=" & HexEq(GetParent(cmdDeep.hwnd), Frame2.hwnd)
            Log1 "EX-actIsMe=" & HexEq(GetActiveWindow(), Me.hwnd) & "/parFr1IsMe=" & HexEq(GetParent(Frame1.hwnd), Me.hwnd)
            Log1 "EX-in2vis=" & TF(IsWindowVisible(cmdIn2.hwnd)) & "/in2en=" & TF(IsWindowEnabled(cmdIn2.hwnd)) & "/in2tabstop=" & TF((GetWindowLong(cmdIn2.hwnd, -16) And 65536) <> 0)
        End If
        Call PostMessage(f, WM_KEYDOWN, VK_TAB, 0)
        Call PostMessage(f, WM_KEYUP, VK_TAB, 0)
        If AllStations() Then gAkDue = True
        Exit Sub
    End If

    ' 相 3：组内方向键 —— 程序化给 optA 焦点，post VK_DOWN，读跑到哪一枚。
    ' `AK-pre` 必须先读出来：如果 SetFocus 根本没落地，`down=` 那条读数就什么也不证明。
    If Not gAkPosted Then
        optA.SetFocus
        DoEvents
        gAkPre = WhereIs(GetFocus())
        Call PostMessage(GetFocus(), WM_KEYDOWN, VK_DOWN, 0)
        Call PostMessage(GetFocus(), WM_KEYUP, VK_DOWN, 0)
        gAkPosted = True
        Exit Sub
    End If

    ' 相 4：收尾读数。
    gAkPost = WhereIs(GetFocus())
    Log1 "AK-pre=" & gAkPre & "/down=" & gAkPost & "/optA=" & TF(optA.Value) & "/optB=" & TF(optB.Value)
    Log1 "TW-seq=" & gSeen
    Log1 "TW-new=" & CStr(gNew)
    Log1 "TW-ticks=" & CStr(gTick)
    Log1 "TW-orenter=" & TF(SeenIt("optA"))
    Log1 "TW-in1=" & TF(SeenIt("cmdIn1")) & "/in2=" & TF(SeenIt("cmdIn2"))
    Log1 "TW-deep=" & TF(SeenIt("cmdDeep")) & "/inpic=" & TF(SeenIt("cmdInPic"))
    Log1 "TW-picstop=" & TF(Not SeenIt("pic1"))
    Log1 "TW-top=" & TF(SeenIt("cmdTop1") And SeenIt("cmdTop2"))
    Log1 "TW-shy=" & TF(Not SeenIt("cmdShy"))
    Log1 "TW-walked=" & TF(gWalked)
    Log1 "TABWALK-DONE"
    tWalk.Enabled = False
    Unload Me
End Sub
