# Number 属性

返回或设置表示错误的数值。**Number** 是 **Err** 对象的缺省属性。可读/可写。

**说明**

从对象返回用户自定义的错误时，把被选作错误代码的数与 **vbObjectError** 常数相加，并由此设置 **Err.Number**。例如，用下列代码返回作为错误代码的数字 1051：

    Err.Raise Number := vbObjectError + 1051, Source:= SomeClass
