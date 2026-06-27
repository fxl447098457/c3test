// P10: RTL 运行时内嵌资源管理
// 从 c3.exe 内嵌的 RCDATA 资源释放 RTL .h/.c 文件到临时会话目录
// 编译完成后自动清除临时目录

#ifndef VB6C3_RTL_EMBEDDED_HPP
#define VB6C3_RTL_EMBEDDED_HPP

#include <string>
#include <vector>

namespace vb6c3 {

// RTL 嵌入资源 ID 定义 (与 c3rtl.rc 中的编号对应)
enum RtlResourceID {
    RTL_VB6RTL_H      = 100,
    RTL_VB6RTL_C      = 101,
    RTL_VB6COM_H      = 102,
    RTL_VB6COM_C      = 103,
    RTL_VB6COMSERVER_H = 104,
    RTL_VB6COMSERVER_C = 105,
    RTL_VB6FORMS_H    = 106,
    RTL_VB6FORMS_C    = 107,
};

// 会话目录管理器
// 创建 %TMP%\C3C\{timestamp}\ 临时目录, 释放 RTL 文件, 编译完成后清除
class SessionManager {
public:
    SessionManager();
    ~SessionManager();

    // 创建新的会话目录并释放 RTL 文件
    // 返回: RTL 目录路径 (含 .h/.c 文件)
    // 如果释放失败返回空字符串
    std::string create();

    // 获取当前会话的 RTL 目录路径
    const std::string& rtlDir() const { return rtlDir_; }

    // 清除会话目录 (析构时自动调用)
    void cleanup();

    // 清理旧的会话目录 (>300 秒)
    // 每次创建会话时自动调用
    static void cleanupOldSessions();

    // 检查是否已创建会话
    bool isActive() const { return !rtlDir_.empty(); }

private:
    std::string sessionDir_;  // session root dir (e.g. %TMP%\C3C\{ts})
    std::string rtlDir_;      // rtl subdir (sessionDir_\rtl)

    // 从 RCDATA 资源释放文件
    // resId: 资源 ID
    // fileName: 目标文件名 (如 "vb6rtl.h")
    // targetDir: 目标目录
    bool extractResource(int resId, const std::string& fileName, const std::string& targetDir);

    // 获取会话根目录 (%TMP%\C3C)
    static std::string getSessionRoot();
};

} // namespace vb6c3

#endif // VB6C3_RTL_EMBEDDED_HPP
