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
//   builder.beginLib("output/MyLib.tlb", libIdStr, "MyLib TypeLib", "MyLib");
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
    /// @param name      类型库内部名 (VB6 约定 = 工程名, 如 "MyLib"; 客户端按
    ///                  "<类型库名>.<coclass名>" 拼 ProgID, 因此不可加 .TypeLib 之类后缀)
    /// @param libidStr  LibID UUID 字符串 (如 "{...}"), 空则自动生成
    /// @param helpString 帮助字符串
    /// @param is64      目标位数 (ai/022 B16): 决定 CreateTypeLib2 的 SYS_WIN64/SYS_WIN32。
    ///                  实测 (D62-3/B16 测量③) 这枚 flag 进 .tlb 字节, 而且建库的 oVft 会按
    ///                  "写入端指针宽" 存、按 "读取端指针宽" 换算 ⇒ 恒 64 会让 32 位客户端
    ///                  读到双倍偏移 (同一份 oVft=24 在 32 位库里读回 48)。所以它必须跟着
    ///                  `--arch` 走; 默认架构 (x64) 下与旧行为逐字节相同。
    /// @return true=成功
    bool beginLib(const std::string& tlbPath,
                  const std::string& libidStr,
                  const std::string& helpString,
                  const std::string& libName = "",
                  bool is64 = true);

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
    /// @param name           类名 (如 "Calc")
    /// @param clsidStr       CLSID, 空则自动生成
    /// @param ifaceName      默认接口名 (如 "_Calc")
    /// @param sourceIfaceName 事件源接口名 (如 "_CalcEvents"), 空则无事件源
    /// @return true=成功
    bool addCoClass(const std::string& name,
                    const std::string& clsidStr,
                    const std::string& ifaceName,
                    const std::string& sourceIfaceName = "");

    /// ai/022 B15/B16: 添加**真接口** (TKIND_INTERFACE) 并把契约成员如实发进去。
    /// 与 addDispInterface 的区别是 kind 与调用契约两样: 这一档的 GUID 是 QI 会认的那枚
    /// IID, 成员是 vtable 槽 (FUNC_PUREVIRTUAL), 且 **oVft/callconv/参数类型按真实生成码写**。
    /// 为什么必须如实: 生成码自 B16 起整条薄面都是 `__stdcall` (测量①), 与这里写的
    /// `CC_STDCALL` + `oVft=(3+槽号)*指针宽` 一一对应; 返回值仍是原生类型 (不是 HRESULT +
    /// [out,retval]) —— 那是"canonical COM"的另一半, 属 B17 (见 022 D63)。
    /// @param name    接口名 (用接口自己的名字, 不加 "_" 前缀)
    /// @param iidStr  接口 IID, 空则自动生成
    /// @param methods 契约槽, 顺序 == 生成码里 vb6_ivtbl_<I> 的槽序 (继承来的在前)
    /// @return true=成功
    bool addVtableInterface(const std::string& name,
                            const std::string& iidStr,
                            const std::vector<MethodInfo>& methods);

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
    bool is64_ = true;              // 目标位数 (BeginLib 的参), 决定 oVft 的指针宽

    // OLE 类型库创建接口 (不透明指针, 避免头文件依赖)
    void* pCreateLib_ = nullptr;    // ICreateTypeLib2*

    // 已添加的接口名列表 (用于 coclass AddImplType 引用)
    struct InterfaceInfo {
        std::string name;
        void* pTypeInfo = nullptr;       // ITypeInfo* (AddRef'd)
        void* pCreateTypeInfo = nullptr; // ICreateTypeInfo* (NOT released until endLib)
        int32_t index = -1;              // TypeLib 内序号
    };
    std::vector<InterfaceInfo> interfaces_;

    // vtable 接口 ByRef 形参的 VT_PTR 链内层 TYPEDESC。AddFuncDesc 之后它仍可能被
    // 读回 (SaveAllChanges 时), 所以不能挂在栈上的局部 vector 里; 类型库接口这里
    // 刻意不引 Windows 头, 用 void* 存 TYPEDESC 数组, 由 .cpp 负责 new[]/delete[]。
    std::vector<void*> vtableInnerAllocs_;

};

} // namespace vb6c3

#endif // VB6C3_TYPELIB_BUILDER_HPP
