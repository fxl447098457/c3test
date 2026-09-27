VERSION 5.00
Begin VB.Form WsForm 
   Caption         =   "WsForm"
   ClientHeight    =   4800
   ClientLeft      =   120
   ClientTop       =   465
   ClientWidth     =   9000
   LinkTopic       =   "WsForm"
   ScaleHeight     =   4800
   ScaleWidth      =   9000
   Begin MSWinsockLib.Winsock wsA 
      Left            =   120
      Protocol        =   1
      Top             =   120
   End
   Begin MSWinsockLib.Winsock wsB 
      Left            =   120
      Protocol        =   1
      Top             =   720
   End
   Begin MSWinsockLib.Winsock wsC 
      Left            =   120
      Top             =   1320
   End
   Begin VB.Timer evtTimer 
      Interval        =   120
      Left            =   4680
      Top             =   120
   End
End
Attribute VB_Name = "WsForm"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit

' ai/029 C29-WS-a：VB6 Winsock 控件走**原生 Winsock2**（不加载 MSWINSCK.OCX —— 它是 32 位
' inproc，x64 里 CoCreateInstance 直接失败，同 029 §三 D6）。本格 = 控件身份 + 状态机 +
' 属性面 + **UDP 一整轮**；TCP 的 Listen/Accept/Connect 在 WS-b，Error / SendProgress /
' Byte 数组那一形 GetData 在 WS-c。
'
' 三枚控件的分工：wsA / wsB = UDP 的一来一回（含"回信靠收来包的地址"这条 VB6 语义），
' wsC = .frm 里**什么都没写**的那枚 ⇒ 钉 Protocol / State / LocalPort 的原生默认值。
'
' 为什么一步一- tick（不是一路 DoEvents 泵到底）：网络通知本来就是"下一条消息"，
' 泵多少次全看这台机器多快 —— 第一版就是这么写的，结果 60 次 DoEvents 里对端的包还没到，
' 判据当场假红。改成 Timer 每 tick 只做一步、检查放在下一个 tick（间隔 120 ms，比回环
' 往返慢三个数量级），CI 上才是确定的。载入期间派发被 vb6_formLoading 挡着（DT-c / MV-c /
' RT-d 同先例），所以一切都在 Timer 里。
'
' 端口一律交给系统挑（Bind 0 + 读回 LocalPort）：不固定端口 ⇒ 不跟 CI 上别的作业抢号；
' 地址一律 127.0.0.1 ⇒ 不出本机、不看防火墙、不看有没有外网。

Private gArrB As Long           ' wsB 的 DataArrival 次数
Private gArrA As Long           ' wsA 的 DataArrival 次数
Private gTotB As Long           ' 最近一次 wsB 的 bytesTotal
Private gTotA As Long
Private gStB As Long            ' wsB 的 StateChanged 次数
Private gStLastB As Long        ' 最近一次 StateChanged 带回来的状态
Private gClsB As Long           ' wsB 的 Close 事件次数
Private gGotB As String         ' wsB 第一次 GetData 拿到的内容
Private gGot2 As String         ' 紧接着第二次 GetData 拿到的内容
Private gPeek As String         ' PeekData 拿到的内容
Private gGot3 As String         ' 再 GetData 拿到的内容
Private gGot4 As String         ' 两段连发拼回来的内容
Private gGot5 As String         ' 关掉之后"不该再来"的那一次取读
Private gPA As Long             ' 两枚控件各自被系统挑中的端口（跨 tick 要用，不能放局部）
Private gPB As Long

Private Function TF(ByVal ok As Boolean) As String
    If ok Then TF = "Y" Else TF = "N"
End Function

Private Sub Form_Load()
    ' --- 1..5 默认值：什么都没写的 wsC（Protocol 0 = sckTCPProtocol、State 0 = sckClosed、
    '     没 bind 过 ⇒ LocalPort 0 / LocalIP 空串），与 .frm 里写了 Protocol=1 的两枚对照 ---
    Debug.Print "WS1=" & TF(wsC.Protocol = 0)
    Debug.Print "WS2=" & TF(wsA.Protocol = 1 And wsB.Protocol = 1)
    Debug.Print "WS3=" & TF(wsC.State = 0)
    Debug.Print "WS4=" & TF(wsC.LocalPort = 0 And wsC.LocalIP = "")
    Debug.Print "WS5=" & TF(wsC.RemotePort = 0 And wsC.RemoteHost = "")
End Sub

' ---------------- C29-WS-a: UDP 一整轮（一步一 tick） ----------------
Private Sub evtTimer_Timer()
    Static step As Integer
    Dim s As String

    If step = 0 Then
        ' --- 6..8 Bind(0)：端口交给系统挑（没 bind 前 getsockname 是失败的 ⇒ 那时只能读 0），
        '      UDP 绑完的状态是 sckOpen = 1，而 StateChanged 就是那条转换的证人 ---
        wsB.Bind 0
        wsA.Bind 0
        gPB = wsB.LocalPort: gPA = wsA.LocalPort
        Debug.Print "WS6=" & TF(gPB > 0 And gPB < 65536)
        Debug.Print "WS7=" & TF(wsB.State = 1 And wsA.State = 1)
        Debug.Print "WS8=" & TF(gStB = 1 And gStLastB = 1)
        ' A → B 的第一趟
        wsA.RemoteHost = "127.0.0.1"
        wsA.RemotePort = gPB
        wsA.SendData "hello-B"
    ElseIf step = 1 Then
        ' --- 9..11 对端收到了一条 DataArrival，bytesTotal = 7 且字节真的在缓冲里 ---
        Debug.Print "WS9=" & TF(gArrB = 1 And wsB.BytesReceived = 7)
        Debug.Print "WS10=" & TF(gTotB = 7)
        Debug.Print "WS11=" & TF(wsB.LocalPort = gPB And wsA.LocalPort = gPA)
        wsB.GetData gGotB
        wsB.GetData gGot2
    ElseIf step = 2 Then
        ' --- 12..13 取走就消费掉；再取是空串（VB6 的 GetData 同口径，不是"读不到"而是"没了"）---
        Debug.Print "WS12=" & TF(gGotB = "hello-B" And wsB.BytesReceived = 0)
        Debug.Print "WS13=" & TF(gGot2 = "")
        wsA.SendData "peek-me"
    ElseIf step = 3 Then
        ' --- 14..15 PeekData 只看不取：BytesReceived 还在，第二次 GetData 拿到的还是那份 ---
        wsB.PeekData gPeek
        Debug.Print "WS14=" & TF(gPeek = "peek-me" And wsB.BytesReceived = 7)
        wsB.GetData gGot3
        Debug.Print "WS15=" & TF(gGot3 = "peek-me" And wsB.BytesReceived = 0)
        ' 连发两段：两笔都得拿回来。**几笔到达不算判据** —— 见下面 WS17 那段
        wsA.SendData "AB"
        wsA.SendData "CD"
    ElseIf step = 4 Then
        ' --- 16..17 VB6 那条"收到之后 RemoteHost 就是发件人"的语义：wsB 的 RemotePort 在 .frm
        '      里没写过（=0），收到 wsA 的包之后应当等于 A 自己那枚自动挑的端口 ⇒ 回信不用
        '      谁去告诉 A 它的端口是多少 ---
        Debug.Print "WS16=" & TF(wsB.RemotePort = gPA And gPA > 0)
        ' 两笔连发的判据问**内容**（"ABCD" 都在、取完就空），不问事件条数：同一对报文在
        ' x64 上落在同一次 FD_READ 里（本机 gArrB=3），在 CI 的 x86 作业上分成两次（gArrB=4）
        ' —— 合不合并只取决于那一次 drain 有没有抢到干净，是时序不是语义。第一版把 3 写成了
        ' 判据，门 #159 的 x86 那格就红在这里（证人面仍在 P= 那行报 gArrB）。
        wsB.GetData gGot4
        Debug.Print "WS17=" & TF(gGot4 = "ABCD" And wsB.BytesReceived = 0)
        wsB.RemoteHost = "127.0.0.1"
        wsB.RemotePort = gPA
        wsB.SendData "hi-A"
    ElseIf step = 5 Then
        ' --- 18..19 回信到了 A：A 的 DataArrival + A 那侧也把发件地址记了下来（同一族语义）---
        Debug.Print "WS18=" & TF(gArrA >= 1 And gTotA = 4)
        Debug.Print "WS19=" & TF(wsA.RemoteHost = "127.0.0.1" And wsA.RemotePort = gPB)
        wsA.GetData s
        Debug.Print "WS20=" & TF(s = "hi-A")
        ' --- 20..22 Close：状态回 sckClosed；之后再发不该炸、也不该有到达 ---
        wsA.Close
        Debug.Print "WS21=" & TF(wsA.State = 0)
        wsA.RemoteHost = "127.0.0.1"
        wsA.RemotePort = gPB
        wsA.SendData "after-close"
    ElseIf step = 6 Then
        ' --- 22..24 关掉之后再发：不该有东西到达 wsB（取回来还是空串），也不该有 Close 事件 ---
        wsB.GetData gGot5
        Debug.Print "WS22=" & TF(gGot5 = "" And wsB.BytesReceived = 0)
        Debug.Print "WS23=" & TF(wsA.State = 0 And gClsB = 0)
        ' --- 23..24 两条通道各自独立：wsA 的一整轮里 wsB 的 Close 事件一次都不该来；
        '      而 Close 之后 wsA 的 LocalPort 读数保持它绑过的那个（VB6：Close 不清身份）---
        Debug.Print "WS24=" & TF(wsA.LocalPort = gPA)
        Debug.Print "P=" & gPA & "/" & gPB & "/" & gStB & "/" & gArrA & "/" & gArrB & "/" & gTotA
        Debug.Print "CTRLWINSOCK-DONE"
        Unload Me
    End If
    step = step + 1
End Sub

Private Sub wsA_DataArrival(ByVal bytesTotal As Long)
    gArrA = gArrA + 1
    gTotA = gTotA + bytesTotal
End Sub

Private Sub wsB_DataArrival(ByVal bytesTotal As Long)
    gArrB = gArrB + 1
    gTotB = gTotB + bytesTotal
End Sub

Private Sub wsB_StateChanged(ByVal State As Integer)
    gStB = gStB + 1
    gStLastB = State
End Sub

Private Sub wsB_Close()
    gClsB = gClsB + 1
End Sub
