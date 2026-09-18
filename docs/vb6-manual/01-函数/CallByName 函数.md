# CallByName 函数

**CallByName 函数**

       

执行一个对象的方法，或者设置或返回一个对象的属性。

**语法**

**CallByName(***object, procedurename, calltype,\[arguments()\]***)**

**CallByName** 函数的语法有以下部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *object* | 必需的；**变体型（对象）**。函数将要执行的对象的名称。 |
| *procedurename* | 必需的；**变体型（字符串）**。一个包含该对象的属性名称或者方法名称的字符串表达式。 |
| *calltype* | 必需的；**常数**。一个 **vbCallType** 类型的常数，代表正在被调用的过程的类型。 |
| *arguments()* | 可选的：**变体型（数组）**。 |

  

**说明**

**CallByName** 函数用于获取或者设置一个属性，或者在运行时使用一个字符串名称来调用一个方法。

在下面的例子中，第一行使用 **CallByName** 来设置一个文本框的 **MousePointer** 属性，第二行得到 **MousePointer** 属性的值，第三行调用 **Move** 方法来移动文本框：

    CallByName Text1, "MousePointer", vbLet, vbCrosshair
    Result = CallByName (Text1, "MousePointer", vbGet)
    CallByName Text1, "Move", vbMethod, 100, 100
