VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "DemoHolder"
Option Explicit

' ai/022 B18 端到端示例: Via 委托 —— 本类一个契约成员都不写, 三个槽由生成的适配器
' 转发到持有对象自己的接口表上。
Implements IDemoShape Via m_h

Private m_h As DemoShape

Public Sub Wire(ByVal h As DemoShape)
    Set m_h = h
End Sub
