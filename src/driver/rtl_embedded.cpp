// P10 (restored): RTL runtime embedded resource management - implementation
// Extract 7 .h headers + 25 .c sources from c3.exe RCDATA resources.
// The P11.3 pre-compiled .lib scheme was reverted (c3 is open source);
// RTL sources are compiled by MSVC together with the generated code.

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

std::string SessionManager::create() {
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

    // 3. Extract RTL resource files (.h headers + .c sources, all arch-neutral)
    struct RtlFileEntry { int id; const char* name; };
    static const RtlFileEntry files[] = {
        { RTL_VB6RTL_H,             "vb6rtl.h" },
        { RTL_VB6RTL_C,             "vb6rtl.c" },
        { RTL_VB6COM_H,             "vb6com.h" },
        { RTL_VB6COM_C,             "vb6com.c" },
        { RTL_VB6COMSERVER_H,       "vb6comserver.h" },
        { RTL_VB6COMSERVER_C,       "vb6comserver.c" },
        { RTL_VB6FORMS_H,           "vb6forms.h" },
        { RTL_VB6FORMS_C,           "vb6forms.c" },
        { RTL_VB6_DI_STUBS_C,       "vb6_di_stubs.c" },
        { RTL_VB6_DI_WIN32_STUBS_C, "vb6_di_win32_stubs.c" },
        { RTL_VB6_DI_USER32_STUBS_C,   "vb6_di_user32_stubs.c" },
        { RTL_VB6_DI_GDIPLUS_STUBS_C,  "vb6_di_gdiplus_stubs.c" },
        { RTL_VB6_DI_CRYPTO_STUBS_C,   "vb6_di_crypto_stubs.c" },
        { RTL_VB6_DI_COM_STUBS_C,      "vb6_di_com_stubs.c" },
        { RTL_VB6_DI_NET_STUBS_C,      "vb6_di_net_stubs.c" },
        { RTL_VB6_DI_SHELL_STUBS_C,    "vb6_di_shell_stubs.c" },
        { RTL_VB6COMSERVER_INTERNAL_H, "vb6comserver_internal.h" },
        { RTL_VB6COMSERVER_OBJ_C,      "vb6comserver_obj.c" },
        { RTL_VB6COMSERVER_FACTORY_C,  "vb6comserver_factory.c" },
        { RTL_VB6COMSERVER_CP_C,       "vb6comserver_cp.c" },
        { RTL_VB6COMSERVER_PCI_C,      "vb6comserver_pci.c" },
        { RTL_VB6COM_INTERNAL_H,       "vb6com_internal.h" },
        { RTL_VB6COM_INVOKE_C,         "vb6com_invoke.c" },
        { RTL_VB6COM_PACK_C,           "vb6com_pack.c" },
        { RTL_VB6COM_WRAP_C,           "vb6com_wrap.c" },
        { RTL_VB6COM_SINK_C,           "vb6com_sink.c" },
        { RTL_VB6COM_FOREACH_C,        "vb6com_foreach.c" },
        { RTL_VB6FORMS_INTERNAL_H,     "vb6forms_internal.h" },
        { RTL_VB6FORMS_CTRL_C,         "vb6forms_ctrl.c" },
        { RTL_VB6FORMS_LIST_C,         "vb6forms_list.c" },
        { RTL_VB6FORMS_STYLE_C,        "vb6forms_style.c" },
        { RTL_VB6FORMS_SCROLL_C,       "vb6forms_scroll.c" },
        { RTL_VB6FORMS_PICTURE_C,      "vb6forms_picture.c" },
        { RTL_VB6FORMS_CTRLARR_C,      "vb6forms_ctrlarr.c" },
        { RTL_VB6FORMS_WEBVIEW_C,      "vb6forms_webview.c" },
        { RTL_VB6FORMS_WIDGET_C,       "vb6forms_widget.c" },
        { RTL_VB6FORMS_SHAPE_C,        "vb6forms_shape.c" },
        { RTL_VB6FORMS_AXSITE_C,       "vb6forms_axsite.c" },

        // vb6rtl.h 按家族拆分 (2026-09-17)
        { RTL_VB6RTL_BASE_H,                  "vb6rtl_base.h" },
        { RTL_VB6RTL_BSTR_H,                  "vb6rtl_bstr.h" },
        { RTL_VB6RTL_VARIANT_H,               "vb6rtl_variant.h" },
        { RTL_VB6RTL_BUILTIN_H,               "vb6rtl_builtin.h" },
        { RTL_VB6RTL_ARRAY_H,                 "vb6rtl_array.h" },
        { RTL_VB6RTL_CLASS_COM_H,             "vb6rtl_class_com.h" },
        { RTL_VB6RTL_RUNTIME_H,               "vb6rtl_runtime.h" },

        // vb6forms 家族再细分 (2026-09-17)
        { RTL_VB6FORMS_PICTURE_PROP_C,        "vb6forms_picture_prop.c" },
        { RTL_VB6FORMS_WIDGET_PROP_C,         "vb6forms_widget_prop.c" },

        // vb6forms.h 按功能域拆分 (2026-09-17)
        { RTL_VB6FORMS_WINDOW_H,              "vb6forms_window.h" },
        { RTL_VB6FORMS_PROP_H,                "vb6forms_prop.h" },
        { RTL_VB6FORMS_PROP_CTRL_H,           "vb6forms_prop_ctrl.h" },
        { RTL_VB6FORMS_PROP_PIC_H,            "vb6forms_prop_pic.h" },
        { RTL_VB6FORMS_PROP_FORM_H,           "vb6forms_prop_form.h" },
        { RTL_VB6FORMS_CTRLARR_H,             "vb6forms_ctrlarr.h" },
        { RTL_VB6FORMS_MDI_H,                 "vb6forms_mdi.h" },
        { RTL_VB6FORMS_WEBVIEW_H,             "vb6forms_webview.h" },
        { RTL_VB6FORMS_CONTROLS_H,            "vb6forms_controls.h" },

        // vb6rtl.c 按家族拆分 (2026-09-17)
        { RTL_VB6RTL_STRING_C,                "vb6rtl_string.c" },
        { RTL_VB6RTL_FORMAT_C,                "vb6rtl_format.c" },
        { RTL_VB6RTL_FORMAT_EXTRACT_INC,      "vb6rtl_format_extract.inc" },
        { RTL_VB6RTL_FORMAT_PARSE_INC,        "vb6rtl_format_parse.inc" },
        { RTL_VB6RTL_FORMAT_NUMERIC_PRE_INC,  "vb6rtl_format_numeric_pre.inc" },
        { RTL_VB6RTL_FORMAT_NUMERIC_BODY_INC, "vb6rtl_format_numeric_body.inc" },
        { RTL_VB6RTL_FORMAT_NUMERIC_TAIL_INC, "vb6rtl_format_numeric_tail.inc" },
        { RTL_VB6RTL_FORMAT_STRING_INC,       "vb6rtl_format_string.inc" },
        { RTL_VB6RTL_CONV_C,                  "vb6rtl_conv.c" },
        { RTL_VB6RTL_MISC_C,                  "vb6rtl_misc.c" },
        { RTL_VB6RTL_SYSTEM_C,                "vb6rtl_system.c" },
        { RTL_VB6RTL_COMPAT_C,                "vb6rtl_compat.c" },
        { RTL_VB6RTL_DATE_C,                  "vb6rtl_date.c" },
        { RTL_VB6RTL_ARRAY_C,                 "vb6rtl_array.c" },
        { RTL_VB6RTL_FILE_C,                  "vb6rtl_file.c" },
        { RTL_VB6RTL_PARAMARRAY_C,            "vb6rtl_paramarray.c" },
        { RTL_VB6RTL_FINANCIAL_C,             "vb6rtl_financial.c" },
        { RTL_VB6RTL_COM_C,                   "vb6rtl_com.c" },
        { RTL_VB6RTL_REGISTRY_C,              "vb6rtl_registry.c" },
    };

    for (auto& entry : files) {
        if (!extractResource(entry.id, entry.name, rtlDir_)) {
            std::cerr << "C3: cannot extract RTL resource: " << entry.name << std::endl;
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
