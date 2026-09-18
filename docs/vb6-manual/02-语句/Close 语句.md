# Close 语句

# Close 语句

       

关闭 **Open** 语句所打开的输入/输出 (I/O) 文件。

**语法**

**Close** \[*filenumberlist*\]

可选的 *filenumberlist* 参数为一个或多个文件号，其中 *filenumber* 为任何有效的文件号，语法如下：

\[\[**\#**\]*filenumber*\] \[**,** \[**\#**\]*filenumber*\] **. . .**

**说明**

若省略 *filenumberlist*，则将关闭 **Open** 语句打开的所有活动文件。

当关闭 **Output** 或 **Append** 打开的文件时，将属于此文件的最终输出缓冲区写入操作系统缓冲区。所有与该文件相关联的缓冲区空间都被释放。

在执行 **Close** 语句时，文件与其文件号之间的关联将终结。
