#include "backend/msvc_driver.hpp"

#include <cstdlib>
#include <cstdio>
#include <string>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#endif

namespace vb6c3 {

bool MsvcDriver::isMsvcAvailable() {
    // 1. Check if vcvarsall environment already set
    const char* vcDir = std::getenv("VCINSTALLDIR");
    if (vcDir && vcDir[0] != '\0') {
        return true;
    }
    // 2. Try running cl.exe directly (might be in PATH from other setup)
    //    走 executeCommand (CREATE_NO_WINDOW) 而不是 std::system, 避免弹 cmd 窗口
    int ret = executeCommand("cl.exe >nul 2>&1");
    if (ret == 0) return true;
    // 3. P11.4: Check if vswhere can find VS installation
    std::string vcvars = findVcvarsallBat();
    return !vcvars.empty();
}

// P11.4: Find VS installation path via vswhere.exe or registry
std::string MsvcDriver::findVsInstallPath() {
    // Method 1: vswhere.exe (VS2017+)
    const char* pf_x86 = std::getenv("ProgramFiles(x86)");
    if (!pf_x86) pf_x86 = "C:\\Program Files (x86)";
    std::string vswhere = std::string(pf_x86) + "\\Microsoft Visual Studio\\Installer\\vswhere.exe";

    if (std::filesystem::exists(vswhere)) {
        // Run vswhere to get installation path
        // -products * : include BuildTools (not just full VS editions)
        std::string cmd = "\"" + vswhere + "\" -all -latest -products * -property installationPath";
        // 隐藏窗口执行并回读 stdout (原来的 _popen 会弹 cmd 窗口)
        std::string result;
        if (executeCommandCapture(cmd, result) == 0) {
            // Trim whitespace
            while (!result.empty() && (result.back() == '\n' || result.back() == '\r' || result.back() == ' '))
                result.pop_back();
            if (!result.empty() && std::filesystem::exists(result)) {
                return result;
            }
        }
    }

    // Method 2: Registry fallback (VS2015 and earlier)
#ifdef _WIN32
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\VisualStudio\\SxS\\VS7",
                       0, KEY_READ | KEY_WOW64_32KEY, &hKey) == ERROR_SUCCESS) {
        char value[512];
        DWORD size = sizeof(value);
        // Check for VS2022 (17.0), VS2019 (16.0), VS2017 (15.0), VS2015 (14.0)
        const char* versions[] = {"17.0", "16.0", "15.0", "14.0"};
        for (const char* ver : versions) {
            size = sizeof(value);
            if (RegQueryValueExA(hKey, ver, nullptr, nullptr, (LPBYTE)value, &size) == ERROR_SUCCESS) {
                std::string path(value);
                // Trim trailing backslash
                while (!path.empty() && path.back() == '\\') path.pop_back();
                RegCloseKey(hKey);
                if (!path.empty()) return path;
            }
        }
        RegCloseKey(hKey);
    }
#endif

    // Method 3: Check well-known filesystem paths directly (fallback for BuildTools
    // when vswhere is absent or doesn't report the product)
    const char* pf_x86_2 = std::getenv("ProgramFiles(x86)");
    if (!pf_x86_2) pf_x86_2 = "C:\\Program Files (x86)";
    const char* pf_64 = std::getenv("ProgramFiles");
    if (!pf_64) pf_64 = "C:\\Program Files";
    const char* editions[] = {"BuildTools", "Community", "Professional", "Enterprise"};
    const char* versions[] = {"2022", "2019"};
    const char* bases[] = {pf_x86_2, pf_64};
    for (const char* base : bases) {
        for (const char* ver : versions) {
            for (const char* ed : editions) {
                std::string path = std::string(base) + "\\Microsoft Visual Studio\\" + ver + "\\" + ed;
                if (std::filesystem::exists(path + "\\VC\\Auxiliary\\Build\\vcvarsall.bat")) {
                    return path;
                }
            }
        }
    }

    return "";
}

// P11.4: Find vcvarsall.bat path
std::string MsvcDriver::findVcvarsallBat() {
    // 1. Check VCINSTALLDIR (already set from previous vcvarsall)
    const char* vcDir = std::getenv("VCINSTALLDIR");
    if (vcDir && vcDir[0] != '\0') {
        // M30-AX86: Normalize to guarantee a trailing backslash.
        // install_msvc.bat writes VCINSTALLDIR without one (e.g.
        // "C:\pro\c3\msvc") while the portable vcvars.bat writes it WITH one.
        // Without normalization the concatenations below yield
        // "C:\pro\c3\msvcAuxiliary\..." (missing separator), which silently
        // breaks --arch x86 on the mini toolchain deployed via install_msvc.bat
        // (LNK1112: env stays x64 because findVcvarsallBat returns "").
        std::string base = vcDir;
        if (!base.empty() && base.back() != '\\') base.push_back('\\');
        // Standard VS layout
        std::string bat = base + "Auxiliary\\Build\\vcvarsall.bat";
        if (std::filesystem::exists(bat)) return bat;
        // P24-AX86: Portable C3 mini toolchain layout (vcvars.bat lives in
        // VCINSTALLDIR root, accepts x64/x86 arg). Enables --arch x86 even when
        // the user env was permanently set to x64 by install_msvc.bat.
        std::string portable = base + "vcvars.bat";
        if (std::filesystem::exists(portable)) return portable;
    }

    // 2. Find via vswhere/registry
    std::string vsPath = findVsInstallPath();
    if (!vsPath.empty()) {
        std::string bat = vsPath + "\\VC\\Auxiliary\\Build\\vcvarsall.bat";
        if (std::filesystem::exists(bat)) return bat;
    }

    return "";
}

std::string MsvcDriver::findClExe() const {
    return "cl.exe";
}

// ai/vb-asm-extension-spec: 定位 ml64.exe。
// 与 cl.exe 同一个 MSVC bin 目录 (Hostx64/x64; 32 位宿主取 Hostx86/x64)。
// 优先 VCINSTALLDIR (vcvars 之后已设), 再走 vswhere 解析的 VS 根; 都找不到退回裸名
// "ml64.exe" (PATH —— 用户从 VS 开发者提示符启动时可命中)。
std::string MsvcDriver::findMl64Exe() {
    namespace fs = std::filesystem;
    std::vector<std::string> vcRoots;
    const char* vc = std::getenv("VCINSTALLDIR");
    if (vc && vc[0] != '\0') vcRoots.push_back(vc);
    std::string vs = findVsInstallPath();
    if (!vs.empty()) vcRoots.push_back(vs + "\\VC");

    std::error_code ec;
    for (auto root : vcRoots) {
        if (!root.empty() && root.back() != '\\') root.push_back('\\');
        std::string msvcDir = root + "Tools\\MSVC";
        if (!fs::exists(msvcDir, ec)) continue;
        for (auto& e : fs::directory_iterator(msvcDir, ec)) {
            if (!e.is_directory()) continue;
            for (const char* host : {"Hostx64\\x64", "Hostx86\\x64"}) {
                fs::path cand = e.path() / (std::string(host) + "\\ml64.exe");
                if (fs::exists(cand, ec)) return cand.string();
            }
        }
    }
    return "ml64.exe";
}

// P11.4: Build vcvarsall.bat prefix if needed
std::string MsvcDriver::buildVcvarsPrefix(const std::string& arch) const {
    // P24-05: Check if vcvarsall already set AND target arch matches current env
    const char* vcDir = std::getenv("VCINSTALLDIR");
    if (vcDir && vcDir[0] != '\0') {
        // VSCMD_ARG_TGT_ARCH is set by VS2017+ vcvarsall.bat ("x86" or "x64")
        const char* tgtArch = std::getenv("VSCMD_ARG_TGT_ARCH");
        if (tgtArch && tgtArch[0] != '\0') {
            if (arch == tgtArch) {
                return "";  // env already matches target arch
            }
            // Mismatch: must re-call vcvarsall with correct arch (e.g. x64 env but --arch x86)
            // Fall through to findVcvarsallBat below
        } else {
            // VSCMD_ARG_TGT_ARCH not set (e.g. portable toolchain via install_msvc.bat)
            // P24-AX86: x64 is the common default - assume env already matches x64.
            // For x86 target we must reconfigure env (call vcvars.bat x86) to switch
            // to the Hostx64 cross compiler and lib\x86; fall through.
            if (arch == "x64") {
                return "";
            }
            // non-x64 target with no arch hint: fall through to findVcvarsallBat
        }
    }

    // Try to find vcvarsall.bat
    std::string vcvars = findVcvarsallBat();
    if (!vcvars.empty()) {
        return "call \"" + vcvars + "\" " + arch + " >nul 2>&1 && ";
    }

    return "";
}

} // namespace vb6c3
