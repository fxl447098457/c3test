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
    RTL_VB6FORMS_UC_C       = 175,  // Fix 112: 工程内 UserControl 实例宿主

    // vb6rtl.h 按家族拆分 (2026-09-17): 伞头 + 7 个子头
    RTL_VB6RTL_BASE_H = 132,
    RTL_VB6RTL_BSTR_H = 133,
    RTL_VB6RTL_VARIANT_H = 134,
    RTL_VB6RTL_BUILTIN_H = 135,
    RTL_VB6RTL_ARRAY_H = 136,
    RTL_VB6RTL_CLASS_COM_H = 137,
    RTL_VB6RTL_RUNTIME_H = 138,

    // vb6forms 家族再细分 (2026-09-17)
    RTL_VB6FORMS_PICTURE_PROP_C = 139,
    RTL_VB6FORMS_WIDGET_PROP_C  = 140,

    // DI 转发桩按 Lib 家族拆分 (2026-09-17)
    RTL_VB6_DI_USER32_STUBS_C    = 141,
    RTL_VB6_DI_GDIPLUS_STUBS_C   = 142,
    RTL_VB6_DI_CRYPTO_STUBS_C    = 143,
    RTL_VB6_DI_COM_STUBS_C       = 144,
    RTL_VB6_DI_NET_STUBS_C       = 145,
    RTL_VB6_DI_SHELL_STUBS_C     = 146,

    // vb6forms.h 按功能域拆分 (2026-09-17): 伞头 + 9 个子头
    RTL_VB6FORMS_WINDOW_H      = 147,
    RTL_VB6FORMS_PROP_H        = 148,
    RTL_VB6FORMS_PROP_CTRL_H   = 149,
    RTL_VB6FORMS_PROP_PIC_H    = 150,
    RTL_VB6FORMS_PROP_FORM_H   = 151,
    RTL_VB6FORMS_CTRLARR_H     = 152,
    RTL_VB6FORMS_MDI_H         = 153,
    RTL_VB6FORMS_WEBVIEW_H     = 154,
    RTL_VB6FORMS_CONTROLS_H    = 155,

    // vb6rtl.c 按家族拆分 (2026-09-17): 主文件 + 13 个族实现
    RTL_VB6RTL_STRING_C        = 156,
    RTL_VB6RTL_FORMAT_C        = 157,
    // format 族按函数体片段拆: 6 个 .inc（不单独编译, 由 format.c include）
    RTL_VB6RTL_FORMAT_EXTRACT_INC      = 158,
    RTL_VB6RTL_FORMAT_PARSE_INC        = 159,
    RTL_VB6RTL_FORMAT_NUMERIC_PRE_INC  = 160,
    RTL_VB6RTL_FORMAT_NUMERIC_BODY_INC = 161,
    RTL_VB6RTL_FORMAT_NUMERIC_TAIL_INC = 162,
    RTL_VB6RTL_FORMAT_STRING_INC       = 163,
    RTL_VB6RTL_CONV_C          = 164,
    RTL_VB6RTL_MISC_C          = 165,
    RTL_VB6RTL_SYSTEM_C        = 166,
    RTL_VB6RTL_COMPAT_C        = 167,
    RTL_VB6RTL_DATE_C          = 168,
    RTL_VB6RTL_ARRAY_C         = 169,
    RTL_VB6RTL_FILE_C          = 170,
    RTL_VB6RTL_PARAMARRAY_C    = 171,
    RTL_VB6RTL_FINANCIAL_C     = 172,
    RTL_VB6RTL_COM_C           = 173,
    RTL_VB6RTL_REGISTRY_C      = 174,

    RTL_VB6RTL_USERCTL_H       = 176,  // Fix 105: UserControl/Ambient/Extender/PropertyPage 宿主

    // Fix 103: VB6 内建对象 Collection 的 C 实现 (IDispatch + IEnumVARIANT)
    RTL_VB6COM_COLLECTION_C    = 177,
    RTL_VB6COM_COLLECTION_ENUM_C = 178,

    // vb6forms_uc 按族细分 (2026-09-19): 内部头 + 6 族编译单元 + 7 个函数体片段
    RTL_VB6FORMS_UC_INTERNAL_H             = 179,
    RTL_UC_HOST_C                          = 180,
    RTL_UC_HOST_WINDOW_C                   = 181,
    RTL_UC_HOSTMODEL_C                     = 182,
    RTL_UC_CONTROLS_C                      = 183,
    RTL_UC_COLLECTION_C                    = 184,
    RTL_UC_DEBUG_C                         = 185,
    RTL_UC_HOST_CREATE_INC                 = 186,
    RTL_UC_HOSTMODEL_GETPROP_INC           = 187,
    RTL_UC_HOSTMODEL_SETPROP_INC           = 188,
    RTL_UC_HOSTMODEL_CALL_INC              = 189,
    RTL_UC_COLLECTION_API_INC              = 190,
    RTL_UC_DEBUG_DIB_INC                   = 191,
    RTL_UC_DEBUG_COMPOSITE_INC             = 192,
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
