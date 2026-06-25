#pragma once
// VB6条件编译预处理器
// 处理 #Const, #If...#ElseIf...#Else...#End If
// 位于 Lexer 和 Parser 之间, 过滤 token 流

#include "lexer/lexer.hpp"
#include "lexer/token.hpp"
#include "common/diagnostics.hpp"
#include "common/source_manager.hpp"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <deque>

namespace vb6c3 {

// 条件编译常量值 (VB6条件表达式只支持 Boolean 和 Long)
struct CondCompileValue {
    enum Kind { Boolean, Long, Undefined };
    Kind kind = Undefined;
    int64_t longValue = 0;
    bool boolValue = false;

    static CondCompileValue fromBool(bool v) {
        return {Boolean, 0, v};
    }
    static CondCompileValue fromLong(int64_t v) {
        return {Long, v, v != 0};
    }
    static CondCompileValue undef() {
        return {Undefined, 0, false};
    }

    // 转为bool: Undefined=false, Long=非零, Boolean=自身
    bool toBool() const {
        if (kind == Undefined) return false;
        if (kind == Long) return longValue != 0;
        return boolValue;
    }

    bool isDefined() const { return kind != Undefined; }
};

// 条件编译块状态 (用于嵌套 #If)
struct CondBlock {
    bool outerActive;      // 外层是否活跃 (决定本块是否有机会)
    bool currentActive;    // 当前分支是否活跃
    bool anyBranchTaken;   // 是否已有分支被选中 (用于 #ElseIf/#Else)
    bool hasElse;          // 是否已遇到 #Else
};

// 预处理器选项
struct PreprocessOptions {
    // 命令行定义: -d:WIN32=1 或 --define:DEBUG=-1
    std::unordered_map<std::string, CondCompileValue> defines;
};

class Preprocessor {
public:
    Preprocessor(std::shared_ptr<SourceBuffer> buffer, Diagnostics& diag,
                 const PreprocessOptions& options = PreprocessOptions());

    // 与Lexer相同的接口, 供Parser使用
    Token nextToken();
    const Token& peekToken();
    const Token& peekToken2();

    // 是否在活跃代码区 (非条件编译排除的代码)
    bool isActive() const;

    // 获取条件编译常量表 (供dump等使用)
    const std::unordered_map<std::string, CondCompileValue>& constants() const {
        return constants_;
    }

private:
    // 从词法器获取下一个原始token
    Token fetchRaw();

    // 处理预处理指令, 返回应该传递给解析器的token
    // 如果当前token是预处理指令, 处理后递归获取下一个; 否则直接返回
    Token processToken();

    // 预处理指令处理
    void handleHashConst();
    void handleHashIf();
    void handleHashElseIf();
    void handleHashElse();
    void handleHashEndIf();

    // 条件表达式求值
    // VB6条件编译表达式支持:
    //   - 布尔常量: True, False
    //   - 整数常量
    //   - 条件编译常量名
    //   - 逻辑运算: And, Or, Not, Xor, Eqv, Imp
    //   - 比较运算: =, <>, <, >, <=, >=
    //   - 算术运算: +, -, *, /, \, Mod, ^
    //   - 括号
    CondCompileValue evalCondition();

    // 条件表达式递归下降求值
    CondCompileValue evalOrExpr();
    CondCompileValue evalAndExpr();
    CondCompileValue evalNotExpr();
    CondCompileValue evalComparison();
    CondCompileValue evalAddExpr();
    CondCompileValue evalMulExpr();
    CondCompileValue evalUnaryExpr();
    CondCompileValue evalPrimaryExpr();

    // 跳过当前行剩余token (用于无效分支中的指令)
    void skipToEndOfLine();

    // 跳过整个块直到遇到匹配的 #ElseIf/#Else/#End If
    // 用于跳过非活跃分支
    void skipInactiveBlock();

    // 消费当前条件表达式token并前进
    Token condAdvance();
    const Token& condPeek() const;

    // 辅助
    SourceLocation currentLoc() const;

    // 状态
    Lexer lexer_;
    Diagnostics& diag_;

    // 条件编译常量表
    std::unordered_map<std::string, CondCompileValue> constants_;

    // 条件编译嵌套栈
    std::vector<CondBlock> condStack_;

    // 前瞻缓冲 (预处理器输出)
    std::deque<Token> lookahead_;

    // 条件表达式求值的token流
    // #If后面的表达式需要独立求值, 从词法器读token
    std::deque<Token> condTokens_;
    size_t condPos_ = 0;

    // 当前原始token (从词法器)
    Token rawCur_;

    // 安全限制: 防止无限循环吃尽内存
    static constexpr size_t MAX_TOKENS = 10'000'000;  // 单文件最多1000万token
    size_t tokenCount_ = 0;
    bool hitTokenLimit_ = false;
};

} // namespace vb6c3
