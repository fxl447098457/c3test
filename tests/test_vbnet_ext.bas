Option Explicit

' <vbeclipse> ai/032 = VB.NET 那批运算符与整型扩展 (用户点名要的那一套)。
' 本用例钉的是"真按 VB.NET/VB6 口径算了值", 不是"解析器没报错" ——
' 语法通过 ≠ 取值正确: 本轮全部实现都是在语义/发码层才发现真值差的。
'
' 覆盖:
'   · 位移 Shl/Shr (含优先级夹逼、算术右移、逻辑右移、计数屏蔽、溢出回绕)
'   · 复合赋值 += -= *= /= \= ^= &= <<= >>= Mod=
'   · IsNot (脱糖成 Not .. Is ..)
'   · If 三元 (VB.NET 运算符 = **短路**, 与 IIf 函数 = 两支都求值, 是两条路)
'   · 新整型 SByte/UInteger/ULong/ULongLong 参与算术/位运算/比较/打印
'   · 转换函数 CSByte/CUInt/CULng/CULngLng (+ BSTR 入口与溢出报错)
'
' **promote 那一段是本用例的重点负控。** 给这 4 个类型补 promote 档位时踩过一个坑:
' 64 位整型 (LongLong/ULongLong) 按位宽排在 Single(4)/Double(5)/Currency(6) *之上*,
' 而 promote 是"取 rank 大者", 于是 `u3 + 1.5` 被判成整型 → 上层 CStr 选整型入口
' → (uint64_t)5.5 = 5, 小数被吃掉。实测 (修前): u3_plus_dbl=5 / lp_plus_dbl=4 /
' lp_mul_dbl=7。修法是 promote 里加"浮点/货币/Decimal 侧必胜, 与位宽无关"
' (对改动前的类型表是恒等变换), 并让 LongPtr **刻意不登记** ——
' 它的宽度是目标相关的 (x86 4 / x64 8), 而 TypeSystem 拿不到目标架构,
' 写死任何一档都会错 (写 6 时实测 `Dim p As LongPtr: p=3` 后 CStr(p + 1.5) 出 4)。
' VE-promote-* 五条就是拦这个反向错的; 其中 VE-promote-ll-wide 同时钉住
' "LongLong + Long 必须保住高 32 位"(改前 rank 落在 default 0 档 → 退化成 Long)。

Private gU As ULong
Private gU3 As ULongLong

Public Sub Main()
    Dim x As Long

    ' ---- 位移 ----
    Debug.Print "VE-shl-1-4=" & CStr(1 << 4)              ' 16
    Debug.Print "VE-shl-256-4=" & CStr(256 >> 4)          ' 16
    ' 优先级: 算术高于移位 ⇒ 1 << (2+3) = 32; 若错分成 (1<<2)+3 则为 7
    Debug.Print "VE-shl-prec-arith=" & CStr(1 << 2 + 3)
    ' 优先级: 移位高于比较 ⇒ (1<<1) = 2 成立
    If 1 << 1 = 2 Then
        Debug.Print "VE-shl-prec-cmp=T"
    Else
        Debug.Print "VE-shl-prec-cmp=F"
    End If
    ' 整除高于移位 ⇒ (16\2) << 1 = 16; 若错分成 16\(2<<1) 则为 4
    Debug.Print "VE-shl-prec-intdiv=" & CStr(16 \ 2 << 1)
    ' 右移是算术右移 (补符号位): -16 >> 2 = -4
    x = -16
    Debug.Print "VE-shr-signed=" & CStr(x >> 2)
    ' 左移溢出按位模式 wrap (不是 UB): &H80000000 << 1 的低 32 位 = 0
    x = &H80000000
    Debug.Print "VE-shl-wrap=" & CStr(x << 1)
    ' 计数屏蔽 + 结果至少 32 位: 1 << 20 = 1048576 (Integer 是 16 位, 不抬位会得 0)
    Debug.Print "VE-shl-mask=" & CStr(1 << 20)

    ' ---- 复合赋值 ----
    x = 10 : x += 5 : Debug.Print "VE-add-eq=" & CStr(x)      ' 15
    x = 10 : x -= 5 : Debug.Print "VE-sub-eq=" & CStr(x)      ' 5
    x = 10 : x *= 5 : Debug.Print "VE-mul-eq=" & CStr(x)      ' 50
    x = 10 : x \= 3 : Debug.Print "VE-intdiv-eq=" & CStr(x)   ' 3
    x = 10 : x ^= 2 : Debug.Print "VE-pow-eq=" & CStr(x)      ' 100
    x = 10 : x Mod= 3 : Debug.Print "VE-mod-eq=" & CStr(x)    ' 1
    x = 1  : x <<= 3 : Debug.Print "VE-shleq=" & CStr(x)      ' 8
    x = 16 : x >>= 2 : Debug.Print "VE-shreq=" & CStr(x)      ' 4
    ' /= 走 VB6 的浮点除, 结果是 Double —— 目标变量必须是 Double, 否则窄化
    Dim dx As Double
    dx = 10 : dx /= 4 : Debug.Print "VE-div-eq=" & CStr(dx)   ' 2.5
    ' &= 是字符串连接, 不是数值加
    Dim s As String
    s = "a" : s &= "b" : Debug.Print "VE-amp-eq=" & s         ' ab

    ' ---- IsNot ----
    Dim o As Object
    Set o = Nothing
    If o IsNot Nothing Then
        Debug.Print "VE-isnot-nothing=T"
    Else
        Debug.Print "VE-isnot-nothing=F"                      ' 期望 F
    End If
    If Not (o IsNot Nothing) Then
        Debug.Print "VE-isnot-negated=T"                      ' 期望 T (证明它是布尔, 不是值比较)
    Else
        Debug.Print "VE-isnot-negated=F"
    End If

    ' ---- If 三元 ----
    Debug.Print "VE-if-basic=" & CStr(If(5 >= 0, 5, -5))                  ' 5
    Debug.Print "VE-if-neg=" & CStr(If(-5 >= 0, -5, 5))                   ' 5
    Debug.Print "VE-if-nested=" & If(0 > 0, "p", If(0 = 0, "z", "n"))     ' z
    Debug.Print "VE-if-in-cond=" & CStr(If(1 = 1, 7, 9))                  ' 7
    ' 关键判据: 短路。d = 0 时 If 只算被选中那一支 ⇒ `100 / d` 不该被求值。
    Dim d As Long
    d = 0
    Debug.Print "VE-if-short-circuit=" & CStr(If(d <> 0, 100 / d, 42))    ' 42
    ' 对照: 同一表达式换成 IIf **会**除零 (函数语义, 两支都先算好) ——
    ' 这一条同时说明 If 与 IIf 是两条不同的路, 不能合并实现。
    On Error GoTo IIfBoom
    Debug.Print "VE-iif-eager=" & CStr(IIf(d <> 0, 100 / d, 42))
    On Error GoTo 0
    GoTo AfterIIf
IIfBoom:
    Debug.Print "VE-iif-eager=ERROR" & CStr(Err.Number)                   ' ERROR11
    On Error GoTo 0
AfterIIf:

    ' ---- 新整型的边界与回绕 ----
    Dim u As ULong
    Dim u3 As ULongLong
    Dim w As UInteger
    Dim sb As SByte
    u = &HFFFFFFFF
    Debug.Print "VE-ulong-max=" & CStr(u)                 ' 4294967295 (不是 -1)
    u = u + 1
    Debug.Print "VE-ulong-wrap=" & CStr(u)                ' 0
    ' 无符号右移是**逻辑**右移 (高位补 0)
    u = &H80000000
    Debug.Print "VE-ulong-shr-logical=" & CStr(u >> 4)    ' 134217728
    u3 = 65536
    u3 = u3 * u3
    Debug.Print "VE-ulonglong-mul=" & CStr(u3)            ' 4294967296 (不能在 32 位处截断)
    w = 65535
    Debug.Print "VE-uinteger-max=" & CStr(w)              ' 65535
    sb = -128
    Debug.Print "VE-sbyte-min=" & CStr(sb)                ' -128
    w = 65535 : w = w + 1
    Debug.Print "VE-uinteger-wrap=" & CStr(w)             ' 0 (按 16 位截断)
    sb = 127 : sb = sb + 1
    Debug.Print "VE-sbyte-wrap=" & CStr(sb)               ' -128 (按 8 位截断)

    ' ---- 无符号参与运算 ----
    u = 3000000000 : u = u + 1000000000
    Debug.Print "VE-op-u-add=" & CStr(u)                  ' 4000000000
    u = 4000000000 : u = u - 1000000000
    Debug.Print "VE-op-u-sub=" & CStr(u)                  ' 3000000000
    u = 100000 : u = u * 100000
    Debug.Print "VE-op-u-mul=" & CStr(u)                  ' 1410065408 (32 位回绕)
    ' 无符号整除: &H80000000 \ 2 = 1073741824 (按有符号会得 3221225472)
    u = &H80000000 : u = u \ 2
    Debug.Print "VE-op-u-intdiv=" & CStr(u)
    u = 4000000001 : u = u Mod 1000000000
    Debug.Print "VE-op-u-mod=" & CStr(u)                  ' 1
    ' 位运算: 高位置 1 时仍须按无符号读数 (按 (int32_t) 收口会把高 32 位整段丢掉)
    u = &HFF00FF00 : u = u And &H0F0F0F0F
    Debug.Print "VE-op-u-and=" & CStr(u)                  ' 251662080 (&H0F000F00)
    u = &HFF00FF00 : u = u Or &H000000FF
    Debug.Print "VE-op-u-or=" & CStr(u)                   ' 4278255615 (&HFF00FFFF)
    u = &HFF00FF00 : u = u Xor &HFFFFFFFF
    Debug.Print "VE-op-u-xor=" & CStr(u)                  ' 16711935 (&H00FF00FF)
    ' 无符号比较: &HFFFFFFFF 必须 > 0 (按有符号会当成 -1)
    u = &HFFFFFFFF
    If u > 0 Then
        Debug.Print "VE-op-u-cmp-gt=T"                    ' 期望 T
    Else
        Debug.Print "VE-op-u-cmp-gt=F"
    End If
    If u < 1 Then
        Debug.Print "VE-op-u-cmp-lt1=T"
    Else
        Debug.Print "VE-op-u-cmp-lt1=F"                   ' 期望 F
    End If
    ' 64 位无符号
    u3 = 1 : u3 = u3 << 40
    u3 = u3 - 1
    Debug.Print "VE-op-u3-sub=" & CStr(u3)                ' 1099511627775
    u3 = 1099511627776 : u3 = u3 \ 2
    Debug.Print "VE-op-u3-intdiv=" & CStr(u3)             ' 549755813888
    u3 = 1099511627777 : u3 = u3 Mod 1099511627776
    Debug.Print "VE-op-u3-mod=" & CStr(u3)                ' 1
    u3 = 1 : u3 = u3 << 40 : u3 = u3 Or 255
    Debug.Print "VE-op-u3-or=" & CStr(u3)                 ' 1099511628031
    u3 = 1 : u3 = u3 << 40 : u3 = u3 And 0
    Debug.Print "VE-op-u3-and=" & CStr(u3)                ' 0

    ' ---- 转换函数 ----
    Debug.Print "VE-conv-csbyte=" & CStr(CSByte(100))              ' 100
    Debug.Print "VE-conv-cuint=" & CStr(CUInt(65535))              ' 65535
    Debug.Print "VE-conv-culng=" & CStr(CULng(4294967295))         ' 4294967295
    Debug.Print "VE-conv-culnglng=" & CStr(CULngLng(4294967296))   ' 4294967296
    ' 名字大小写不敏感 (VB6/VB.NET 通例)
    Debug.Print "VE-conv-case=" & CStr(culng(7))                   ' 7
    ' 转换结果直接参与运算
    Debug.Print "VE-conv-in-expr=" & CStr(CULng(4000000000) + 294967295)   ' 4294967295

    ' ---- 模块级无符号 (走 symTab 兜底路, 与局部变量不是同一条) + Debug.Print 分派 ----
    gU = &HFFFFFFFF
    Debug.Print "VE-mod-ulong=" & CStr(gU)                ' 4294967295
    Debug.Print "VE-dbg-ulong="; gU                       ' 4294967295
    gU = 4000000000 : gU = gU \ 2
    Debug.Print "VE-mod-ulong-intdiv=" & CStr(gU)         ' 2000000000
    gU3 = 1 : gU3 = gU3 << 40
    Debug.Print "VE-mod-ulonglong=" & CStr(gU3)           ' 1099511627776
    Debug.Print "VE-dbg-ulonglong="; gU3                  ' 1099511627776

    ' ---- promote 负控: 64 位整型/指针与浮点/货币混合, 小数**不能**被截掉 ----
    ' 见文件头注释: rank 按位宽取档会把 64 位整型排到 Double/Currency 之上,
    ' 一旦 promote 答整型, 这几条就会打成 5 / 4 / 7 / 4 而不是 5.5 / 4.5 / 7.5 / 4.5。
    Dim p As LongPtr
    Dim ll As LongLong
    Dim u3w As ULongLong
    Dim l As Long
    p = 3
    ll = 4
    u3w = 4
    l = 4
    Debug.Print "VE-promote-lp-dbl=" & CStr(p + 1.5)        ' 4.5 (LongPtr: 刻意不登记档位)
    Debug.Print "VE-promote-lp-mul=" & CStr(p * 2.5)        ' 7.5
    Debug.Print "VE-promote-ll-dbl=" & CStr(ll + 1.5)       ' 5.5
    Debug.Print "VE-promote-ll-cur=" & CStr(ll + CCur(1.5)) ' 5.5 (Currency 侧也必胜)
    Debug.Print "VE-promote-u3-dbl=" & CStr(u3w + 1.5)      ' 5.5
    Debug.Print "VE-promote-u3-cur=" & CStr(u3w + CCur(1.5))' 5.5
    Debug.Print "VE-promote-dbl-ll=" & CStr(1.5 + ll)       ' 5.5 (换边也是浮点胜)
    ' 反向: 64 位整型 + 32 位整型必须**保住高 32 位** (改前退化成 Long)
    ll = 4294967296
    Debug.Print "VE-promote-ll-wide=" & CStr(ll + l)        ' 4294967300

    Debug.Print "VE-DONE"
End Sub
