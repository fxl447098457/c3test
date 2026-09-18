# MAPISession 控件

# MAPISession 控件

               

消息应用程序接口 (MAPI) 控件可以创建具有邮件功能的 Visual Basic MAPI 应用程序。有 2 个 MAPI 控件：

- MAPISession  
    
- MAPIMessages

**MAPISession** 控件登录并且建立一个 MAPI 会话。它也用于结束一个 MAPI 会话并退出。**MAPIMessages** 控件使用户可以可以执行多种消息系统功能。

**语法**

MAPISession

**说明**

登录成功后，**SessionID** 属性包含访问 MAPI 会话的句柄。当使用 **MAPIMessages** 控件时，会话句柄必须传给 **MAPIMessages** 控件，否则将引发错误。

**MAPISession** 控件在运行时是不可见的。而且，该控件不产生事件。为使用它，必须指定适当的属性和方法。

为使这些控件正常工作。必须有 MAPI 服务。MAPI 提供的 MAPI 服务是符合电子邮件系统规范的。

**注意** 如果试图运行一个使用 MAPI 控件的程序，必须先保证已正确安装了 32 位 MAPI DDLs，否则将不能完成象SignOn这样简单的 MAPI 功能。例如，为了正确使用 MAPI 功能或者来自 visual basic 的 MAPI 自定义控件，在 Windows 95 上必须在操作系统的安装过程中安装 Exchange，或者从控制面板上单独地安装 MAIL。
