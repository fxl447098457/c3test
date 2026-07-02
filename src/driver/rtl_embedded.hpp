// P10/P11.3/DualArch: RTL runtime embedded resource management
// Extract RTL .h headers + pre-compiled .lib from c3.exe RCDATA resources
// P11.3: .c source replaced by .lib static libraries (source protection)
// DualArch: x64 (IDs 110-112) and x86 (IDs 120-122) .lib variants
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
    RTL_VB6COM_H        = 102,
    RTL_VB6COMSERVER_H  = 104,
    RTL_VB6FORMS_H      = 106,
    // Pre-compiled static libraries — x64 (IDs 110-112)
    RTL_VB6RTL_LIB      = 110,  // vb6rtl + vb6com (all programs)
    RTL_VB6RTL_DLL_LIB  = 111,  // vb6comserver (ActiveX DLL only)
    RTL_VB6RTL_GUI_LIB  = 112,  // vb6forms (GUI programs only)
    // Pre-compiled static libraries — x86 (IDs 120-122)
    RTL_VB6RTL_LIB_X86      = 120,  // vb6rtl + vb6com (all programs)
    RTL_VB6RTL_DLL_LIB_X86  = 121,  // vb6comserver (ActiveX DLL only)
    RTL_VB6RTL_GUI_LIB_X86  = 122,  // vb6forms (GUI programs only)
};

// Session directory manager
// Creates %TMP%\C3C\{timestamp}\ temp dir, extracts RTL files, cleans up after compile
class SessionManager {
public:
    SessionManager();
    ~SessionManager();

    // Create new session directory and extract RTL files
    // arch: "x64" (default) or "x86" — selects which .lib variant to extract
    // Returns: RTL directory path (contains .h + .lib)
    // Empty string on failure
    std::string create(const std::string& arch = "x64");

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
