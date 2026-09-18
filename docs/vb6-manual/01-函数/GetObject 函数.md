# GetObject 函数

**GetObject 函数**

       

返回文件中的 ActiveX 对象的引用。

**语法**

**GetObject(**\[***pathname***\] \[**, *class***\]**)**

**GetObject** 函数的语法包含下面几个命名参数：

|  |  |
|----|----|
| **部分** | **描述** |
| ***pathname*** | 可选的；**Variant** (**String**)。包含待检索对象的文件的全路径和名称。如果省略 ***pathname***，则 ***class*** 是必需的。 |
| ***class*** | 可选的；**Variant** (**String**)。代表该对象的类的字符串。 |

  

其中，***class*** 参数的语法格式为 *appname***.***objecttype*，且语法的各个部分如下：

|  |  |
|----|----|
| **部分** | **描述** |
| *appname* | 必需的；**Variant** (**String**)。提供该对象的应用程序名称。 |
| *objecttype* | 必需的；**Variant** (**String**)。待创建对象的类型或类。 |

  

**说明**

使用 **GetObject** 函数可以访问文件中的 ActiveX 对象，而且可以将该对象赋给对象变量。可以使用 **Set** 语句将 **GetObject** 返回的对象赋给对象变量。例如：

    Dim CADObject As Object
    Set CADObject = GetObject("C:\CAD\SCHEMA.CAD")

当执行上述代码时，就会启动与指定的 ***pathname*** 相关联的应用程序，同时激活指定文件中的对象。

如果 ***pathname*** 是一个零长度的字符串 ("")，则 **GetObject** 返回指定类型的新的对象实例。如果省略了 ***pathname*** 参数，则 **GetObject** 返回指定类型的当前活动的对象。如果当前没有指定类型的对象，就会出错。

有些应用程序允许只激活文件的一部分，其方法是在文件名后加上一个惊叹号 (**!**) 以及用于标识想要激活的文件部分的字符串。关于如何创建这种字符串的信息，请参阅有关应用程序创建对象的文档。

例如，在绘图应用程序中，一个存放在文件中的图可能有多层。可以使用下述代码来激活图中被称为 `SCHEMA.CAD `的层：

    Set LayerObject = GetObject("C:\CAD\SCHEMA.CAD!Layer3")

如果不指定对象的 ***class***，则自动化会根据所提供的文件名，来确定被启动的应用程序以及被激活的对象。不过，有些文件可能不止支持一种对象类。例如，图片可能支持三种不同类型的对象：**Application** 对象，**Drawing** 对象，以及 **Toolbar** 对象，所有这些都是同一个文件中的一部分。为了说明要具体激活文件中的哪种对象，就应使用这个可选的 ***class*** 参数。例如：

    Dim MyObject As Object
    Set MyObject = GetObject("C:\DRAWINGS\SAMPLE.DRW", "FIGMENT.DRAWING")

在上述例子中，`FIGMENT` 是一个绘图应用程序的名称，而 `DRAWING` 则是它支持的一种对象类型。

对象被激活之后，就可以在代码中使用所定义的对象变量来引用它。在前面的例子中，可以使用对象变量 `MyObject` 来访问这个新对象的属性和方法。例如：

    MyObject.Line 9, 90
    MyObject.InsertText 9, 100, "Hello, world."
    MyObject.SaveAs "C:\DRAWINGS\SAMPLE.DRW"

**注意** 当对象当前已有实例，或要创建已加载的文件的对象时，就使用 **GetObject** 函数。如果对象当前还没有实例，或不想启动已加载文件的对象，则应使用 **CreateObject** 函数。

如果对象已注册为单个实例的对象，则不管执行多少次 **CreateObject**，都只能创建该对象的一个实例。若使用单个实例对象，当使用零长度字符串 ("") 语法调用时，**GetObject** 总是返回同一个实例，而若省略 ***pathname*** 参数，就会出错。不能使用 **GetObject** 来获取 Visual Basic 创建的类的引用。
