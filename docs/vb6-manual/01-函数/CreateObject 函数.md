# CreateObject 函数

**CreateObject 函数**

       

创建并返回一个对 ActiveX 对象的引用。

**语法**

**CreateObject(***class,\[servername\]***)**

 **CreateObject** 函数的语法有如下部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *class* | 必需的； **Variant** (**String**). 要创建的应用程序名称和类。 |
| *servername* | 可选的； **Variant** (**String**). 要在其上创建对象的网络服务器名称。 |

  

*class* 参数使用 *appname***.***objecttype* 这种语法，包括以下部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *appname* | 必需的；**Variant**（**字符串**）。提供该对象的应用程序名。 |
| *objecttype* | 必需的；**Variant**（**字符串**）。待创建对象的类型或类。 |

  

**说明**

每个支持自动化的应用程序都至少提供一种对象类型。例如，一个字处理应用程序可能会提供 **Application** 对象，**Document** 对象，以及 **Toolbar** 对象。

要创建 ActiveX 对象，只需将 **CreateObject** 返回的对象赋给一个对象变量：

    '声明一个对象变量来存放该对象
    '的引用。Dim as Object 采用后期绑定方式。
    Dim ExcelSheet As Object
    Set ExcelSheet = CreateObject("Excel.Sheet")

上述代码将启动该应用程序创建该对象，在本例中就是创建一个 Microsoft Excel 电子数据表。对象创建后，就可以在代码中使用自定义的对象变量来引用该对象。在下面的示例中，可以使用对象变量 `ExcelSheet` 来访问新建对象的属性和方法，以及访问 Microsoft Excel 的其它对象，包括应用程序对象和单元格集合。

    '设置 Application 对象使 Excel 可见
    ExcelSheet.Application.Visible = True
    '在表格的第一个单元中写些文本
    ExcelSheet.Cells(1, 1).Value = "This is column A, row 1"
    '将该表格保存到 C:\test.doc 目录
    ExcelSheet.SaveAs "C:\ TEST.DOC"
    '使用应用程序对象的 Quit 方法关闭 Excel。
    ExcelSheet.Application.Quit
    '释放该对象变量
    Set ExcelSheet = Nothing

使用 `As Object` 子句声明对象变量，可以创建一个能包含任何类型对象引用的变量。不过，该变量访问对象是后期绑定的，也就是说，绑定在程序运行时才进行。要创建一个使用前期绑定方式的对象变量，也就是说，在程序编译时就完成绑定，则对象变量在声明时应指定类 ID。例如，可以声明并创建下列 Microsoft Excel 引用：

`Dim xlApp As Excel.Application `

`Dim xlBook As Excel.Workbook`

`Dim xlSheet As Excel.WorkSheet`

`Set xlApp = CreateObject("Excel.Application")`

`Set xlBook = xlApp.Workbooks.Add`

`Set xlSheet = xlBook.Worksheets(1)`

前期绑定的变量引用可以提供更好的性能，但该变量只能存放声明中所指定的类的引用。

可以将 **CreateObject** 函数返回的对象传给一个参数为对象的函数。例如，下面的代码创建并传递了一个 Excel.Application 对象的引用：

    Call MySub (CreateObject("Excel.Application"))

可以在一个远端连网的计算机上创建一个对象，方法是把计算机的名称传递给 **CreateObject** 的 *servername* 参数。这个名称与共享名称的机器名部份相同：对于一个共享名称为 "\\\\MyServer\\Public," 的 *servername* 参数是 "MyServer" 。

下面的代码返回在一个名为 `MyServer `的远端计算机上运行的 Excel 实例的版本号：

    Dim xlApp As Object
    Set xlApp = CreateObject("Excel.Application", "MyServer")
    Debug.Print xlApp.Version

如果远端服务器不存在或者不可用，则会发生一个运行时错误。

**注意** 当该对象当前没有实例时，应使用 **CreateObject**。如果该对象已有实例在运行，就会启动一个新的实例，并创建一个指定类型的对象。要使用当前实例，或要启动该应用程序并加载一个文件，可以使用 **GetObject** 函数。

如果对象已登记为单个实例对象，则不管执行多少次 **CreateObject**，都只能创建该对象的一个实例。
