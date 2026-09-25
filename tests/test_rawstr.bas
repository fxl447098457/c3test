Option Explicit

' ai/028 V1 回归: 反引号原始多行串 (C3 扩展; 真 VB6 没有这个语法)。
' 每条断言都拿"与手写的 VB6 串逐字相等"来钉 —— 归一发生在词法出口, 所以这些读数
' 同时是 R4 的证据: AST 里根本没有第二种字符串。

' R4 消费点之一: Const 的串值由语义层自己剥引号、自己折 ""
Const RS_CONST = `k1
k2 = "v"`

' R4 消费点之二: Declare 的 Lib 名走同一份 rawText (后端 stripQuotes)。
' 名字必须是 RTL 里已存在的 DI 桩 (vb6_di_GetTickCount)，而桩所属的**家族**就是按
' Lib 串选的 (发码里那行 /* vb6_di_lib: kernel32 */) —— Lib 名没折对就 LNK2019。
Declare Function GetTickCount Lib `kernel32` () As Long

Sub Main()
    Dim a As String

    ' 跨行: 行界归一成 CRLF, 与源码行尾风格无关
    a = `line1
line2`
    chk "RS01", a, "line1" & vbCrLf & "line2"

    ' 起始反引号后紧跟的那一个换行裁掉, 第二个换行是内容
    a = `
x
y`
    chk "RS02", a, "x" & vbCrLf & "y"
    chk "RS03", CStr(Len(a)), "4"

    ' 只裁开头那一枚: 以换行结尾就自己留着
    a = `
p
`
    chk "RS04", a, "p" & vbCrLf

    ' 串内双引号原样, 不需要双写 (这是这条语法的第一价值)
    a = `He said "hi" to "you"`
    chk "RS05", a, "He said ""hi"" to ""you"""

    ' 反引号本身靠双写
    a = `a``b`
    chk "RS06", a, "a" & Chr(96) & "b"

    ' 零转义: 反斜杠就是反斜杠, \n 是两个字符
    a = `C:\note\{x}\n`
    chk "RS07", a, "C:\note\{x}\n"
    chk "RS08", CStr(Len(a)), "13"

    ' 行尾的下划线在串内是普通文本: 既不续行也不吞换行
    a = `first _
second`
    chk "RS09", a, "first _" & vbCrLf & "second"

    ' 串内以 # 开头的行不会被预处理器当成指令 (嵌 shell / Makefile 的决定性性质)
    a = `CFLAGS = -O2
# This is not a directive
X`
    chk "RS10", a, "CFLAGS = -O2" & vbCrLf & "# This is not a directive" & vbCrLf & "X"

    ' 单行反引号串 = 同一个 token 的退化情形
    a = `plain "quoted" text`
    chk "RS11", a, "plain ""quoted"" text"

    ' 非 ASCII: 与手写的中文逐字相等, 长度按 UTF-16 码元计 (6 + 2 + 9 = 17)
    a = `姓名: 张三
备注: "vip"`
    chk "RS12", a, "姓名: 张三" & vbCrLf & "备注: ""vip"""
    chk "RS13", CStr(Len(a)), "17"

    ' Const 落点: 值里同时有换行与引号
    chk "RS14", RS_CONST, "k1" & vbCrLf & "k2 = ""v"""

    ' Declare 落点真跑一次 —— 桩所属家族按 Lib 串选, 名字没折对就 LNK2019。
    ' (不用 CStr(比较式) 做读数: 今天 C3 的 CStr(True) 回的是 "-1"，见本轮记录。)
    Dim tick As Long
    tick = GetTickCount()
    If tick <> 0 Then
        Debug.Print "RS15=OK"
    Else
        Debug.Print "RS15=BAD tick=0"
    End If

    ' 定界计数与 VB6 的 "" 同规: 两枚 = 空串 (VB6 里 "" 也是空串)
    Dim e As String
    e = ``
    chk "RS16", CStr(Len(e)), "0"

    ' 一枚字面反引号要写四枚 (开头 + 双写 + 闭合) —— 与 VB6 写一个引号要 """" 同理
    Dim one As String
    one = ````
    chk "RS17", one, Chr(96)
    chk "RS18", CStr(Len(one)), "1"

    Debug.Print "RAWSTR-DONE"
End Sub

Private Sub chk(n As String, got As String, want As String)
    If got = want Then
        Debug.Print n & "=OK"
    Else
        Debug.Print n & "=BAD got=[" & got & "] want=[" & want & "]"
    End If
End Sub
