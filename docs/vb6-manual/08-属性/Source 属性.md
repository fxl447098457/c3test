# Source 属性

返回或设置一个字符串表达式，指明最初生成错误的对象或应用程序的名称。可读 / 可写。

**说明**

**Source** 属性是字符串表达式，指定生成错误的对象；此表达式通常是这个对象的类名或程序设计的 ID。在程序代码无法处理被访问对象产生的错误时，请使用 **Source** 提供消息。例如，如果访问 Microsoft Excel 时生成了一个“除以零”的错误，则 Microsoft Excel 将 **Err.Number** 设置成代表此错误的错误代码，并将 **Source** 设置成 Excel.Application。

在错误生成时，**Source** 就是应用程序的程序设计 ID。对于类模块，**Source** 应该包含一个具有 *project.class* 窗体的名称。当代码中出现不可预料的错误时，**Source** 属性会自动填上数据。对于标准模块中的错误，**Source** 含有工程名称。对于类模块中的错误，**Source** 包含具有 *project.class* 窗体的名称。
