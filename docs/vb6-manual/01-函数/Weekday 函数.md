# Weekday 函数

返回一个 **Variant** (**Integer**)，包含一个整数，代表某个日期是星期几。

**语法**

**Weekday(***date,* \[***firstdayofweek***\]**)**

**Weekday** 函数语法有下列的命名参数：

|  |  |
|----|----|
| **部分** | **描述** |
| ***date*** | 必要。能够表示日期的 Variant、数值表达式、字符串表达式或它们的组合。如果 ***date*** 包含 Null，则返回 **Null**。 |
| ***Firstdayofweek*** | 可选。指定一星期第一天的常数。如果未予指定，则以 **vbSunday** 为缺省值。 |

  

**设置**

***firstdayofweek*** 参数有以下设定值：

|                 |        |                     |
|-----------------|--------|---------------------|
| **常数**        | **值** | **描述**            |
| **vbUseSystem** | 0      | 使用 NLS API 设置。 |
| **VbSunday**    | 1      | 星期日（缺省值）    |
| **vbMonday**    | 2      | 星期一              |
| **vbTuesday**   | 3      | 星期二              |
| **vbWednesday** | 4      | 星期三              |
| **vbThursday**  | 5      | 星期四              |
| **vbFriday**    | 6      | 星期五              |
| **vbSaturday**  | 7      | 星期六              |

  

**返回值**

**Weekday** 函数可以返回以下诸值：

|                 |        |          |
|-----------------|--------|----------|
| **常数**        | **值** | **描述** |
| **vbSunday**    | 1      | 星期日   |
| **vbMonday**    | 2      | 星期一   |
| **vbTuesday**   | 3      | 星期二   |
| **vbWednesday** | 4      | 星期三   |
| **vbThursday**  | 5      | 星期四   |
| **vbFriday**    | 6      | 星期五   |
| **vbSaturday**  | 7      | 星期六   |
