// P10/P11.3/DualArch: RTL runtime embedded resource management - implementation
// Extract 4 .h headers + 3 .lib static libraries from c3.exe RCDATA resources
// P11.3: .c source replaced by pre-compiled .lib (source protection)
// DualArch: x64 (IDs 110-112) or x86 (IDs 120-122) based on target arch

#include "driver/rtl_embedded.hpp"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <cstdlib>
#include <vector>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace vb6c3 {

// ============================================================
// Helper: load RCDATA resource from current EXE
// ============================================================
static bool loadRtlResource(int resId, std::vector<char>& outData) {
#ifdef _WIN32
    HRSRC hrsrc = FindResourceW(nullptr, MAKEINTRESOURCEW(resId), RT_RCDATA);
    if (!hrsrc) {
        // Fallback: try loading from current module (when loaded as DLL)
        HMODULE hMod = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                           reinterpret_cast<LPCWSTR>(&loadRtlResource), &hMod);
        if (hMod) {
            hrsrc = FindResourceW(hMod, MAKEINTRESOURCEW(resId), RT_RCDATA);
        }
    }
    if (!hrsrc) return false;

    HGLOBAL hGlobal = LoadResource(nullptr, hrsrc);
    if (!hGlobal) return false;

    DWORD size = SizeofResource(nullptr, hrsrc);
    const void* ptr = LockResource(hGlobal);
    if (!ptr || size == 0) return false;

    outData.assign(static_cast<const char*>(ptr), static_cast<const char*>(ptr) + size);
    return true;
#else
    // Non-Windows: resources not available, fallback to file system
    (void)resId;
    (void)outData;
    return false;
#endif
}

// ============================================================
// SessionManager implementation
// ============================================================

SessionManager::SessionManager() = default;

SessionManager::~SessionManager() {
    cleanup();
}

std::string SessionManager::getSessionRoot() {
    const char* tmp = std::getenv("TMP");
    if (!tmp || tmp[0] == '\0') tmp = std::getenv("TEMP");
    if (!tmp || tmp[0] == '\0') tmp = "C:\\Temp";
    return std::string(tmp) + "\\C3C";
}

std::string SessionManager::create(const std::string& arch) {
    // 1. Clean up old sessions
    cleanupOldSessions();

    // 2. Create unique session directory
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    std::string root = getSessionRoot();
    sessionDir_ = root + "\\" + std::to_string(now);
    rtlDir_ = sessionDir_ + "\\rtl";

    std::error_code ec;
    std::filesystem::create_directories(rtlDir_, ec);
    if (ec) {
        std::cerr << "C3: cannot create session directory: " << rtlDir_ << " (" << ec.message() << ")" << std::endl;
        rtlDir_.clear();
        sessionDir_.clear();
        return "";
    }

    // 3. Extract RTL resource files
    // Headers are arch-neutral (always extract), .lib depends on target arch
    struct RtlFileEntry { int id; const char* name; };
    static const RtlFileEntry headers[] = {
        { RTL_VB6RTL_H,       "vb6rtl.h" },
        { RTL_VB6COM_H,       "vb6com.h" },
        { RTL_VB6COMSERVER_H, "vb6comserver.h" },
        { RTL_VB6FORMS_H,     "vb6forms.h" },
    };

    // Select .lib resource IDs based on target architecture
    bool isX86 = (arch == "x86");
    static const RtlFileEntry libs_x64[] = {
        { RTL_VB6RTL_LIB,     "vb6rtl.lib" },       // vb6rtl + vb6com (all programs)
        { RTL_VB6RTL_DLL_LIB, "vb6rtl_dll.lib" },   // vb6comserver (ActiveX DLL)
        { RTL_VB6RTL_GUI_LIB, "vb6rtl_gui.lib" },   // vb6forms (GUI programs)
    };
    static const RtlFileEntry libs_x86[] = {
        { RTL_VB6RTL_LIB_X86,     "vb6rtl.lib" },       // vb6rtl + vb6com (all programs)
        { RTL_VB6RTL_DLL_LIB_X86, "vb6rtl_dll.lib" },   // vb6comserver (ActiveX DLL)
        { RTL_VB6RTL_GUI_LIB_X86, "vb6rtl_gui.lib" },   // vb6forms (GUI programs)
    };

    // Extract headers (arch-neutral)
    for (auto& entry : headers) {
        if (!extractResource(entry.id, entry.name, rtlDir_)) {
            std::cerr << "C3: cannot extract RTL resource: " << entry.name << std::endl;
            cleanup();
            return "";
        }
    }

    // Extract .lib files (arch-specific)
    const RtlFileEntry* libs = isX86 ? libs_x86 : libs_x64;
    int libsCount = isX86 ? (int)(sizeof(libs_x86)/sizeof(libs_x86[0]))
                          : (int)(sizeof(libs_x64)/sizeof(libs_x64[0]));
    for (int i = 0; i < libsCount; i++) {
        if (!extractResource(libs[i].id, libs[i].name, rtlDir_)) {
            std::cerr << "C3: cannot extract RTL resource: " << libs[i].name
                      << " (arch=" << arch << ")" << std::endl;
            cleanup();
            return "";
        }
    }

    return rtlDir_;
}

bool SessionManager::extractResource(int resId, const std::string& fileName, const std::string& targetDir) {
    std::vector<char> data;
    if (!loadRtlResource(resId, data)) {
        return false;
    }

    std::string filePath = targetDir + "\\" + fileName;
    std::ofstream ofs(filePath, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!ofs) return false;

    ofs.write(data.data(), static_cast<std::streamsize>(data.size()));
    return ofs.good();
}

void SessionManager::cleanup() {
    if (sessionDir_.empty()) return;

    std::error_code ec;
    std::filesystem::remove_all(sessionDir_, ec);
    // Ignoring errors - best effort cleanup

    sessionDir_.clear();
    rtlDir_.clear();
}

void SessionManager::cleanupOldSessions() {
    std::string root = getSessionRoot();
    if (!std::filesystem::exists(root)) return;

    auto now = std::chrono::steady_clock::now();
    // 300 seconds = 5 minutes
    const auto maxAge = std::chrono::seconds(300);

    std::error_code ec;
    for (auto& entry : std::filesystem::directory_iterator(root, ec)) {
        if (!entry.is_directory()) continue;

        // Parse directory name as timestamp
        auto dirName = entry.path().filename().string();
        try {
            long long ts = std::stoll(dirName);
            auto dirTime = std::chrono::steady_clock::time_point(
                std::chrono::steady_clock::duration(ts));
            auto age = now - dirTime;
            if (age > maxAge) {
                std::filesystem::remove_all(entry.path(), ec);
            }
        } catch (...) {
            // Not a valid session directory, skip
        }
    }
}

} // namespace vb6c3
