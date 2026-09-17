#include "backend/msvc_driver.hpp"
#include "common/encoding.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <unordered_map>
#include <vector>
#include <string>

namespace vb6c3 {

namespace {

// opt3: FNV-1a 64位哈希 (跨运行稳定, 不依赖标准库实现)
std::string fnv1aHex(const void* data, size_t len) {
    uint64_t h = 1469598103934665603ULL;
    const unsigned char* p = static_cast<const unsigned char*>(data);
    for (size_t i = 0; i < len; ++i) {
        h ^= p[i];
        h *= 1099511628211ULL;
    }
    std::ostringstream oss;
    oss << std::hex << h;
    return oss.str();
}

std::string hashString(const std::string& s) {
    return fnv1aHex(s.data(), s.size());
}

// 文件内容哈希; 读取失败返回空串
std::string hashFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    uint64_t h = 1469598103934665603ULL;
    char buf[65536];
    while (f.read(buf, sizeof(buf)) || f.gcount() > 0) {
        std::streamsize n = f.gcount();
        const unsigned char* p = reinterpret_cast<const unsigned char*>(buf);
        for (std::streamsize i = 0; i < n; ++i) {
            h ^= p[i];
            h *= 1099511628211ULL;
        }
    }
    std::ostringstream oss;
    oss << std::hex << h;
    return oss.str();
}

// 解析 .c 文件中的本地头文件 #include "xxx.h"，返回绝对路径列表
// (opt3: 依赖跟踪 — 任一依赖头文件变化即触发该 .c 重编译)
// srcDir: 生成代码目录; rtlDir: RTL 源码目录 (释放的 vb6rtl.h 等)
std::vector<std::string> collectLocalIncludes(const std::string& cPath, const std::string& srcDir,
                                              const std::string& rtlDir) {
    std::vector<std::string> incs;
    std::ifstream f(cPath);
    if (!f) return incs;
    std::string line;
    while (std::getline(f, line)) {
        size_t hash = line.find('#');
        if (hash == std::string::npos) continue;
        size_t q1 = line.find('"', hash);
        if (q1 == std::string::npos) continue;
        size_t q2 = line.find('"', q1 + 1);
        if (q2 == std::string::npos) continue;
        std::string name = line.substr(q1 + 1, q2 - q1 - 1);
        if (name.size() < 3) continue;
        if (name.compare(name.size() - 2, 2, ".h") != 0) continue;
        // 绝对路径: 优先 srcDir (生成代码头), 不存在则查 rtlDir (RTL 头)
        std::string p = srcDir + "/" + name;
        if (!std::filesystem::exists(utf8ToPath(p)) && !rtlDir.empty()) {
            p = rtlDir + "/" + name;
        }
        incs.push_back(p);
    }
    return incs;
}

} // namespace

// opt3: 增量编译 — 基于内容哈希的 obj 级缓存。
// 每个 .c 的缓存记录 = 源文件哈希 + 依赖头文件哈希 + 编译选项指纹。
// 命中且缓存 obj 存在 → 跳过 cl 编译，直接复用 obj 链接。
bool MsvcDriver::compileAndLinkIncremental(const MsvcDriverOptions& options) {
    std::string cl = findClExe();
    std::string arch = options.arch;

    // obj 输出目录 (新编译的 obj 先落在这里, 链接成功后复制到缓存目录)
    std::string objDir = options.objDir;
    if (objDir.empty() && !options.outputFile.empty()) {
        std::filesystem::path outP(utf8ToPath(options.outputFile));
        objDir = pathToUtf8(outP.parent_path());
    }
    if (objDir.empty()) objDir = ".";
    std::string tmpLogPath = objDir + "/_c3_msvc_out.txt";

    // 持久缓存目录
    std::string cacheDir = options.incrementalCacheDir;
    if (cacheDir.empty()) cacheDir = objDir;
    std::error_code ec;
    std::filesystem::create_directories(utf8ToPath(cacheDir), ec);
    std::string cacheFile = cacheDir + "/cache.txt";

    // 编译选项指纹: 选项变化(优化级别/架构/DLL标志等)会使全部缓存失效
    std::string optsFp = hashString(
        options.arch + "|" +
        std::to_string(options.isDll ? 1 : 0) + "|" +
        std::to_string(options.isGui ? 1 : 0) + "|" +
        std::to_string(options.optimizationLevel) + "|" +
        std::to_string(options.debugInfo ? 1 : 0));

    // 读缓存: objName -> record
    std::unordered_map<std::string, std::string> cache;
    {
        std::ifstream f(cacheFile);
        std::string line;
        while (std::getline(f, line)) {
            size_t tab = line.find('\t');
            if (tab != std::string::npos) {
                cache[line.substr(0, tab)] = line.substr(tab + 1);
            }
        }
    }

    // 公共编译选项 (不含 /Fe /Fo /MP /c 与源文件)
    std::ostringstream common;
    common << cl;
    if (!options.rtlDir.empty()) common << " /I\"" << options.rtlDir << "\"";
    if (!options.srcDir.empty()) common << " /I\"" << options.srcDir << "\"";
    switch (options.optimizationLevel) {
        case 0: common << " /Od"; break;
        case 1: common << " /O1"; break;
        case 2: common << " /O2"; break;
        case 3: common << " /Ox"; break;
    }
    if (options.debugInfo) common << " /Zi";
    common << " /std:c11 /DUNICODE /D_UNICODE /utf-8 /D_CRT_SECURE_NO_WARNINGS /D_CRT_NONSTDC_NO_WARNINGS";
    common << " /W3";
    // P10 恢复: RTL 源码直接编译, /Gy 函数级链接配合 /OPT:REF 剔除未引用 RTL 代码
    common << " /Gy";
    if (options.arch == "x86") common << " /MT";

    // 增量判断
    std::vector<std::string> toCompile;   // 需要重编译的 .c
    std::vector<std::string> reusedObjs;  // 缓存命中的 obj 完整路径 (cacheDir)
    std::vector<std::string> newObjs;     // 新编译 obj 的预测路径 (objDir)
    std::unordered_map<std::string, std::string> newCache; // objName -> record
    int hitCount = 0;
    for (const auto& src : options.sourceFiles) {
        std::filesystem::path sp(utf8ToPath(src));
        std::string objName = pathToUtf8(sp.stem()) + ".obj";
        std::string srcHash = hashFile(src);
        if (srcHash.empty()) {  // 读不到源文件: 必须编译
            toCompile.push_back(src);
            newObjs.push_back(objDir + "/" + objName);
            continue;
        }
        auto incs = collectLocalIncludes(src, options.srcDir, options.rtlDir);
        std::string deps;
        for (auto& inc : incs) deps += hashFile(inc);
        std::string record = srcHash + " " + hashString(deps) + " " + optsFp;
        newCache[objName] = record;
        std::string cachedObj = cacheDir + "/" + objName;
        if (cache.count(objName) && cache[objName] == record &&
            std::filesystem::exists(utf8ToPath(cachedObj))) {
            reusedObjs.push_back(cachedObj);
            hitCount++;
        } else {
            toCompile.push_back(src);
            newObjs.push_back(objDir + "/" + objName);
        }
    }

    if (options.verbose) {
        std::cout << "C3: 增量编译: " << hitCount << "/" << options.sourceFiles.size()
                  << " 个源文件命中缓存, 跳过编译" << std::endl;
    }

    // === 编译需要重编的 .c (仅这些) ===
    if (!toCompile.empty()) {
        std::ostringstream compileCmd;
        compileCmd << common.str() << " /MP /c";
        if (!options.outputFile.empty()) {
            compileCmd << " /Fo\"" << objDir << "/\"";
        }
        for (const auto& src : toCompile) {
            compileCmd << " \"" << src << "\"";
        }
        std::string rspPath = objDir + "/_c3_cl_args.rsp";
        {
            std::ofstream rspFile(rspPath, std::ios::out | std::ios::trunc);
            if (rspFile) rspFile << compileCmd.str().substr(cl.length());
        }
        std::string vcvarsPrefix = buildVcvarsPrefix(arch);
        std::string fullCmd = vcvarsPrefix + cl + " @\"" + rspPath + "\" > \"" + tmpLogPath + "\" 2>&1";
        int ret = executeCommand(fullCmd);
        if (ret != 0) {
            std::string outputDirForLog;
            if (!options.outputFile.empty()) {
                std::filesystem::path outP(utf8ToPath(options.outputFile));
                outputDirForLog = pathToUtf8(outP.parent_path());
            }
            if (outputDirForLog.empty()) outputDirForLog = ".";
            std::string errorLogPath = outputDirForLog + "/c3-error.log";
            std::ifstream tmpLog(tmpLogPath);
            std::ofstream errLog(errorLogPath, std::ios::out | std::ios::trunc);
            if (tmpLog && errLog) {
                errLog << "C3: Compilation failed (exit code " << ret << ")" << std::endl;
                errLog << "=== MSVC Output ===" << std::endl;
                std::string line;
                while (std::getline(tmpLog, line)) errLog << line << "\n";
            }
            if (tmpLog) {
                tmpLog.clear();
                tmpLog.seekg(0);
                std::string line;
                while (std::getline(tmpLog, line)) std::cerr << line << std::endl;
            }
            std::cerr << "C3: 编译失败 (exit code " << ret << ")" << std::endl;
            std::cerr << "C3: 错误日志已保存: " << errorLogPath << std::endl;
            std::filesystem::remove(tmpLogPath, std::error_code());
            std::filesystem::remove(rspPath, std::error_code());
            return false;
        }
        std::filesystem::remove(tmpLogPath, std::error_code());
        std::filesystem::remove(rspPath, std::error_code());

        // 复制新 obj 到持久缓存目录
        for (const auto& src : toCompile) {
            std::filesystem::path sp(utf8ToPath(src));
            std::string objName = pathToUtf8(sp.stem()) + ".obj";
            std::string srcObj = objDir + "/" + objName;
            std::string dstObj = cacheDir + "/" + objName;
            if (std::filesystem::exists(utf8ToPath(srcObj))) {
                std::error_code ec2;
                std::filesystem::copy_file(utf8ToPath(srcObj), utf8ToPath(dstObj),
                                           std::filesystem::copy_options::overwrite_existing, ec2);
            }
        }
    }

    // 更新缓存索引 (总是写, 保证新模块/新记录持久)
    {
        std::ofstream f(cacheFile, std::ios::out | std::ios::trunc);
        for (auto& kv : newCache) {
            f << kv.first << "\t" << kv.second << "\n";
        }
    }

    // === 链接 (所有 obj: 复用的 + 新编译的) ===
    // 注意: 拆分为两步后编译阶段无源文件, cl /link 不会进入链接模式(D8003),
    // 因此链接阶段直接调用 link.exe (vcvarsall 后位于 PATH)。
    std::string linkExe = "link.exe";
    std::ostringstream linkCmd;
    linkCmd << linkExe << " /NOLOGO";
    if (options.isDll) {
        linkCmd << " /DLL";
    } else if (options.isGui) {
        linkCmd << " /SUBSYSTEM:WINDOWS";
    } else {
        linkCmd << " /SUBSYSTEM:CONSOLE";
    }
    if (!options.outputFile.empty()) linkCmd << " /OUT:\"" << options.outputFile << "\"";
    for (auto& o : reusedObjs) linkCmd << " \"" << o << "\"";
    for (auto& o : newObjs) linkCmd << " \"" << o << "\"";
    if (options.isDll) {
        if (!options.typelibResFile.empty()) linkCmd << " \"" << options.typelibResFile << "\"";
        if (!options.versionInfoResFile.empty()) linkCmd << " \"" << options.versionInfoResFile << "\"";
        if (!options.userResFile.empty()) linkCmd << " \"" << options.userResFile << "\"";
        if (!options.defFile.empty()) linkCmd << " /DEF:\"" << options.defFile << "\"";
        linkCmd << " ole32.lib oleaut32.lib uuid.lib advapi32.lib user32.lib shell32.lib gdi32.lib";
    } else if (options.isGui) {
        if (!options.typelibResFile.empty()) linkCmd << " \"" << options.typelibResFile << "\"";
        if (!options.versionInfoResFile.empty()) linkCmd << " \"" << options.versionInfoResFile << "\"";
        if (!options.userResFile.empty()) linkCmd << " \"" << options.userResFile << "\"";
        linkCmd << " user32.lib gdi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib advapi32.lib";
    } else {
        if (!options.typelibResFile.empty()) linkCmd << " \"" << options.typelibResFile << "\"";
        if (!options.versionInfoResFile.empty()) linkCmd << " \"" << options.versionInfoResFile << "\"";
        if (!options.userResFile.empty()) linkCmd << " \"" << options.userResFile << "\"";
        linkCmd << " ole32.lib oleaut32.lib uuid.lib advapi32.lib user32.lib shell32.lib gdi32.lib";
    }
    if (options.arch == "x86") linkCmd << " /MACHINE:X86";
    if (options.debugInfo) linkCmd << " /DEBUG";

    std::string linkRsp = objDir + "/_c3_link_args.rsp";
    {
        std::ofstream rspFile(linkRsp, std::ios::out | std::ios::trunc);
        if (rspFile) rspFile << linkCmd.str().substr(linkExe.length());
    }
    std::string vcvarsPrefix2 = buildVcvarsPrefix(arch);
    std::string fullLinkCmd = vcvarsPrefix2 + linkExe + " @\"" + linkRsp + "\" > \"" + tmpLogPath + "\" 2>&1";

    if (options.verbose) {
        std::cout << "C3: 执行: " << fullLinkCmd << std::endl;
    }

    int ret2 = executeCommand(fullLinkCmd);
    if (ret2 != 0) {
        std::string outputDirForLog;
        if (!options.outputFile.empty()) {
            std::filesystem::path outP(utf8ToPath(options.outputFile));
            outputDirForLog = pathToUtf8(outP.parent_path());
        }
        if (outputDirForLog.empty()) outputDirForLog = ".";
        std::string errorLogPath = outputDirForLog + "/c3-error.log";
        std::ifstream tmpLog(tmpLogPath);
        std::ofstream errLog(errorLogPath, std::ios::out | std::ios::trunc);
        if (tmpLog && errLog) {
            errLog << "C3: Compilation failed (exit code " << ret2 << ")" << std::endl;
            errLog << "=== MSVC Output ===" << std::endl;
            std::string line;
            while (std::getline(tmpLog, line)) errLog << line << "\n";
        }
        if (tmpLog) {
            tmpLog.clear();
            tmpLog.seekg(0);
            std::string line;
            while (std::getline(tmpLog, line)) std::cerr << line << std::endl;
        }
        std::cerr << "C3: 编译失败 (exit code " << ret2 << ")" << std::endl;
        std::cerr << "C3: 错误日志已保存: " << errorLogPath << std::endl;
        std::filesystem::remove(tmpLogPath, std::error_code());
        std::filesystem::remove(linkRsp, std::error_code());
        return false;
    }
    std::filesystem::remove(tmpLogPath, std::error_code());
    std::filesystem::remove(linkRsp, std::error_code());

    if (options.verbose) {
        std::cout << "C3: 编译成功: " << options.outputFile << std::endl;
    }
    return true;
}

} // namespace vb6c3
