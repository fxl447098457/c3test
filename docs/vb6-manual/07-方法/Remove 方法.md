# Remove 方法

# Remove 方法

           

**描述**

从一个 **Dictionary** 对象中删除一个关键字和条目对。

**语法**

*object*.**Remove(***key***)**

**Remove** 方法语法有如下几部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *Object* | 必需的。始终是一个 **Dictionary** 对象的名字。 |
| *Key* | 必需的。*Key* 与要从 **Dictionary** 对象中删除的关键字和条目对相关联。 |

  

**说明**

如果指定的关键字和条目对不存在，则发生一个错误。

下面的代码举例说明了 **Remove** 方法的使用：

    Dim a, d, i             '创建一些变量
    Set d = CreateObject("Scripting.Dictionary")
    d.Add "a", "Athens"     '添加一些关键字和条目
    d.Add "b", "Belgrade"
    d.Add "c", "Cairo"
    ...
    a = d.Remove()          '删除第二对
