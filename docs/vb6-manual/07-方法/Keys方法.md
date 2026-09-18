# Keys方法

# Keys方法

           

**描述**

返回一个数组，该数组包含一个 **Dictionary** 对象中的全部已有的关键字。

**语法**

*object*.**Keys**

*object*始终是一个 **Dictionary** 对象的名字。

**说明**

下面的代码举例说明了 **Keys** 方法的使用。

    Dim a, d, i             '创建一些变量
    Set d = CreateObject("Scripting.Dictionary")
    d.Add "a", "Athens"     '添加一些关键字和条目。
    d.Add "b", "Belgrade"
    d.Add "c", "Cairo"
    a = d.keys              '取得关键字
    For i = 0 To d.Count -1 '重复数组
        Print a(i)          '打印关键字
    Next
    ...
