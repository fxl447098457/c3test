# GetAllSettings 函数

从 Windows 注册表中返回应用程序项目的所有注册表项设置及其相应值（开始是由 **SaveSetting** 产生）。

**语法**

**GetAllSettings(*appname,*** ***section*)**

**GetAllSettings** 函数的语法具有下列命名参数：

|          |          |
|----------|----------|
| **部分** | **描述** |

  

|  |  |
|----|----|
| ***appname*** | 必要。字符串表达式，包含应用程序或工程的名称，并要求这些应用程序或工程有注册表项设置 |

  

|  |  |
|----|----|
| ***section*** | 必要。字符串表达式，包含区域名称，并要求该区域有注册表项设置。**GetAllSettings** 返回 Variant，其内容为字符串的二维数组，该二维数组包含指定区域中的所有注册表项设置及其对应值。 |

  

**说明**

如果 ***appname*** 或 ***section*** 不存在，则 **GetAllSettings** 返回未初始化的 **Variant**。
