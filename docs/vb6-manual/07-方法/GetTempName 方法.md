# GetTempName 方法

# GetTempName 方法

           

**描述**

返回一个随机产生的临时文件或文件夹的名字，该名字在执行需要临时文件或文件夹的操作时有用。

**语法**

*object*.**GetTempName**

可选的 *object* 始终是一个 **FileSystemObject**. 的名字。

**说明**

**GetTempName** 方法不产生一个文件，它仅提供一个临时文件名字，该名字可被 **CreateTextFile** 用于创建一个文件。
