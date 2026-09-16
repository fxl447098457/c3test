// P10 (restored): RTL runtime embedded resource management
// Extract RTL .h headers + .c sources from c3.exe RCDATA resources.
// The P11.3 pre-compiled .lib scheme was reverted (c3 is open source);
// RTL sources are compiled by MSVC together with the generated code.
// Compiles and cleans up temp session dir automatically

#ifndef VB6C3_RTL_EMBEDDED_HPP
#define VB6C3_RTL_EMBEDDED_HPP

#include <string>
#include <vector>

namespace vb6c3 {

// RTL embedded resource IDs (must match c3rtl.rc)
enum RtlResourceID {
    // Headers (for #include in generated code) — arch-neutral
    RTL_VB6RTL_H        = 100,
    RTL_VB6RTL_C        = 101,
    RTL_VB6COM_H        = 102,
    RTL_VB6COM_C        = 103,
    RTL_VB6COMSERVER_H  = 104,
    RTL_VB6COMSERVER_C  = 105,
    RTL_VB6FORMS_H      = 106,
    RTL_VB6FORMS_C      = 107,
    RTL_VB6_DI_STUBS_C  = 108,
    RTL_VB6_DI_WIN32_STUBS_C = 109,

    // vb6comserver 按 COM 接口家族拆分 (P6.6): 内部共享头 + 各族实现
    RTL_VB6COMSERVER_INTERNAL_H = 110,
    RTL_VB6COMSERVER_OBJ_C      = 111,
    RTL_VB6COMSERVER_FACTORY_C  = 112,
    RTL_VB6COMSERVER_CP_C       = 113,
    RTL_VB6COMSERVER_PCI_C      = 114,

    // vb6com 按 COM 调用层次拆分 (P6): 内部共享头 + 各层实现
    RTL_VB6COM_INTERNAL_H = 115,
    RTL_VB6COM_INVOKE_C   = 116,
    RTL_VB6COM_PACK_C     = 117,
    RTL_VB6COM_WRAP_C     = 118,
    RTL_VB6COM_SINK_C     = 119,
    RTL_VB6COM_FOREACH_C  = 120,

    // vb6forms 按控件/窗体功能家族拆分 (P7): 内部共享头 + 各家族实现
    RTL_VB6FORMS_INTERNAL_H = 121,
    RTL_VB6FORMS_CTRL_C     = 122,
    RTL_VB6FORMS_LIST_C     = 123,
    RTL_VB6FORMS_STYLE_C    = 124,
    RTL_VB6FORMS_SCROLL_C   = 125,
    RTL_VB6FORMS_PICTURE_C  = 126,
    RTL_VB6FORMS_CTRLARR_C  = 127,
    RTL_VB6FORMS_WEBVIEW_C  = 128,
    RTL_VB6FORMS_WIDGET_C   = 129,
    RTL_VB6FORMS_SHAPE_C    = 130,
    RTL_VB6FORMS_AXSITE_C   = 131,
};

// Session directory manager
// Creates %TMP%\C3C\{timestamp}\ temp dir, extracts RTL files, cleans up after compile
class SessionManager {
public:
    SessionManager();
    ~SessionManager();

    // Create new session directory and extract RTL files (.h + .c, arch-neutral)
    // Returns: RTL directory path (contains .h + .c)
    // Empty string on failure
    std::string create();

    // Get current session's RTL directory path
    const std::string& rtlDir() const { return rtlDir_; }

    // P11.2: session root dir (for intermediates .c/.h/.obj)
    const std::string& sessionDir() const { return sessionDir_; }

    // Clean up session directory (auto-called by destructor)
    void cleanup();

    // Release session without cleanup (keep intermediates for debugging)
    void release() { sessionDir_.clear(); rtlDir_.clear(); }

    // Clean up old session dirs (>300 seconds)
    // Auto-called on each create()
    static void cleanupOldSessions();

    // Check if session is active
    bool isActive() const { return !rtlDir_.empty(); }

private:
    std::string sessionDir_;  // session root dir (e.g. %TMP%\C3C\{ts})
    std::string rtlDir_;      // rtl subdir (sessionDir_\rtl)

    // Extract file from RCDATA resource
    bool extractResource(int resId, const std::string& fileName, const std::string& targetDir);

    // Get session root directory (%TMP%\C3C)
    static std::string getSessionRoot();
};

} // namespace vb6c3

#endif // VB6C3_RTL_EMBEDDED_HPP
