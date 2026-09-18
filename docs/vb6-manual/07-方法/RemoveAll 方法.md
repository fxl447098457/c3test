# RemoveAll 方法

# RemoveAll 方法

           

**描述**

**RemoveAll** 方法从 **Dictionary** 对象中删除所有关键字和条目对。

**语法**

*object*.**RemoveAll**

*object*始终是一个 **Dictionary** 对象的名字。

**说明**

下面的代码举例说明了 **RemoveAll** 方法的用法：

    Dim a, d, i             '创建一些变量
    Set d = CreateObject("Scripting.Dictionary")
    d.Add "a", "Athens"     '添加一些关键字和条目
    d.Add "b", "Belgrade"
    d.Add "c", "Cairo"
    ...
    a = d.RemoveAll         '清除字典
