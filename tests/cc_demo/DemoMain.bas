Attribute VB_Name = "DemoMain"
Option Explicit

' ai/022 B18 端到端示例: 语言层断言（x64 与 x86 各跑一遍）。
' 把 P3/P6 那条线的东西用一遍: 接口 + Implements(+ Via 委托) + Inherits/Overrides/Protected/MyBase
' + CoClass 块 + `As <块名>`/`New <块名>`/`CreateObject(ProgID)` + TypeOf。
' 契约成员是 Private ⇒ 只能经**接口变量**调（类的默认面上点不到, 这是 VB6 语义）。
Sub Main()
    ' --- 接口链: 组名当类型用 = 绑到 [Implementation] 那个类 ---
    Dim s As Shape
    Set s = New Shape
    Dim itf As IDemoShape
    Set itf = s
    itf.Move 3#
    If itf.Area() = 6# Then
        Debug.Print "DEMO1:OK"
    Else
        Debug.Print "DEMO1:FAIL area=" & itf.Area()
    End If
    If itf.Label() = "shape" Then
        Debug.Print "DEMO2:OK"
    Else
        Debug.Print "DEMO2:FAIL label=" & itf.Label()
    End If
    If s.Twice(21) = 42 Then
        Debug.Print "DEMO3:OK"            ' 公有成员（外部晚绑定也能点这一个）
    Else
        Debug.Print "DEMO3:FAIL twice=" & s.Twice(21)
    End If

    ' --- 继承链: Overrides + 虚派发 + MyBase 去虚化 + Protected 家族内可用 ---
    Dim d As DemoDerived
    Set d = New DemoDerived
    If d.Speak() = "derived:base" Then
        Debug.Print "DEMO4:OK"
    Else
        Debug.Print "DEMO4:FAIL speak=" & d.Speak()
    End If
    If d.BaseSpeak() = "base" Then
        Debug.Print "DEMO5:OK"            ' MyBase: 直调基类实现, 不查表
    Else
        Debug.Print "DEMO5:FAIL base=" & d.BaseSpeak()
    End If
    d.Bump
    d.Bump
    If d.Hits() = 2 Then
        Debug.Print "DEMO6:OK"            ' 继承来的公有成员
    Else
        Debug.Print "DEMO6:FAIL hits=" & d.Hits()
    End If
    If d.ShiftSeen() = 2# Then
        Debug.Print "DEMO7:OK"            ' Protected 成员在派生类里可用（Class_Initialize 里设的）
    Else
        Debug.Print "DEMO7:FAIL shift=" & d.ShiftSeen()
    End If
    ' 基类型变量持有派生实例: 不切片, 虚派发仍落到派生实现
    Dim b As DemoBase
    Set b = d
    If b.Speak() = "derived:base" Then
        Debug.Print "DEMO8:OK"
    Else
        Debug.Print "DEMO8:FAIL speak=" & b.Speak()
    End If

    ' --- Via 委托: Holder 自己一个契约成员都不写, 三个槽全由适配器转发 ---
    Dim h As DemoHolder
    Set h = New DemoHolder
    h.Wire s
    Dim itf2 As IDemoShape
    Set itf2 = h
    itf2.Move 1#
    If itf2.Area() = 8# Then
        Debug.Print "DEMO9:OK"
    Else
        Debug.Print "DEMO9:FAIL area=" & itf2.Area()
    End If
    If itf2.Label() = "shape" Then
        Debug.Print "DEMO10:OK"
    Else
        Debug.Print "DEMO10:FAIL label=" & itf2.Label()
    End If

    ' --- 工程内激活: CreateObject(组 ProgID) 编译期改写 + TypeOf 判定 ---
    Dim o As Object
    Set o = CreateObject("DemoApp.Shape")
    If o Is Nothing Then
        Debug.Print "DEMO11:FAIL null"
    Else
        Debug.Print "DEMO11:OK"
    End If
    If TypeOf s Is IDemoShape Then
        Debug.Print "DEMO12:OK"
    Else
        Debug.Print "DEMO12:FAIL typeof"
    End If

    Debug.Print "DEMO-DONE"
End Sub
