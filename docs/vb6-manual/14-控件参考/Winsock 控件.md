# Winsock 控件

# Winsock 控件

               

**Winsock** 控件对用户来说是不可见的，它提供了访问 TCP 和 UDP 网络服务的方便途径。Microsoft Access、Visual Basic、Visual C++ 或 Visual FoxPro 的开发人员都可使用它。为编写客户或服务器应用程序，不必了解 TCP 的细节或调用低级的 Winsock APIs。通过设置控件的属性并调用其方法就可轻易连接到一台远程机器上去，并且还可双向交换数据。

**TCP 基础**

数据传输协议允许创建和维护与远程计算机的连接。连接两台计算机就可彼此进行数据传输。

如果创建客户应用程序，就必须知道服务器计算机名或者 IP 地址（**RemoteHost** 属性），还要知道进行“侦听”的端口（**RemotePort** 属性），然后调用 **Connect** 方法。

如果创建服务器应用程序，就应设置一个收听端口（**LocalPort** 属性）并调用 **Listen** 方法。当客户计算机需要连接时就会发生 ConnectionRequest 事件。为了完成连接，可调用 ConnectionRequest 事件内的 **Accept** 方法。

建立连接后，任何一方计算机都可以收发数据。为了发送数据，可调用 **SendData** 方法。当接收数据时会发生 DataArrival 事件。调用 DataArrival 事件内的 **GetData** 方法就可获取数据。

**UDP 基础**

用户数据文报协议 (UDP) 是一个无连接协议。跟 TCP 的操作不同，计算机并不建立连接。另外 UDP 应用程序可以是客户机，也可以是服务器。

为了传输数据，首先要设置客户计算机的 **LocalPort** 属性。然后，服务器计算机只需将 **RemoteHost** 设置为客户计算机的 Internet 地址，并将 **RemotePort** 属性设置为跟客户计算机的 **LocalPort** 属性相同的端口，并调用 **SendData** 方法来着手发送信息。于是，客户计算机使用 DataArrival 事件内的 **GetData** 方法来获取已发送的信息。
