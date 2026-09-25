Attribute VB_Name = "CnMain"
Option Explicit

' ai/022 B19: 控制台输出的编码靶子 —— 四种语言一次打全。
' 本文件刻意存成 **UTF-8 with BOM**: C3 认 BOM, 而 GBK 存不下韩文。
Sub Main()
    Debug.Print "中文"
    Debug.Print "日本語 テスト"
    Debug.Print "한국어 테스트"
    Debug.Print "English test"
End Sub
