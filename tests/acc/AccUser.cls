VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "AccUser"
Option Explicit

' 跨类访问: 只许 Public / Friend (同工程), Private 会被 3028 拦下
Public Sub Use(t As AccThing)
    t.Setup 6
    t.Friendly
    Debug.Print "ACC-USE:" & CStr(t.Value())
End Sub
