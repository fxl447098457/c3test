Attribute VB_Name = "LvMod"
Option Explicit

' 账 #612: 定义在标准模块里、被 .frm 引用 → 走 driver 的跨模块预注册。
' 预注册发生在 Pass 1 之前, 那时模块级 Const 还没进符号表, 若求值器不扫一遍
' 声明, 枚举成员就会回落成声明序号 (0,1,2...), 引用侧拿到的是错的位。
Public Const LSX2_FULL As Long = &H20
Public Const LSX2_TRACK As Long = &H8

Public Enum LvFlag2
    Flag2Full = LSX2_FULL
    Flag2Track = LSX2_TRACK
    Flag2Both = Flag2Full Or Flag2Track
End Enum
