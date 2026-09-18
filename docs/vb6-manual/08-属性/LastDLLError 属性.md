# LastDLLError 属性

返回因调用动态链接库 (DLL) 而产生的系统错误号。只读。

**说明**

**LastDLLError** 属性只适用于由 Visual Basic 代码进行的 DLL 调用。在调用时，被调用的函数通常返回一个表明成功还是失败的代码，同时对 **LastDLLError** 属性填充数据。请检查 DLL 函数的文档，确定返回值，表明是成功还是失败。只要返回失败代码，Visual Basic 的应用程序就应立即检查 **LastDLLError** 属性。在设置 **LastDLLError** 属性时不会有任何例外。
