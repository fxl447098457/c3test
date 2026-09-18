# Dictionary 对象

# Dictionary 对象

                   

**描述**

对象，用于存储数据关键字和条目对。

**语法**

**Scripting.Dictionary**

**说明**

**Dictionary** 对象与 PERL 关联数组等价。可以是任何形式的数据的条目被存储在数组中。每个条目都与一个唯一的关键字相关联。该关键字用来检索单个条目，通常是整数或字符串，可以是除数组外的任何类型。

下面的代码举例说明了如何创建一个 **Dictionary** 对象：

    Dim d                   '创建一个变量
    Set d = CreateObject(Scripting.Dictionary)
    d.Add "a", "Athens"     '添加一些关键字和条目
    d.Add "b", "Belgrade"
    d.Add "c", "Cairo"
    ...
