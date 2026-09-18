# Count 属性

# Count 属性

           

**描述**

返回集合或 **Dictionary** 对象中的条目数。只读。

**语法**

*object*.**Count**

*object* 总是“应用于”列表中某一项的名称。

**说明**

下面的代码举例说明了 **Count** 属性的使用方法：

    Dim a, d, i             '创建一些变量
    Set d = CreateObject("Scripting.Dictionary")
    d.Add "a", "Athens"     '添加一些关键字和条目。
    d.Add "b", "Belgrade"
    d.Add "c", "Cairo"
    a = d.Keys              '获得关键字
    For i = 0 To d.Count -1 '遍及数组
        Print a(i)          '打印关键字
    Next
    ...
