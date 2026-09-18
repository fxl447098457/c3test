# Deftype 语句

**Deftype 语句**

       

在模块级别上，为变量和传给过程的参数，设置缺省数据类型，以及为其名称以指定的字符开头的 **Function** 和 **Property** **Get** 过程，设置返回值类型。

**语法**

**DefBool** *letterrange*\[**,** *letterrange*\] **. . .**

**DefByte** *letterrange*\[**,** *letterrange*\] **. . .**

**DefInt** *letterrange*\[**,** *letterrange*\] **. . .**

**DefLng** *letterrange*\[**,** *letterrange*\] **. . .**

**DefCur** *letterrange*\[**,** *letterrange*\] **. . .**

**DefSng** *letterrange*\[**,** *letterrange*\] **. . .**

**DefDbl** *letterrange*\[**,** *letterrange*\] **. . .**

**DefDec** *letterrange*\[**,** *letterrange*\] **. . .**

**DefDate** *letterrange*\[**,** *letterrange*\] **. . .**

**DefStr** *letterrange*\[**,** *letterrange*\] **. . .**

**DefObj** *letterrange*\[**,** *letterrange*\] **. . .**

**DefVar** *letterrange*\[**,** *letterrange*\] **. . .**

所需的 *letterrange* 参数遵循下述语法：

*letter1*\[**-***letter2*\]

*letter1* 和 *letter2* 参数指定设置缺省数据类型的名称范围。每个参数都是指变量，参数和 **Function** 过程，或 **Property Get** 过程名称的首字母，且参数可以是字母表中的任意字母。*letterrange* 中不区分字母的大小写。

**说明**

语句的名字就确定相应的数据类型：

|             |                                                        |
|-------------|--------------------------------------------------------|
| **语句**    | **数据类型**                                           |
| **DefBool** | 布尔                     |
| **DefByte** | Byte                    |
| **DefInt**  | Integer                 |
| **DefLng**  | Long                    |
| **DefCur**  | Currency                |
| **DefSng**  | Single                  |
| **DefDbl**  | Double                  |
| **DefDec**  | Decimal（目前尚不支持） |
| **DefDate** | Date                    |
| **DefStr**  | String                  |
| **DefObj**  | Object                  |
| **DefVar**  | Variant                 |

  

例如，在下面的程序段中，`Message` 就是一个字符串变量：

    DefStr A-Q
    . . .
    Message = "Out of stack space."

**Def***type* 语句只在使用该语句的模块中有效。例如，一个模块中的 **DefInt** 语句只对在该模块中声明的变量和传给过程的参数的缺省数据类型，以及 **Function** 和 **Property** **Get** 过程的返回值的类型有效；而其它模块中的变量、参数、以及返回值的缺省数据类型就不受影响。如果不用 **Def***type* 语句显式地声明，则所有变量、参数、**Function** 过程、以及 **Property** **Get** 过程的缺省数据类型都是 **Variant**。

当指定字符范围时，通常为以字符集的前 128 个字符中的字符开始的变量定义数据类型。不过，如果指定的字符范围是A–Z，则将所有的变量，包括以字符集的扩展部分(128–255) 中的国际字符开始的变量的缺省类型都设为指定的类型。

在指定了A–Z 范围之后，就不能再使用 **Def***type* 语句来重新定义任何子范围的变量了。在指定一个范围后，如果另一个 **Def***type* 语句定义的范围中含有前面已定义的字符，就会产生错误。不过，不管变量是否已定义，都可以使用带 **As** *type* 子句的 **Dim** 语句来显式指定其数据类型。例如，可以在模块级使用如下代码将一个缺省数据类型为 **Integer** 的变量定义为 **Double**：

    DefInt A-Z
    Dim TaxRate As Double

**Def***type* 语句对用户定义类型中的元素无影响，因为这些元素必须显式声明。
