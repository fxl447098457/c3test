## C3 v{VERSION} 发布说明

C3 是第三代 VB6 编译器, 将 VB6 代码编译为原生 x64 Windows 二进制。

### 安装

1. 下载下方附件 `{ZIPNAME}` 并解压到任意目录
2. 右键 `install_msvc.bat` -> 以管理员身份运行
   (发布包自带迷你 MSVC 工具链, 离线模式数秒完成, 无需下载 VS Build Tools)
3. 右键任意 VB6 文件 -> "使用 C3 编译"; 或新开命令行执行 `c3 demo.bas`

详细说明见发布包内 `安装说明.md`。

### 包含内容

- `C3.exe` - C3 编译器主程序
- `msvc/` - 内置迷你 MSVC 工具链 + Windows SDK (离线模式)
- `menu/` - 右键菜单 "使用 C3 编译" 安装 / 卸载注册表
- `demos/` - 示例工程

- 官网: https://c3.vb6.pro/
