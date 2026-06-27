// P9: TypeLib 内建生成器 — 替代 MIDL 外部工具
// 使用 Windows CreateTypeLib2 API 直接生成 .tlb 文件
// 消除 4MB MIDL 分发依赖，ActiveX DLL 编译成为真正单步操作
//
// 版权: vb6c3 项目

#ifndef VB6C3_TYPELIB_BUILDER_HPP
#define VB6C3_TYPELIB_BUILDER_HPP

#include "semantics/symbol_table.hpp"
#include <string>
#include <vector>

namespace vb6c3 {

// ============================================================
// TypeLibBuilder: 基于 CreateTypeLib2 API 的 TypeLib 生成器
// ============================================================
//
// 使用流程:
//   TypeLibBuilder builder;
//   builder.beginLib("MyLib.TypeLib", libIdStr, "MyLib");
//   builder.addDispInterface("_Calc", iidStr, methods...);
//   builder.addCoClass("Calc", clsidStr, "_Calc");
//   builder.endLib("output/MyLib.tlb");
//
// 类型映射: Vb6Type -> VARTYPE (用于 FUNCDESC.elemdescFunc 等)
// ============================================================

class TypeLibBuilder {
public:
    TypeLibBuilder();
    ~TypeLibBuilder();

    // --- 生命周期 ---

    /// 开始构建 TypeLib
    /// @param name      类型库内部名 (如 "MyLib.TypeLib")
    /// @param libidStr  LibID UUID 字符串 (如 "{...}"), 空则自动生成
    /// @param helpString 帮助字符串
    /// @return true=成功
    bool beginLib(const std::string& tlbPath,
                  const std::string& libidStr,
                  const std::string& helpString,
                  const std::string& libName = "");

    /// 添加 dispinterface (自动生成 IDispatch 实现)
    /// @param name    接口名 (如 "_Calc")
    /// @param iidStr  接口 IID, 空则自动生成
    /// @param methods 方法列表 (name, returnType, params, dispid)
    /// @return true=成功
    struct MethodInfo {
        std::string name;                       // 方法名
        int32_t dispid = 0;                     // DISPID
        Vb6Type returnType = Vb6Type::Void;     // 返回类型
        bool isPropertyGet = false;             // Property Get
        bool isPropertyPut = false;             // Property Let
        bool isPropertyPutRef = false;          // Property Set
        std::vector<ParameterInfo> params;      // 参数列表
    };

    bool addDispInterface(const std::string& name,
                          const std::string& iidStr,
                          const std::vector<MethodInfo>& methods);

    /// 添加 coclass
    /// @param name       类名 (如 "Calc")
    /// @param clsidStr   CLSID, 空则自动生成
    /// @param ifaceName  默认接口名 (如 "_Calc")
    /// @return true=成功
    bool addCoClass(const std::string& name,
                    const std::string& clsidStr,
                    const std::string& ifaceName);

    /// 结束构建, 保存到文件
    /// @param tlbPath 输出 .tlb 文件路径
    /// @return true=成功
    bool endLib(const std::string& tlbPath);

    // --- 工具 ---

    /// Vb6Type -> VARTYPE 映射
    static unsigned short mapVartype(Vb6Type t);

    /// 自动生成 UUID (基于名称的确定性 UUID, 公开供 Driver 使用)
    static std::string generateUuid(const std::string& seed);

    /// 获取最后的错误信息
    const std::string& lastError() const { return lastError_; }

private:
    std::string lastError_;
    bool libOpen_ = false;

    // OLE 类型库创建接口 (不透明指针, 避免头文件依赖)
    void* pCreateLib_ = nullptr;    // ICreateTypeLib2*

    // 已添加的接口名列表 (用于 coclass AddImplType 引用)
    struct InterfaceInfo {
        std::string name;
        void* pTypeInfo = nullptr;  // ITypeInfo* (AddRef'd)
        int32_t index = -1;         // TypeLib 内序号
    };
    std::vector<InterfaceInfo> interfaces_;

};

} // namespace vb6c3

#endif // VB6C3_TYPELIB_BUILDER_HPP
