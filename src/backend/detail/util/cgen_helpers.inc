// cgen_helpers.inc - CCodeGen 的 private 方法声明段（类体片段）
// 由 src/backend/cgen.hpp 拆出（2026-09-17），在类体内被 #include，非独立头文件。
// 内容 = 原 cgen.hpp 第 616~993 行，逐行未改；类体收尾 `};` 由伞头给出。

    // ---- 类型映射 ----

    // Vb6Type → C类型字符串
    std::string mapType(Vb6Type type) const;

    // Vb6Type → COM vtable 方法参数 C类型字符串 (数组→SAFEARRAY*等)
    std::string mapComType(Vb6Type type) const;

    // IID字符串 {XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX} → GUID结构体初始化字符串
    std::string emitGuidInitializer(const std::string& iidStr) const;

    // TypeRefPtr → C类型字符串 (非const: P6.3收集COM接口类型名)
    std::string mapTypeRef(ASTNode* typeRef);

    // Fix 081e: Declare函数返回类型映射 - Long/LongPtr → intptr_t (x64指针安全)
    std::string mapDeclareType(ASTNode* typeRef);

    // Fix 010b: 尝试将AST表达式常量折叠为int64_t (用于enum成员值)
    // 成功返回true并设置result, 失败返回false
    bool tryEvalConstInt(ASTNode* expr, int64_t& result);

    // VB6默认值 → C表达式
    std::string defaultValue(Vb6Type type) const;

    // ---- 标识符命名 ----

    // VB6标识符 → 安全C标识符 (处理关键字冲突、特殊字符)
    std::string cIdent(const std::string& vb6Name) const;

    // P8.4: 根据表达式类型推断Variant构造函数
    // 例如: 整数字面量→vb6_VariantLong, 浮点→vb6_VariantDouble, BSTR→vb6_VariantString
    std::string wrapVariantValue(ASTNode* valueNode, const std::string& cExpr) const;

    // VB6标识符 → C全局函数名
    // 本模块: vb6_<ProcName>
    // 跨模块(外部): vb6_<ModuleName>_<ProcName>
    std::string cProcName(const std::string& procName, AccessLevel access,
                          const std::string& sourceModule = "") const;

    // ---- 表达式求值 ----

    // 生成C表达式字符串, 存入lastExpr_
    void emitExpr(Expr& expr);

    // ---- 语句生成 ----

    // 生成语句列表
    void emitStmtList(StmtList& stmts, bool emitResumePoints = false);

    // ---- 声明生成 ----

    // 生成模块级声明
    void emitModuleDecl(Decl& decl);

    // 生成函数签名 (不含函数体)
    std::string makeProcSignature(SubDecl& node);
    std::string makeProcSignature(FunctionDecl& node);

    // 生成参数列表
    std::string makeParamList(std::vector<std::unique_ptr<ParameterDecl>>& params, bool isDeclare = false);

    // Fix 084k: 生成单个参数的C类型+名字 ("int32_t x" / "vb6_cls_cWinsock** o"),
    // 与makeParamList逐参数逻辑完全一致, 供事件包装器等复用
    std::string makeParamCType(ParameterDecl* p, bool isDeclare = false);

    // 生成Property签名 (不含函数体)
    std::string makePropertySignature(PropertyDecl& node);

    // 生成类工厂函数 (New/Destroy)
    void emitClassFactory(Module& module);
    // Fix 099: 生成 Public 字段的 COM 读写访问器 (ActiveX DLL 专用, 见 cgen_com.cpp)
    void emitClassFieldAccessors(Module& module);
    void emitInterfaceVtable(Module& module);
    void emitEventSink(Module& module);
    void emitComVtableSinkDecls();  // .h 前向声明
    void emitComVtableSinks();      // .c 实现



    // P7: 生成Win32窗体框架代码 (WndProc + 控件创建 + 消息映射)
    void emitFormFramework(const FrmFormDesc& frmDesc, Module& module);

    // P7.8: 递归生成菜单项 (VB.Menu子项)
    void emitMenuItem(const std::string& parentVar, const FrmControl& menuCtrl, int& menuId);

    // P7.8: 递归生成菜单点击事件派发 (WM_COMMAND中)
    void emitMenuClickDispatch(const FrmControl& menuCtrl, int& menuId);

    // M22: 转义宽C字符串（UTF-8→\xNNNN），用于AppendMenuW等宽字符API
    static std::string escapeWideCString(const std::string& s);

    // M22: 将非BSTR表达式包装为BSTR (用于字符串连接运算)
    std::string wrapToBSTR(const std::string& expr, Expr& node);

    // Fix 100a: 判断表达式本身是否为"返回 BSTR 的调用" (顶层前缀判定, 非子串判定).
    // 用于 wrapToBSTR 的 BSTR 直通分支 — 实参中含 vb6_BSTR_FromStr 的标量函数
    // (vb6_Len("abc") 等) 不得因未锚定子串检查被判为已是 BSTR.
    static bool isBstrReturningCall(const std::string& expr);

    // P7.8: 转义C字符串中的特殊字符
    static std::string escapeCString(const std::string& s);

    // P7.5: 判断控件属性访问 → 返回RTL读取函数名 (如"vb6_GetControlText")
    // 空字符串表示不是已知控件属性
    std::string getControlPropReadFn(FrmControlType ctrlType, const std::string& propName) const;

    // P7.5: 判断控件属性写入 → 返回RTL写入函数名 (如"vb6_SetControlText")
    std::string getControlPropWriteFn(FrmControlType ctrlType, const std::string& propName) const;
    // P20-36: 生成控件属性访问的HWND参数 (Menu控件用GetMenu+menuId)
    std::string makeCtrlHwndArg(const std::string& ctrlNameLower, FrmControlType ctrlType) const;

    // 应用 .frm 设计期样式属性 (BorderStyle / Alignment)。顶层控件与容器子控件共用，
    // hwndExpr 为控件句柄表达式 (如 "vb6_hwnd_Foo")。
    void emitDesignerStyleProps(const FrmControl& ctrl, const std::string& hwndExpr);

    // 返回控件类型相关的 Win32 样式位 (BS_*/SS_*/ES_*/CBS_*/LBS_*/WS_BORDER...)。
    // 顶层控件与容器(Frame/PictureBox)子控件必须使用同一套逻辑，否则子控件缺
    // SS_NOTIFY/BS_GROUPBOX 等样式会导致外观/交互 (含 tooltip) 异常。
    long controlTypeStyleBits(const FrmControl& ctrl) const;

    // VB6 PictureBox/Image 的 STATIC 控件不显示 Caption (标题在创建时应清空)。
    bool controlTypeClearsCaption(const FrmControl& ctrl) const;

    // P11.7: 返回控件类型的默认属性名 (如 TextBox->"Text", Label->"Caption")
    // 空字符串表示该控件类型无默认属性
    static const char* getDefaultPropertyName(FrmControlType ctrlType);

    // 生成类方法函数体中的Me引用名
    std::string classMeParam() const;

    // Fix 019: 在事件包装函数体内把 void* handler 转换为类指针类型
    // 用于替代历史上误用的 classMeParam() (那是参数声明, 不能作为函数调用实参)
    std::string classHandlerCast() const;

    // ---- 二元运算符映射 ----
    std::string mapBinaryOp(BinaryOp op) const;

    // ---- 表达式类型推断 ----
    // 推断表达式的Vb6Type（简化版，用于Select Case等需要类型判断的场景）
    Vb6Type inferExprType(Expr& expr) const;

    // Fix 029: 严格 Variant 推断 — 仅当表达式"明确"为 Variant/Variant 数组时返回 true.
    // 与 inferExprType 不同: inferExprType 对内置函数/Udt 字段访问等无法识别的情况
    // 会回退到 Variant (默认值), 此处不认为那些是 Variant, 避免误用 vb6_VariantToXxx 包装.
    // isArrOut: 若不为 nullptr, 用于返回是否为 Variant() 数组类型.
    bool isDefinitelyVariantExpr(Expr& expr, bool* isArrOut = nullptr) const;

    // Fix 038b: 基于 C 表达式字符串的 Variant 检测 — 补充 isDefinitelyVariantExpr
    // 的 AST 级检测. 当 codegen 生成的 C 表达式包含已知返回 vb6_VARIANT 的函数调用
    // (如 vb6_VariantArrayGet, vb6_VariantFromComResult 等) 时, 判定为 Variant.
    // 仅检查顶层表达式 (去除前导括号后), 避免对子表达式误判.
    bool cExprIsVariant(const std::string& cExpr) const;

    // Fix 038b: 运行时函数参数 C 类型查找表 — 当 calleeParams 为空 (运行时/内置函数)
    // 时, 通过函数名和参数索引查找期望的 C 类型. 返回空字符串表示未知.
    // 用于在函数调用参数生成时插入类型转换 (VARIANT→具体, 具体→VARIANT).
    static std::string getRuntimeParamCType(const std::string& funcName, size_t paramIdx);

    // ---- AST辅助 ----
    // 检测语句列表中是否包含GoSubStmt
    bool hasGoSubInStmts(StmtList& stmts) const;
    // P12.3: 检测语句列表中是否包含OnErrorStmt
    bool hasOnErrorInStmts(StmtList& stmts) const;
    // P14.1.2: 检测语句列表中是否包含Resume/Resume Next
    bool hasResumeInStmts(StmtList& stmts) const;
    // Bug #1 fix (082h): 预扫描语句中的UBound/LBound(arr,N>1)收集ND数组名
    void scanNDArraysInStmts(StmtList& stmts);
    void scanNDArraysInExpr(Expr& expr);

    // Fix 086: 局部声明过程级作用域提升
    // VB6 的 Dim/Const 是过程级作用域 (块内 Dim 在块外仍可见), 而 C 块作用域
    // 会造成 "分支内 Dim, 另一分支使用" 时 C2065 未声明标识符. 过程体发射前
    // 预扫描收集所有可提升的 LocalDeclStmt (非数组标量/动态数组指针/Const),
    // 在函数序言处统一声明, 原位置的 visit 跳过.
    void hoistLocalDecls(StmtList& body);
    void collectLocalDeclStmts(StmtList& stmts, std::vector<LocalDeclStmt*>& out);
    // 已提升的声明节点集合 (按指针, 每过程清空)
    std::set<const ASTNode*> hoistedLocalDeclSet_;
    // Fix 086: 工程内窗体模块名集合 (小写), driver预扫描填充
    std::unordered_set<std::string> knownFormModuleNames_;
    // Fix 086: For方向拆分的循环体副本索引 (0=首份, 1=第二份); Label/GoTo配对加后缀
    int labelCopyIdx_ = 0;
    // Fix 090o: For方向拆分期间各被拆 body 内定义的标签名栈 (嵌套层)。
    // GoTo 目标若定义在(某个被拆的) For body 内 → 第二份副本 goto 须加 _dN 后缀与
    // 副本内 LabelStmt 配对; 若目标在 body 外 (函数级出口标签如 QH/EH/ErrHandler,
    // VB6: GoTo 跳出循环到过程尾) 只在过程尾定义一份 → 盲目加后缀会 C2094 标签未定义。
    std::vector<std::unordered_set<std::string>> forSplitLabelStack_;
    void collectForBodyLabels(const StmtList& stmts, std::unordered_set<std::string>& out);
    // Fix 090m/090p: ReDim/Erase 目标是 Variant 数组判定 (顶层 As Variant 变量 /
    // UDT 的 As Variant 字段如 (*uFile).BufferArray)
    bool isVariantArrayTarget(const std::string& name);
    // Fix 086: 各模块 Public 常量表 (driver预扫描填充, 本体归driver所有)
    const std::unordered_map<std::string,
        std::unordered_map<std::string, long long>>* modulePublicConsts_ = nullptr;

    // Fix 093b: 跨模块同名模块级变量集合(小写), 本体归 driver 所有
    const std::unordered_set<std::string>* staticModuleVarNames_ = nullptr;

    // Fix 093b: WithEvents 事件包装函数名 (带模块前缀).
    // 不同模块里的同名 WithEvents 变量 (如各处的 m_ListenSocket / m_socket) 会生成
    // 同名包装函数 → LNK2005; 包装函数只在本模块内使用 (sink 是 Set 处的函数内
    // static 局部变量), 带模块前缀即可彻底消除撞名, 且不改变链接属性.
    std::string evtWrapperName(const std::string& varLower, const std::string& evtName) const {
        return "vb6_evt_wrap_" + cIdent(moduleName_) + "_" + varLower + "_" + cIdent(evtName);
    }
    // Fix 093b: COM 源接口事件包装函数名 (带模块前缀, 同上)
    std::string comEvtWrapperName(const std::string& varLower, const std::string& evtName) const {
        return "vb6_com_evt_" + cIdent(moduleName_) + "_" + varLower + "_" + cIdent(evtName);
    }

    // ---- COM辅助 (P6.2) ----
    // 推断COM参数的封装函数: 根据表达式类型选择vb6_ComPackBSTR/Int/Double/Object
    std::string comPackExpr(Expr& expr);

    // Fix 177b: 创建 coclass 实例的表达式 (New / Dim As New 自动实例化共用).
    // 默认 vb6_NewObject(L"<ProgID>") — 运行期按 ProgID 走 CoCreateInstance.
    // 但若该 coclass 被本工程的同名类模块遮蔽 (Symbol::comProjectImplClass 非空,
    // 如 VBMAN.vbp 的 Class=Dictionary vs Microsoft Scripting Runtime 的
    // Scripting.Dictionary), ProgID 仍解析到引用库的实现 → 拿到的对象缺少工程类
    // 自有成员 → DISP_E_MEMBERNOTFOUND. 此时改为直接创建工程类实例, 再经
    // vb6_ComPackVB6InstanceRaw 包装成真 IDispatch (与 ExeComBridge 03 同一通路).
    std::string comNewExprFor(const Symbol* comSym);

    // P25: 解析COM标记为类型化属性取值, 用于COM调用参数打包
    // 当isComMarker_为true时调用, 根据packFnHint选择ComGetObjectProp/GetIntProp/GetStringProp等
    // packFnHint: comPackExpr返回的封装函数名, 用于推断所需属性类型
    std::string resolveComMarkerForPack(const std::string& packFnHint);

    // 解析COM标记为C值表达式 (属性读取语义)
    // 当isComMarker_为true时调用, 生成vb6_ComGetProp+Unpack, 并清除标记
    // unresolvedType: 期望的解封类型, 默认为BSTR (最通用)
    std::string resolveComValue(const std::string& unresolvedType = "BSTR");

    // Fix 092m: With 块类字段写的 COM 解包 hint — 按目标类字段的 VB 类型名
    // (Symbol::memberFieldTypes, 语义分析阶段填充 + driver 跨模块拷贝) 映射到
    // resolveComValue 期望的解封类型:
    //   String → "BSTR" / Long|Integer|Boolean|Byte → "Long" /
    //   Single|Double|Date|Currency → "Double" / Object → "Object" /
    //   Variant → "Variant" / 项目类|UDT → "Object" / 未登记 → "BSTR"(默认).
    std::string classFieldComUnpackHint(const std::string& className,
                                        const std::string& memberName) const;

    // Fix 092p: 类数据字段名规范化 — VB6 大小写不敏感, 源码 `.socket` 与声明
    // `Socket` 是同一字段, 但 C 结构体成员名按声明原样生成. 访问点先经本函数换回
    // 声明名 (Symbol::memberFieldNames), 避免 `me->Client->socket` C2039.
    // 表中无此项 (方法/属性/非本类成员/未知类) 时原样返回.
    std::string canonicalClassFieldName(const std::string& className,
                                        const std::string& memberName) const;

    // Fix 093a: 类成员(方法/属性)名规范化 — VB6 大小写不敏感, 调用点拼写 (count)
    // 可能与类内声明 (Count) 不同; C 符号大小写敏感 → 生成的 vb6_<cls>_prop_get_<m>
    // 与类定义不一致 → LNK2019. 以 Class 符号 memberNames 的声明拼写为准; 无此项
    // (非本类成员/未知类) 时原样返回.
    std::string canonicalClassMemberName(const std::string& className,
                                         const std::string& memberName) const;

    // Fix 093a: 当前类是否声明了同名成员字段 (裸标识符赋值时字段优先于外部同名
    // Property Let/Set).
    bool isOwnClassField(const std::string& memberName) const;

    // Fix 010r-16: COM/Property-Get 左值重写辅助
    // 当赋值语句(Let/Set/Assignment fallback)的 LHS 是非常量 C 表达式(非左值)时,
    // 尝试重写为 COM SetProp/SetPropArg 或 Property Let/Set 调用.
    // 参数:
    //   target  - LHS 的 C 表达式(已被 emitExpr 产生)
    //   value   - RHS 的 C 表达式(已被 emitExpr 产生, 已完成 COM 解封)
    //   valueExpr - RHS 的 AST 节点(用于 comPackExpr 推断封装函数)
    //   isSet   - true 表示 Set 语句(对象引用语义), false 表示 Let/Assignment
    // 返回值: 若成功重写并已 emit, 返回 true; 否则返回 false(让调用者继续 fallback)
    bool tryRewriteCOMLvalue(const std::string& target, const std::string& value,
                             Expr* valueExpr, bool isSet);

    // Fix 090w/090x: Property Let/Set 值实参打包 — Let 末形参 As Variant 时,
    // Pattern C/D2 (prop_get_ LHS rewrite) 与 With 块 prop_let_ 调用追加的值实参
    // (double/int/BSTR 等标量 C 表达式) 需按方向转换:
    //   - ByVal Variant  → vb6_VariantFromValue(v)
    //   - ByRef Variant  → 可寻址字段式复合字面量 (&(vb6_VARIANT){.vt=..., .xxx=v})
    //     (按 valueExpr 推断的 VB 类型选字段; 与 cgen_expr M22 ByRef Variant 分支一致)
    // 非法形态 (&(vb6_VARIANT){vb6_VariantFromValue(v)}) 会触发 C2440
    // "vb6_VARIANT→vb6_vartype". lastP.type 非 Variant 时原样返回.
    std::string packLetValueArg(const ParameterInfo& lastP, Expr* valueExpr,
                                const std::string& val) const;

    // Fix 090ae: 链式 COM 默认属性索引赋值 (P25b 提取为可复用 helper)
    // VB: dic(a)(b) = v / Set dic(a)(b) = v — 多层默认属性/Item 索引写.
    // AST 形态: 多层 IndexOrCallExpr 嵌套, 最内层 callee 为 COM 变量/类 void*
    // COM 字段 (IdentifierExpr) 或模块默认成员 PropertyGet. 中间层对象用
    // vb6_ComCallObject 逐层解包, 最外层走 vb6_ComSetPropArg.
    // AssignmentStmt/SetStmt 均调用 (090ae: SetStmt 原先缺失该分支 → LHS 被
    // emitExpr 生成值语义 vb6_VariantFromComResult(...) = ... → C2440).
    // 返回值: 若已识别并 emit, 返回 true; 否则 false (让调用者继续 fallback).
    bool tryEmitChainedComWrite(Expr* targetNode, Expr* valueNode);

    // Fix 011r-1: 类实例成员调用解析辅助
    // 给定类名与成员名, 在当前作用域的模块级符号表中查找属于该类的方法/属性符号,
    // 返回构造的 C 函数名:
    //   - Sub/Function     → vb6_<className>_<memberName>
    //   - Property Get     → vb6_<className>_prop_get_<memberName>  (读上下文优先)
    //   - Property Let     → vb6_<className>_prop_let_<memberName>  (仅当没有 Get 时返回)
    //   - Property Set     → vb6_<className>_prop_set_<memberName>  (仅当没有 Get/Let 时返回)
    // 匹配条件 (跨模块): sym->isExternal && sym->sourceModule == className
    // 匹配条件 (同模块类): !sym->isExternal && isClassModule_ && moduleName_ == className  (Fix 013: moduleName_ = VB_Name)
    // 返回空串表示该类中无对应方法/属性, 调用者应视为数据字段访问 (obj->member)
    std::string resolveClassMemberCall(const std::string& className,
                                       const std::string& memberName) const;

    // Fix 033: 类感知方法/属性参数查找 (类符号 + memberParams 表的精确解析).
    // IndexOrCallExpr calleeParams 解析: 按 className 查找指定类的方法/属性参数表,
    // 避免跨模块 storageKey 冲突命中的首个注册者导致 Optional _has_ 标志数错 (→ C2197).
    // Phase A: 与 resolveClassMemberCall 同迭代规则 (匹配 sourceModule 或 同模块
    //          moduleName_), 在 moduleScope 找到真实 Sub/Function/Property 符号,
    //          返回其 params + isBuiltin. 命中即返回 true.
    // Phase B: Phase A 失败时 (storageKey 冲突让消费模块的 external Create 符号的
    //          sourceModule 不匹配 className), 退到 Class 符号自身的 memberParams[lower]
    //          (语义分析阶段已按 Get > Function > Sub > Let > Set 优先级填充), 返回该
    //          参数表 (isBuiltin=false). 命中即返回 true.
    // 都未命中返回 false, 调用者回退到 class-unaware lookupModule.
    bool findClassMemberCallParams(const std::string& className,
                                   const std::string& memberName,
                                   std::vector<ParameterInfo>& outParams,
                                   bool& outIsBuiltin) const;

    // ---- Fix 091a: 属性写方向 (Property Let/Set) 参数查找 ----
    // Pattern C/D2 (prop_get_ LHS → prop_let_/prop_set_) 与 With 块属性写需要
    // **写方向** 参数表来判定末参是否 As Variant (值实参打包). 读方向优先级的
    // memberParams 对「Get 有参 + Let 末参才是 value」的属性 (cJson.Item(key) /
    // Let Item(key, Dat As Variant)) 会取到 Get 的 [key] → 不打包 → C2440.
    // Phase A: 模块作用域中 sourceModule 匹配 className 的 PropertyLet/PropertySet
    //          符号 (跨模块 storageKey 冲突致其缺失时转 Phase B).
    // Phase B: Class 符号自身的 memberLetParams/memberSetParams (driver.cpp 已跨
    //          模块拷贝; 同类内 Let/Set 各自唯一, 不受 Get 优先级遮蔽).
    bool findClassMemberWriteParams(const std::string& className,
                                    const std::string& memberName,
                                    bool isSet,
                                    std::vector<ParameterInfo>& outParams) const;

    // ---- Fix 015: Method chaining 解析辅助 ----
    // 给定一个表达式 AST 节点, 推断其在运行时返回的类名 (如果它返回类实例)
    //  - IdentifierExpr: 查 knownClassVars_, 找到则返回该变量的声明类名
    //  - IndexOrCallExpr: 递归推断 callee.object 的类名, 再用 getClassMethodReturnType 找方法的返回类名
    //  - 其他: 返回空串 (不可推断为类实例)
    // 用于链式调用 db.Sql(s).Exec(...) 中 .Exec 的对象表达式 (db.Sql(s)) 类型推断
    std::string inferClassTypeOfExpr(const ASTNode& expr) const;

    // ---- Fix 037: UDT 类型推断辅助 ----
    // 给定一个表达式 AST 节点, 递归推断其 UDT C 类型标识符 (如 "vb6_type_UcsBuffer").
    //  - IdentifierExpr: 查 knownUdtVars_ 得到 UDT C 类型
    //  - MemberAccessExpr: 递归推断 object 的 UDT 类型, 再在 udtMembers 中找
    //    memberName 对应成员. 若该成员本身是 UDT (typeRefName 非空且符号表中有
    //    UserDefinedType), 返回 "vb6_type_<memberUdtName>"; 否则返回空串.
    //    支持嵌套 UDT: _vb6_with_3.DecrBuffer → 推断 DecrBuffer 的 UDT 类型.
    //  - 其他: 返回空串
    // 用于 IndexOrCallExpr 中 obj.udtArrayField(idx) 模式识别, 避免把 UDT 数组
    // 字段误当函数调用 (C2064).
    std::string inferUdtTypeOfExpr(const ASTNode& expr) const;

    // ---- Fix 085: UDT 对象字段类型推断 ----
    // 给定 UDT C 类型标识符 (如 "vb6_type_tZipFileItem") 与字段名(小写), 返回该字段
    // 的对象类别 C 类型:
    //   - ""                          → 非对象字段 (标量/字符串/数组等)
    //   - "vb6_type_Y"                → 嵌套 UDT 字段 (非对象, 供链式推断)
    //   - "vb6_cls_X*"                → 项目类对象字段 (X=类名, 走类方法调用)
    //   - "void*"                     → Collection/COM/接口 对象字段 (走 COM dispatch)
    // 依据语义阶段 TypeDecl 在 udtMembers 中登记的 type/typeRefName.
    std::string udtFieldObjCType(const std::string& udtCType,
                                 const std::string& memberLower) const;

    // Fix 085: 在 UDT 字段访问拼接处追加对象字段注释标记 (供外层 MemberAccessExpr
    // 消费): obj.field 追加 "  /* udt objfield <CType> */"; 非对象字段原样返回.
    // objExpr 为已生成的左值表达式, udtCType/member 用于判定字段类别.
    std::string appendUdtObjFieldMarker(const std::string& objExpr,
                                        const std::string& udtCType,
                                        const std::string& member,
                                        const std::string& accessOp = ".") const;

    // Fix 084n: 推断 target 是否为 UDT 字段链, 是则返回该字段的 Vb6Type (含 Array 标志), 否则 Unknown.
    // 供赋值语句判断 Variant RHS 需转换的目标类型 (如 cZipArchive 的 .FileName As String
    // ← vb6_VariantArrayGet → vb6_VariantToString; uBuf.MaxMatch As Long ← At() → vb6_VariantToLong)
    Vb6Type inferUdtFieldVb6Type(const ASTNode* target) const;

    // Fix 084o: 若 C 表达式为 Variant (字符串级检测或已知 Variant 变量), 返回
    // vb6_VariantToLong(...) 包装, 否则原样返回. 用于需要 int32_t 的上下文:
    // 整除运算符 \ 的操作数、vb6_VariantArrayGet/GetVal 的索引、For 循环 Variant
    // 控制变量的比较/步进等. astExpr 可为 nullptr.
    std::string toLongIfVariant(const std::string& cExpr, const Expr* astExpr);

    // 给定类名与成员名, 在当前模块作用域符号表中查找属于该类的 Function/PropertyGet 符号,
    // 若其返回类型为 Object 且记录了 variableTypeName (Fix 015), 返回经 canonicalClassName
    // 规范化后的类名; 否则返回空串. 用于推断方法返回值的类类型.
    std::string getClassMethodReturnType(const std::string& className,
                                         const std::string& memberName) const;

    // 将可能存在大小写差异的类型名 (来自源码 variableTypeName) 规范化为符号表中
    // Class 符号记录的标准名称 (clsSym->name 或 sourceModule), 与 struct 定义
    // vb6_cls_<canonicalName> 大小写一致. 找不到时原样返回.
    std::string canonicalClassName(const std::string& typeName) const;

    // ---- 数组辅助 ----
    // VB6类型 → SAFEARRAY元素类型C枚举名
    std::string mapSaElemType(Vb6Type type) const;
    // VB6类型 → SAFEARRAY元素C类型 (如int32_t)
    std::string mapSaElemCType(Vb6Type type) const;
    // 从ArrayTypeRef或asType获取元素Vb6Type
    Vb6Type resolveArrayElemType(ASTNode* typeRef) const;
    // Fix 055: 从ArrayTypeRef获取UDT元素C类型名 (如"vb6_type_RECT"), 非UDT返回空串
    std::string resolveArrayUdtElemCType(ASTNode* typeRef) const;

    // Fix 100: ReDim complex target (member chain with subscript)
    void emitReDimComplexTarget(ReDimStmt& node);
    // Fix 100: resolve element type of a complex ReDim target
    void resolveReDimComplexElemType(const ReDimStmt& node, Vb6Type& outType,
                                     std::string& outUdtCType) const;
    // P6.4+: class-module default instance singleton
    void emitClassDefaultInstance(Module& module);
    // Fix 110: emit symbol support for .ctl/.pag designer sub-controls
    void emitDesignerControlDecls(const FrmFormDesc& frmDesc);
    // Fix 110: recursively emit designer control handle/array/COM declarations
    void emitControlHandleDecls(const FrmFormDesc& frmDesc);
    // Fix 107: lookup type-category symbol (Class/UDT/Enum/COM), ignoring proc/var
    Symbol* lookupTypeSymbol(const std::string& name) const;
    // Fix 112: find .ctl designer spec by control type name
    const FrmFile* findUserControlSpec(const std::string& controlTypeName) const;
    // Fix 092w: rewrite RHS assigned to a Byte array into byte-array semantics
    std::string rewriteByteArrayValue(const std::string& value) const;
