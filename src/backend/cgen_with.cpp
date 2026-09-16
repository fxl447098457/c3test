#include "backend/cgen.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <functional>

namespace vb6c3 {

// --- cgen_with.cpp: With 块语句生成 ---

void CCodeGen::visit(WithStmt& node) {
    // P17.1: With块 — 根据对象类型创建适当类型的临时变量
    WithObjInfo withInfo;
    std::string tempVar = "_vb6_with_" + std::to_string(tempCounter_++);
    std::string tempType = "void*";

    // 检测With对象类型: 遍历表达式判断
    if (node.object) {
        // --- Fix 086: 方法调用链对象推断 (With .Sql(...) / With obj.Method(...)) ---
        // With 对象是 IndexOrCallExpr/MemberAccessExpr/WithMemberExpr (方法调用链) 时,
        // 用 inferClassTypeOfExpr 推断返回类. 此前 `With .Sql(...)` 解析为
        // IndexOrCallExpr (非裸 WithMemberExpr), 嵌套With分支不命中 → tempType void*
        // → 成员解析回退全局 lookupModule 捡错符号 (如 .Fetch 命中模块级 Sub Fetch,
        // 生成 vb6_Demo_Fetch(_vb6_with_N) 这类带多余参数的非法调用).
        // 仅接受项目 Class (排除 COM 类/接口/UDT), 其余情况走原有分支.
        if (node.object->kind == ASTNodeKind::IndexOrCallExpr
            || node.object->kind == ASTNodeKind::MemberAccessExpr
            || node.object->kind == ASTNodeKind::WithMemberExpr) {
            std::string inferred = inferClassTypeOfExpr(*node.object);
            if (!inferred.empty()) {
                const Symbol* clsSym = lookupModuleDotted(inferred);
                if (clsSym && clsSym->kind == SymbolKind::Class && !clsSym->isInterface) {
                    withInfo.kind = WithObjKind::ClassInstance;
                    withInfo.className = cIdent(inferred);
                    tempType = "vb6_cls_" + withInfo.className + "*";
                    withInfo.ctrlOrigName = withInfo.className;
                }
            }
        }
        // --- Fix 010n: New表达式检测 (With New ClassName) ---
        if (node.object->kind == ASTNodeKind::NewExpr) {
            auto& newExpr = static_cast<NewExpr&>(*node.object);
            std::string clsLower = newExpr.className;
            std::transform(clsLower.begin(), clsLower.end(), clsLower.begin(), ::tolower);
            // Fix 086: COM类检测 — With New ADODB.Stream 等 COM 类型没有项目
            // Class 符号 (ComClass/ComInterface 或未注册), 不能按项目类生成
            // vb6_cls_<X>* 临时 (类型未定义 → C2065). 转 COMObject 走后期绑定.
            const Symbol* newClsSym = lookupModuleDotted(newExpr.className);
            if (!newClsSym || newClsSym->kind == SymbolKind::ComClass
                || newClsSym->kind == SymbolKind::ComInterface) {
                withInfo.kind = WithObjKind::COMObject;
                tempType = "void*";
                withInfo.ctrlOrigName = cIdent(newExpr.className);
            } else {
                // "With New X" 总是类实例
                withInfo.kind = WithObjKind::ClassInstance;
                withInfo.className = cIdent(newExpr.className);  // Fix 011r-1
                tempType = "vb6_cls_" + withInfo.className + "*";
                withInfo.ctrlOrigName = withInfo.className;
            }
        }
        // --- Fix 010n: WithMemberExpr (嵌套With: With .Method()) ---
        else if (node.object->kind == ASTNodeKind::WithMemberExpr) {
            // 嵌套With: .Member() — 继承外层With的对象类型
            if (!withObjectInfoStack_.empty()) {
                const auto& outerInfo = withObjectInfoStack_.back();
                if (outerInfo.kind == WithObjKind::ClassInstance ||
                    outerInfo.kind == WithObjKind::COMObject) {
                    // 方法返回值通常是同类或另一个类实例
                    withInfo.kind = WithObjKind::ClassInstance;
                    // Fix 011r-1: 继承外层className (对property chain如 .SubObj.SubMethod常见)
                    // 注意: 如外层方法返回不同类实例, 此继承会错误 — 当前简化处理
                    withInfo.className = outerInfo.className;
                    if (!withInfo.className.empty()) {
                        tempType = "vb6_cls_" + withInfo.className + "*";
                    }
                } else if (outerInfo.kind == WithObjKind::FormControl ||
                           outerInfo.kind == WithObjKind::WithEventsCtrl) {
                    // 控件属性返回的对象 → 类实例
                    withInfo.kind = WithObjKind::ClassInstance;
                    // 控件方法返回的对象类型未知, 不设置className
                }
                // UDT/Unknown/BuiltinObject: 保持Unknown (struct.field访问)
            }
        }

        // --- IdentifierExpr: 标识符With对象 ---
        std::string objNameLower;
        if (node.object->kind == ASTNodeKind::IdentifierExpr) {
            auto& idExpr = static_cast<IdentifierExpr&>(*node.object);
            objNameLower = idExpr.name;
            std::transform(objNameLower.begin(), objNameLower.end(), objNameLower.begin(), ::tolower);
        }

        if (!objNameLower.empty()) {
            // P16: WithEvents控件 (优先于普通控件)
            auto itWE = knownWithEventsCtrlVars_.find(objNameLower);
            if (itWE != knownWithEventsCtrlVars_.end()) {
                withInfo.kind = WithObjKind::WithEventsCtrl;
                withInfo.ctrlType = itWE->second;
                auto itOrig = knownWithEventsCtrlOrigNames_.find(objNameLower);
                withInfo.ctrlOrigName = (itOrig != knownWithEventsCtrlOrigNames_.end())
                    ? "vb6_hwnd_" + cIdent(itOrig->second) : "vb6_hwnd_" + cIdent(objNameLower);
                tempType = "HWND";
            } else {
                // 窗体控件
                auto itCtrl = knownFormControls_.find(objNameLower);
                if (itCtrl != knownFormControls_.end()) {
                    withInfo.kind = WithObjKind::FormControl;
                    withInfo.ctrlType = itCtrl->second;
                    if (itCtrl->second == FrmControlType::Menu) {  // P20-36: Menu stores lowercase name for makeCtrlHwndArg
                        withInfo.ctrlOrigName = objNameLower;
                    } else {
                        withInfo.ctrlOrigName = "vb6_hwnd_" + cIdent(objNameLower);
                    }
                    tempType = "HWND";
                }
            }

            // COM对象变量检测
            if (withInfo.kind == WithObjKind::Unknown) {
                if (knownObjectVars_.count(objNameLower)) {
                    withInfo.kind = WithObjKind::COMObject;
                }
            }

            // Fix 043b: Early-bound COM variable detection (Dim x As Dictionary, etc.)
            // knownObjectVars_ only contains void* (late-binding) variables.
            // Early-bound COM variables (vb6_ComIface_IDictionary*, etc.) are in
            // knownTypedComVars_. Without this check, With blocks on early-bound
            // COM locals fall through as Unknown, causing .Add to be resolved as
            // the class's own method instead of COM dispatch (C2198).
            if (withInfo.kind == WithObjKind::Unknown) {
                if (knownTypedComVars_.count(objNameLower)) {
                    withInfo.kind = WithObjKind::COMObject;
                }
            }

            // 类实例变量检测
            if (withInfo.kind == WithObjKind::Unknown) {
                auto itClassVar = knownClassVars_.find(objNameLower);
                if (itClassVar != knownClassVars_.end()) {
                    withInfo.kind = WithObjKind::ClassInstance;
                    // Fix 011r-1: 设置className, 让WithMemberExpr能精确解析该类方法
                    withInfo.className = cIdent(itClassVar->second);
                    tempType = "vb6_cls_" + withInfo.className + "*";
                }
            }

            // Fix 043b: Class field detection — knownObjectVars_ and knownClassVars_
            // only contain local variables/parameters, NOT class fields. When a With
            // block targets a class field (e.g., `With Dic` where Dic is `Dim Dic As
            // Dictionary`), we need to check classVoidFieldMap_ (for void*/COM fields)
            // and classTypedFieldMap_ (for typed class fields) to determine the correct
            // WithObjKind. Without this, COM fields like Dictionary fall through as
            // Unknown, and .Add inside the With block gets resolved to the class's own
            // Add method instead of COM dispatch (C2198).
            if (withInfo.kind == WithObjKind::Unknown && isClassModule_) {
                // Check void* (COM) fields first
                if (classVoidFieldMap_) {
                    auto itV = classVoidFieldMap_->find(moduleName_);
                    if (itV != classVoidFieldMap_->end() && itV->second.count(objNameLower)) {
                        withInfo.kind = WithObjKind::COMObject;
                    }
                }
                // Check typed class fields (project class or COM interface)
                if (withInfo.kind == WithObjKind::Unknown && classTypedFieldMap_) {
                    auto itT = classTypedFieldMap_->find(moduleName_);
                    if (itT != classTypedFieldMap_->end()) {
                        auto itField = itT->second.find(objNameLower);
                        if (itField != itT->second.end()) {
                            const std::string& typeName = itField->second;
                            if (typeName.rfind("COM:", 0) == 0) {
                                // COM interface field (e.g., "COM:Dictionary")
                                withInfo.kind = WithObjKind::COMObject;
                            } else {
                                // Project class field (e.g., "cAsyncSocket")
                                withInfo.kind = WithObjKind::ClassInstance;
                                withInfo.className = cIdent(typeName);
                                tempType = "vb6_cls_" + withInfo.className + "*";
                            }
                        }
                    }
                }
            }

            // Fix 010n: UDT变量检测 → 设置正确的struct类型 (而非void*)
            // 这样 struct.field 访问才能通过编译 (C2224修复)
            if (withInfo.kind == WithObjKind::Unknown) {
                auto itUdt = knownUdtVars_.find(objNameLower);
                if (itUdt != knownUdtVars_.end()) {
                    tempType = itUdt->second;  // e.g., "vb6_type_OPENFILENAME"
                    // Keep Unknown kind — struct.field 访问对UDT是正确的
                    // Fix 037: 注册 With 临时变量到 knownUdtVars_, 让嵌套 UDT 字段
                    // 访问 (如 _vb6_with_N.DecrBuffer.Data(0)) 能推断出 UDT 类型,
                    // 正确生成 VB6_SA_AT 而非误当函数调用 (C2064).
                    knownUdtVars_[tempVar] = itUdt->second;
                    knownLocalVars_.insert(tempVar);
                }
            }

            // Fix 054: With目标为当前函数UDT返回值 (如 With QRCodegenMakeBytes → vb6_ret_QRCodegenMakeBytes)
            // 函数返回变量不在 knownUdtVars_ 中, 但返回类型可能是UDT
            if (withInfo.kind == WithObjKind::Unknown && !currentReturnVar_.empty()) {
                std::string retLower = currentReturnVar_;
                std::transform(retLower.begin(), retLower.end(), retLower.begin(), ::tolower);
                if (retLower.find(objNameLower) != std::string::npos) {
                    // objName matches the return variable → check return type
                    if (currentReturnCType_.rfind("vb6_type_", 0) == 0) {
                        tempType = currentReturnCType_;
                        knownUdtVars_[tempVar] = currentReturnCType_;
                        knownLocalVars_.insert(tempVar);
                    }
                }
            }

            // Fix 010l: 内置全局对象检测 (Err/App/Screen/Printer/Clipboard/Debug)
            if (withInfo.kind == WithObjKind::Unknown) {
                if (objNameLower == "err" || objNameLower == "app" ||
                    objNameLower == "screen" || objNameLower == "printer" ||
                    objNameLower == "clipboard" || objNameLower == "debug") {
                    withInfo.kind = WithObjKind::BuiltinObject;
                    withInfo.ctrlOrigName = objNameLower;  // store lowercase name
                }
            }
        }

        // --- Fix 010n: MemberAccessExpr (With obj.member / With me.member) ---
        if (node.object->kind == ASTNodeKind::MemberAccessExpr && withInfo.kind == WithObjKind::Unknown) {
            auto& memExpr = static_cast<MemberAccessExpr&>(*node.object);
            std::string memberLower = memExpr.memberName;
            std::transform(memberLower.begin(), memberLower.end(), memberLower.begin(), ::tolower);

            // Fix 090s: MAE 目标是「宿主类的字段」时按宿主类查字段表 — With
            // Http.RequestDataQuery (Http As cHttpClient, RequestDataQuery As New
            // Dictionary = COM void* 字段): 此前成员不在 knownClassVars_ 后走
            // memSym 全局查找, 捡到 Dictionary 符号按 ClassInstance +
            // vb6_cls_Dictionary* 处理 → .Item("k")=v 生成结构体字段调用 C2039.
            // 正确按宿主类 classVoidFieldMap_/classTypedFieldMap_ 分类: void*/
            // COM: → WithObjKind::COMObject (COM dispatch), 项目类 → ClassInstance.
            std::string hostCls90s = memExpr.object ? inferClassTypeOfExpr(*memExpr.object) : "";
            if (!hostCls90s.empty()) {
                if (classVoidFieldMap_) {
                    auto itV90s = classVoidFieldMap_->find(hostCls90s);
                    if (itV90s != classVoidFieldMap_->end() && itV90s->second.count(memberLower)) {
                        withInfo.kind = WithObjKind::COMObject;
                        withInfo.ctrlOrigName = hostCls90s;
                    }
                }
                if (withInfo.kind == WithObjKind::Unknown && classTypedFieldMap_) {
                    auto itT90s = classTypedFieldMap_->find(hostCls90s);
                    if (itT90s != classTypedFieldMap_->end()) {
                        auto itF90s = itT90s->second.find(memberLower);
                        if (itF90s != itT90s->second.end()) {
                            if (itF90s->second.rfind("COM:", 0) == 0) {
                                withInfo.kind = WithObjKind::COMObject;
                                withInfo.ctrlOrigName = hostCls90s;
                            } else {
                                withInfo.kind = WithObjKind::ClassInstance;
                                withInfo.className = cIdent(itF90s->second);
                                tempType = "vb6_cls_" + withInfo.className + "*";
                                withInfo.ctrlOrigName = withInfo.className;
                            }
                        }
                    }
                }
            }

            // 检查成员是否为类实例变量 (me.member As SomeClass)
            if (knownClassVars_.find(memberLower) != knownClassVars_.end()) {
                withInfo.kind = WithObjKind::ClassInstance;
                // Fix 011r-1: 获取成员的类名, 设置tempType
                auto itClassVar = knownClassVars_.find(memberLower);
                if (itClassVar != knownClassVars_.end()) {
                    withInfo.className = cIdent(itClassVar->second);
                    tempType = "vb6_cls_" + withInfo.className + "*";
                }
            } else if (knownObjectVars_.count(memberLower)) {
                withInfo.kind = WithObjKind::COMObject;
            } else if (knownUdtVars_.count(memberLower)) {
                // UDT成员: 使用struct类型 (如 With ofn → vb6_type_OPENFILENAME)
                auto itUdt = knownUdtVars_.find(memberLower);
                if (itUdt != knownUdtVars_.end()) {
                    tempType = itUdt->second;
                    // Fix 037: 注册 With 临时变量到 knownUdtVars_
                    knownUdtVars_[tempVar] = itUdt->second;
                    knownLocalVars_.insert(tempVar);
                }
            } else if (withInfo.kind == WithObjKind::Unknown) {
                // Fix 090s: kind 已被上面 hostCls90s 字段表解析 (COMObject/ClassInstance)
                // 时不再走 memSym 兜底 — 否则 With Http.RequestDataQuery (RequestDataQuery
                // As New Dictionary = COM void* 字段, A1 已置 COMObject) 被
                // lookup("RequestDataQuery") 捡到 Dictionary 符号 → 覆盖成 ClassInstance
                // + vb6_cls_Dictionary* → .Item(k)=v 生成结构体字段调用 C2039.
                // 尝试从符号表推断类型
                Symbol* memSym = symTab_.lookupModule(memExpr.memberName);
                if (!memSym) memSym = symTab_.lookup(memExpr.memberName);
                if (memSym) {
                    if (memSym->type == Vb6Type::UserDefinedType) {
                        // 查找UDT类型的C标识符
                        // TODO: Symbol没有存储typeRefName, 需要其他方式
                    } else if (memSym->type == Vb6Type::Object) {
                        withInfo.kind = WithObjKind::ClassInstance;
                        // Fix 011r-1: 若Symbol有variableTypeName, 用之; 否则className未知
                        if (!memSym->variableTypeName.empty()) {
                            withInfo.className = cIdent(memSym->variableTypeName);
                            tempType = "vb6_cls_" + withInfo.className + "*";
                        }
                    }
                }
                // 无法确定类型时默认为类实例 (void*不支持.member访问)
                if (withInfo.kind == WithObjKind::Unknown && tempType == "void*") {
                    withInfo.kind = WithObjKind::ClassInstance;
                }
            }
        }

        // --- Fix 010n: IndexOrCallExpr (With arr(idx) / With func()) ---
        if (node.object->kind == ASTNodeKind::IndexOrCallExpr && withInfo.kind == WithObjKind::Unknown) {
            // 数组元素或函数返回值 — 通常是类实例或VARIANT
            // 数组元素访问如 m_uWindowState(0) → VARIANT UDT
            auto& callExpr = static_cast<IndexOrCallExpr&>(*node.object);
            if (callExpr.callee && callExpr.callee->kind == ASTNodeKind::IdentifierExpr) {
                auto& idExpr = static_cast<IdentifierExpr&>(*callExpr.callee);
                std::string arrLower = idExpr.name;
                std::transform(arrLower.begin(), arrLower.end(), arrLower.begin(), ::tolower);
                // Fix 055: 优先检查UDT数组元素类型
                auto itUdtArr = arrayUdtElemTypes_.find(arrLower);
                if (itUdtArr != arrayUdtElemTypes_.end()) {
                    tempType = itUdtArr->second;  // e.g. "vb6_type_RECT"
                    // 注册到knownUdtVars_, 让后续字段访问正确
                    knownUdtVars_[tempVar] = itUdtArr->second;
                    knownLocalVars_.insert(tempVar);
                } else {
                    // 检查是否为已知数组 → 元素类型
                    auto itArr = arrayElemTypes_.find(arrLower);
                    if (itArr != arrayElemTypes_.end()) {
                        if (itArr->second == Vb6Type::UserDefinedType) {
                            tempType = "vb6_VARIANT";  // UDT数组元素存储为VARIANT (fallback, should be caught above)
                        } else if (itArr->second == Vb6Type::Variant || itArr->second == Vb6Type::Object) {
                            tempType = "vb6_VARIANT";
                        }
                    }
                }
            }
            // 函数返回值且仍为void* → 默认按 COM 后期绑定分发
            if (withInfo.kind == WithObjKind::Unknown && tempType == "void*") {
                // Fix 090y: void* With 目标 (COM 方法返回对象, 如 cIni.Section As
                // Dictionary) → COMObject (后期绑定 dispatch). 此前 ClassInstance
                // (className 空) → WithMemberExpr 成员解析落入全局符号表撞名
                // (cTimers.Item) → 左值错误 C2106. UDT 目标 tempType=vb6_type_*
                // 不受影响; Variant-对象目标运行时 dispatch 也更贴合 COM 语义.
                withInfo.kind = WithObjKind::COMObject;
            }
        }

        // --- 最终回退: void* 不支持 .member 访问 → COM 后期绑定分发 ---
        // Fix 090y: (同上方 Fix, 独立于 isClassModule_ 字段检测的最终兜底)
        // UDT struct (vb6_type_*) 目标 tempType 非 void* → 不受影响.
        if (withInfo.kind == WithObjKind::Unknown && tempType == "void*") {
            withInfo.kind = WithObjKind::COMObject;
        }
        // Fix 054: tempType 已解析为 UDT struct (vb6_type_*) → 保持 Unknown, 用 struct.field 访问
        if (withInfo.kind == WithObjKind::ClassInstance && tempType.rfind("vb6_type_", 0) == 0) {
            withInfo.kind = WithObjKind::Unknown;
        }
    }

    // Fix 092o: 本帧 withInfo 延后到目标表达式生成之后再入栈 — 若提前入栈, 嵌套
    // With 的目标表达式 (.ReturnJson()) 会以内层 className (cJson) 解析外层成员
    // (实为 cHttpClient 的 ReturnJson) → 解析失败退化为数据字段访问 (cAliyunCaptcha
    // 222: `_vb6_with_3->ReturnJson()` C2039 + 实参丢失). 见下方 emitExpr 之后的入栈.

    // Fix 010l: BuiltinObject 不需要临时变量 — 属性读写直接映射为RTL函数调用
    if (withInfo.kind == WithObjKind::BuiltinObject) {
        withObjectInfoStack_.push_back(withInfo);  // Fix 092o: BuiltinObject 无目标表达式, 直接入栈
        // 推入占位符以保持 withObjectVars_ 与 withObjectInfoStack_ 同步
        withObjectVars_.push_back(withInfo.ctrlOrigName);
        c_.emitLine("{");
        c_.indent();
        emitStmtList(node.body);
        c_.dedent();
        c_.emitLine("}");
        withObjectVars_.pop_back();
        withObjectInfoStack_.pop_back();
        return;
    }

    // P17.1: 抑制With对象表达式的默认属性解析
    bool prevSuppress = suppressDefaultProp_;
    if (withInfo.kind == WithObjKind::FormControl || withInfo.kind == WithObjKind::WithEventsCtrl) {
        suppressDefaultProp_ = true;
    }

    emitExpr(*node.object);

    suppressDefaultProp_ = prevSuppress;

    // Fix 092o: 目标表达式已生成完毕 (期间保持外层栈顶), 现在把本帧 withInfo 入栈 —
    // body 内的 .成员 解析与下方的 Menu 判定都依赖它.
    withObjectInfoStack_.push_back(withInfo);

    if (!withObjectInfoStack_.empty() && withObjectInfoStack_.back().kind == WithObjKind::FormControl && withObjectInfoStack_.back().ctrlType == FrmControlType::Menu) {  // P20-36
        c_.emitLine("int " + tempVar + " = 0;  /* Menu: no HWND, props use (hmenu,menuId) */");
    } else {
        // Fix 038: C2440 修复 — UDT 同类型转换和 UDT/VARIANT → void* 转换
        bool isUdtTempType = (tempType.rfind("vb6_type_", 0) == 0);
        if (isUdtTempType) {
            // Fix 081j: UDT With块使用指针引用，而非值拷贝
            // VB6中 With uPoints(lIdx) 内 .X = ... 直接修改数组元素
            // C中需要用指针: vb6_type_RECT* _vb6_with = &VB6_SA_AT(...)
            c_.emitLine(tempType + "* " + tempVar + " = &(" + lastExpr_ + ")  /* With object ref (ptr) */;");
        } else if (tempType == "void*") {
            // 检查表达式是否为 UDT 或 VARIANT — 这些类型不能直接 cast 到 void*
            std::string udtCType = inferUdtTypeOfExpr(*node.object);
            if (!udtCType.empty()) {
                // UDT → void*: 取地址获取指针
                c_.emitLine(tempType + " " + tempVar + " = &(" + lastExpr_ + ")  /* With object ref */;");
            } else if (isDefinitelyVariantExpr(*node.object) && inferClassTypeOfExpr(*node.object).empty()) {
                // VARIANT → void*: 用 VariantToObjectVal 提取对象指针
                // Fix 084e: 若表达式实际是类对象 (如 Cookies("name") 返回
                // vb6_cls_cHttpServerCookieAttr* 但被误判为 Variant), 则
                // VariantToObjectVal(对象指针) 触发 C2440, 走下方 (void*) 直转.
                // Fix 090e: 符号表把项目类默认成员属性 (Property Get Cookie()
                // As cHttpServerCookieAttr) 的返回类型误注册为 Variant 时,
                // 生成的 C 表达式是 vb6_cHttpServerCookies_prop_get_Cookie(...)
                // (返回 vb6_cls_cHttpServerCookieAttr*), 并非 vb6_VARIANT 值;
                // 对类指针调 VariantToObjectVal → C2440 (cHttpServerCookies
                // ExpireCookie: With Cookie(Key)). 仅当 C 级确认实参是
                // vb6_VARIANT (cExprIsVariant / 已知 Variant 变量/字段) 时
                // 走 VariantToObjectVal, 否则按对象指针 (void*) 直转.
                bool withIsVariantVal090e = cExprIsVariant(lastExpr_);
                if (!withIsVariantVal090e) {
                    std::string lower090e = lastExpr_;
                    std::transform(lower090e.begin(), lower090e.end(),
                                   lower090e.begin(), ::tolower);
                    if (knownVariantVars_.count(lower090e)) {
                        withIsVariantVal090e = true;
                    } else if (lower090e.compare(0, 4, "me->") == 0) {
                        std::string mem090e = lower090e.substr(4);
                        if (classVariantMembers_.count(mem090e)) {
                            withIsVariantVal090e = true;
                        }
                    }
                }
                if (withIsVariantVal090e) {
                    c_.emitLine(tempType + " " + tempVar + " = vb6_VariantToObjectVal(" + lastExpr_ + ")  /* With object ref */;");
                } else {
                    c_.emitLine(tempType + " " + tempVar + " = (" + tempType + ")" + lastExpr_ + "  /* With object ref */;");
                }
            } else {
                // Fix 092j: inferClassTypeOfExpr 非空 (按 VB 声明推断出项目类) 但
                // C 级表达式实际是 Variant 值 (COM 链结果) → 不能 (void*) 硬转:
                //   cLang 144: With me->LangInfo.Item("LangList").Item(Idx+1)
                //     → (void*)vb6_VariantFromComResult(vb6_ComCall(...)) C2440
                //       "无法从 vb6_VARIANT 转换为 void *";
                // 仅当 C 级确认是 vb6_VARIANT 时改用 VariantToObjectVal 提取.
                if (cExprIsVariant(lastExpr_)) {
                    c_.emitLine(tempType + " " + tempVar + " = vb6_VariantToObjectVal(" + lastExpr_ + ")  /* With object ref */;");
                } else {
                    c_.emitLine(tempType + " " + tempVar + " = (" + tempType + ")" + lastExpr_ + "  /* With object ref */;");
                }
            }
        } else {
            c_.emitLine(tempType + " " + tempVar + " = (" + tempType + ")" + lastExpr_ + "  /* With object ref */;");
        }
    }

    withObjectVars_.push_back(tempVar);

    c_.emitLine("{");
    c_.indent();
    emitStmtList(node.body);
    c_.dedent();
    c_.emitLine("}");

    withObjectVars_.pop_back();
    withObjectInfoStack_.pop_back();
}


} // namespace vb6c3
