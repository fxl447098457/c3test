Attribute VB_Name = "RsBlocks"
Option Explicit

' ai/028 V1: 一个只写 [Implementation] 的最小 CoClass 块, 给 RsMain 的
' CreateObject(工程内 ProgID) 提供改写目标 —— ProgID 是按 unquote(rawText) 比对的,
' 所以 ProgID 写成反引号串也必须命中同一条改写。
CoClass Greeter
    [Implementation("RsGreeter")]
End CoClass
