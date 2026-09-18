# HelpContext 属性

返回或设置一个字符串表达式，包含 Microsoft Windows 帮助文件中的主题的上下文 ID。可读 / 可写。

**说明**

**HelpContext** 属性被用来自动显示 **HelpFile** 属性中指定的帮助主题。如果 **HelpFile** 和 **HelpContext** 都是空的，则检查 **Number** 的值。如果 **Number** 的值与 Visual Basic 运行时错误一致，则对此错误使用 Visual Basic 帮助上下文 ID。如果 **Number** 的值与 Visual Basic 错误不一致，则在屏幕上显示 Visual Basic 帮助文件的内容。

**注意** 应该在应用程序中写入一些例程来处理常见错误。当使用对象编程时，可以用该对象的帮助文件来提高处理错误的质量，而如果错误无法补救，则要为用户显示一段有意义的消息。
