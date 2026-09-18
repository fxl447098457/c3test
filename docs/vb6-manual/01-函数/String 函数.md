# String 函数

返回 **Variant** (**String**)，其中包含指定长度重复字符的字符串。

**语法**

**String**(***number***, ***character***)

**String** 函数的语法有下面的命名参数：

|  |  |
|----|----|
| **部分** | **说明** |
| ***number*** | 必要参数；Long。返回的字符串长度。如果 ***number*** 包含 Null，将返回 **Null**。 |
| ***character*** | 必要参数；Variant。为指定字符的字符码或字符串表达式，其第一个字符将用于建立返回的字符串。如果 ***character*** 包含 **Null**，就会返回 **Null**。 |

  

**说明**

如果指定 ***character*** 的数值大于 255，**String** 会按下面的公式将其转为有效的字符码：

***character*** **Mod** 256
