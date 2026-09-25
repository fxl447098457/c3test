Attribute VB_Name = "DemoBlocksDll"
Option Explicit

' ai/022 B18 端到端示例: CoClass 块 —— 组名 Shape 绑实现类 DemoShape, 默认接口 IDemoShape。
' DLL 形态: [ComCreatable(True)] 让它在外部注册表里可创建（B17 那条外部激活的靶子）。
CoClass Shape
    [Implementation("DemoShape")]
    [ComCreatable(True)]
    [Default] Interface IDemoShape
End CoClass
