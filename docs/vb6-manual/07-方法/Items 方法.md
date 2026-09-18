# Items 方法

# Items 方法

           

**描述**

返回一个包含 **Dictionary** 对象中所有条目的数组。

**语法**

*object*.**Items**

*object*始终是一个 **Dictionary** 对象的名字。

**说明**

下面的代码举例说明了 **Items** 方法的使用。:

    Dim a, d, i             '创建一些变量
    Set d = CreateObject("Scripting.Dictionary")
    d.Add "a", "Athens"     '添加一些关键字和条目。
    d.Add "b", "Belgrade"
    d.Add "c", "Cairo"
    a = d.Items             '取得条目
    For i = 0 To d.Count -1 '重复数组
        Print a(i)          '打印条目
    Next
    ...
