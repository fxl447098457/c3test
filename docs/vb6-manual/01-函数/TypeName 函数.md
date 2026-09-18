# TypeName 函数

返回一个 **String，**提供有关变量的信息。

**语法**

**TypeName(***varname***)**

必要的 *varname* 参数是一个 Variant，它包含用户定义类型变量之外的任何变量。

**说明**

**TypeName** 所返回的字符串可以是下面列举的任何一个字符串**：**

|  |  |
|----|----|
| **返回字符串** | **变量** |
| 对象类型 | 类型为 *objecttype* 的对象 |
| Byte | 位值 |
| Integer | 整数 |
| Long | 长整数 |
| Single | 单精度浮点数 |
| Double | 双精度浮点数 |
| Currency | 货币 |
| Decimal | 十进制值 |
| Date | 日期 |
| String | 字符串 |
| 布尔 | 布尔值 |
| **Error** | 错误值 |
| Empty | 未初始化 |
| Null | 无效数据 |
| Object | 对象 |
| Unknown | 类型未知的对象 |
| **Nothing** | 不再引用对象的对象变量 |

  

如果 *varname* 是一个数组，则返回的字符串可以是任何一个后面添加了空括号的可能的返回字符串（或 **Variant**）。例如，如果 *varname* 是一个整数数组，则 **TypeName** 返回 "`Integer()`"。
