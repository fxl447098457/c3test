Attribute VB_Name = "DemoBlocks"
Option Explicit

' ai/022 B18 端到端示例: CoClass 块 —— 组名 Shape 绑实现类 DemoShape, 默认接口 IDemoShape。
' EXE 形态: 不写 [ComCreatable] —— 只有 ActiveX DLL 才注册 COM 服务器（EXE 保留组内那一半）。
CoClass Shape
    [Implementation("DemoShape")]
    [Default] Interface IDemoShape
End CoClass
