# WeekdayName函数

# WeekdayName函数

       

**描述**

返回一个字符串，表示一星期中的某天。

**语法**

**WeekdayName(***weekday***,** *abbreviate***,** *firstdayofweek***)**

**WeekdayName**函数语法有如下几部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *weekday* | 必需的。数字值，表示一星期中的某天。该数字值要依赖于*firstdayofweek*设置中的设置值来决定。 |
| *abbreviate* | 可选的。**Boolean**值，表示星期的名称是否被缩写。如果忽略该值，缺省值为**False**，表明星期的名称不能被缩写。 |
| *firstdayofweek* | 可选的。数字值，表示一星期中第一天。关于其值，请参阅“设置值”部分。 |

  

**设置值**

*firstdayofweek*参数值如下：

|                 |        |                                    |
|-----------------|--------|------------------------------------|
| **常数**        | **值** | **描述**                           |
| **vbUseSystem** | 0      | 使用本国语言支持 (NLS) API设置值。 |
| **vbSunday**    | 1      | 星期日（缺省）。                   |
| **vbMonday**    | 2      | 星期一                             |
| **vbTuesday**   | 3      | 星期二                             |
| **vbWednesday** | 4      | 星期三                             |
| **vbThursday**  | 5      | 星期四                             |
| **vbFriday**    | 6      | 星期五                             |
| **vbSaturday**  | 7      | 星期六                             |
