Option Explicit

' 账 #305 §B218 (甲) 刀1 的行为尺: 每格窗口先记下 Made/Terms 的起点, 只打差值,
' 这样各格之间的存活实例不会互相把对方的出厂次数算进自己头上。
' C445: Class_Initialize 里 Main.Made +1, Class_Terminate 里 Main.Terms +1, N 恒 7。
Public Made As Long
Public Terms As Long
Private m_auto As New C445
Public m_plain As C445

' W1 = 腿③ 置空: 局部那一枚交出去之前自己先放手。
Sub W1SetNothing()
    Dim a As C445
    Dim t0 As Long
    Dim isNull As Long
    t0 = Terms
    Set a = New C445
    Set a = Nothing
    If a Is Nothing Then isNull = 1
    Debug.Print "W1-terms=" & (Terms - t0) & " null=" & isNull
End Sub

' W2 = R3 别名双退: Set b = a 之后 a 置空, b 必须还活着 (只多一枚引用, 不当场拆)。
Sub W2Alias()
    Dim a As C445
    Dim b As C445
    Dim t0 As Long
    t0 = Terms
    Set a = New C445
    Set b = a
    Set a = Nothing
    Debug.Print "W2-terms=" & (Terms - t0) & " n=" & b.N
End Sub

' W3 = R2 自赋值: 先 claim 后 release, 同一枚不会把自己拆掉。
Sub W3SelfSet()
    Dim a As C445
    Dim t0 As Long
    Dim n0 As Long
    t0 = Terms
    Set a = New C445
    Set a = a
    n0 = a.N
    Set a = Nothing
    Debug.Print "W3-terms=" & (Terms - t0) & " n=" & n0
End Sub

' W4 = 腿② 覆写模块级槽: 每次覆写放手旧的那一枚 (今天这条腿压根没有)。
Sub W4ModuleSlot()
    Dim t0 As Long
    Dim isNull As Long
    t0 = Terms
    Set m_plain = New C445
    Set m_plain = New C445
    Set m_plain = Nothing
    If m_plain Is Nothing Then isNull = 1
    Debug.Print "W4-terms=" & (Terms - t0) & " null=" & isNull
End Sub

' W5 = R6 As New 的守卫: 用到才造, 读两次只造一枚, 而且守卫那条腿不许 claim。
Sub W5AutoNew()
    Dim m0 As Long
    Dim x As Long
    Dim y As Long
    m0 = Made
    x = m_auto.N
    y = m_auto.N
    Debug.Print "W5-made=" & (Made - m0) & " sum=" & (x + y)
End Sub

' W6 = 形⑤ 工厂返回: 被调方交出的是它自己那枚引用, 调用方接手后必须还活着。
Public Function MakeOne() As C445
    Dim t As C445
    Set t = New C445
    Set MakeOne = t
End Function

Sub W6Factory()
    Dim c As C445
    Dim t0 As Long
    t0 = Terms
    Set c = MakeOne()
    Debug.Print "W6-terms=" & (Terms - t0) & " n=" & c.N
    Set c = Nothing
End Sub

' W7 = 形参那一档 (今天刻意留在计数系统外面): 被调方里的 Set 形参 = Nothing
' 不许拆掉调用方还在用的那一枚 —— 这条就是 procClassFormals_ 那面夹的行为针。
Public Sub DropByVal(ByVal o As C445)
    Set o = Nothing
End Sub

Sub W7ParamBorrow()
    Dim a As C445
    Dim t0 As Long
    t0 = Terms
    Set a = New C445
    DropByVal a
    Debug.Print "W7-terms=" & (Terms - t0) & " n=" & a.N
    Set a = Nothing
End Sub

Sub Main()
    W1SetNothing
    W2Alias
    W3SelfSet
    W4ModuleSlot
    W5AutoNew
    W6Factory
    W7ParamBorrow
    Debug.Print "B445-DONE"
End Sub
