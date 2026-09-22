#pragma once
// VB6 Token定义 - 所有token类型枚举和Token结构体

#include <string>
#include <cstdint>

namespace vb6c3 {

// Token类别
enum class TokenKind : uint16_t {
    // === 文件级 ===
    EndOfFile = 0,
    NewLine,

    // === 标识符和字面量 ===
    Identifier,     // 以字母开头, 可含字母/数字/下划线, VB6标识符不区分大小写
    IntegerLiteral, // &H hex, &O oct, &B bin, 十进制
    LongLiteral,    // 后缀 &
    LongPtrLiteral, // 后缀 ^ (VBA7 LongPtr, 指针宽度)
    FloatLiteral,   // Single(!后缀) / Double(#后缀, 或有小数点/E)
    DecimalLiteral, // @后缀 (Currency)
    StringLiteral,  // "..."
    DateLiteral,    // #...#
    TrueKeyword,
    FalseKeyword,
    NothingKeyword,
    EmptyKeyword,
    NullKeyword,
    MeKeyword,

    // === 运算符 ===
    // 算术
    Plus,           // +
    Minus,          // -
    Star,           // *
    Slash,          // /
    BackSlash,      // \ 整除
    Mod,            // Mod
    Caret,          // ^ 幂
    // 字符串连接
    Ampersand,      // &
    // 赋值/比较
    Equals,         // = 赋值或比较(上下文决定)
    NotEquals,      // <>
    LessThan,       // <
    GreaterThan,    // >
    LessEqual,      // <=
    GreaterEqual,   // >=
    // 逻辑(也是位运算)
    And,
    Or,
    Xor,
    Not,
    Eqv,
    Imp,
    // 对象比较
    Is,
    Like,
    // 特殊运算符
    AddressOf,      // AddressOf
    TypeOf,         // TypeOf

    // === 分隔符 ===
    LeftParen,      // (
    RightParen,     // )
    LeftBracket,    // [   (用于括号标识符)
    RightBracket,   // ]
    Dot,            // .
    Comma,          // ,
    Colon,          // :
    Semicolon,      // ;
    Exclamation,    // !  (字典访问 / Single类型后缀)
    Hash,           // #  (Date定界符 / 文件号 / Double后缀)
    Percent,        // %  (Integer类型后缀)
    AtSign,         // @  (Currency类型后缀)
    Dollar,         // $  (String类型后缀)
    Assign,         // := (命名参数)

    // === 块结构关键字 ===
    If,
    Then,
    ElseIf,
    Else,
    End,

    For,
    To,
    Step,
    Next,
    Each,
    In,

    Do,
    Loop,
    While,
    Until,
    Wend,

    Select,
    Case,
    IsKeyword,      // Case Is 中的 Is

    With,
    // 无End With, End With由End + With组合解析

    // === 声明关键字 ===
    Dim,
    ReDim,
    Preserve,
    Const,
    Public,
    Private,
    Static,
    Friend,
    Global,

    As,
    New,
    WithEvents,
    PropertyGet,    // Property Get (解析为Property + Get，此处仅标记)

    // === 过程关键字 ===
    Sub,
    Function,
    Property,
    Get,            // Property Get / Get #文件号 (共用同一token)
    Let,
    Set,
    Call,
    Declare,
    Lib,
    Alias,
    CDecl,       // Declare语句的CDecl调用约定
    ByVal,
    ByRef,
    Optional,
    ParamArray,
    Default,

    // === 类型关键字 ===
    Type,
    Enum,
    Event,
    Delegate,       // Delegate 语句 (tB 扩展: 带签名检查的函数指针类型)
    RaiseEvent,
    Implements,
    Class,
    Interface,      // Interface 语句 (tB 扩展: 显式接口契约块, 见 ai/022 D1)
    Extends,        // Interface 的单继承子句 (仅接口域; 类继承用 Inherits, P3)

    // === 数据类型关键字 ===
    Boolean,
    Byte,
    Integer,
    Long,
    LongLong,        // 64-bit (VB7+)
    LongPtr,         // 平台相关指针 (VB7+)
    Single,
    Double,
    Currency,
    Decimal,
    Date,
    Object,
    String,
    Variant,
    Any,             // Declare中的Any类型

    // === 类型后缀关键字 ===
    DefBool,
    DefByte,
    DefInt,
    DefLng,
    DefCur,
    DefSng,
    DefDbl,
    DefDate,
    DefStr,
    DefObj,
    DefVar,

    // === 控制流语句关键字 ===
    GoTo,
    GoSub,
    Return,
    On,
    Resume,
    Error,          // Error语句(不是Err对象)
    Stop,

    Exit,           // Exit Sub/Function/Property/Do/For
    Continue,       // Continue Do/For (VB7+保留)

    // === 错误处理 ===
    OnError,        // On Error GoTo/Resume Next (解析时合并On+Error)

    // === 文件I/O关键字 ===
    Open,
    Close,
    Input,
    Output,
    Append,
    Binary,
    Random,
    Access,
    Read,
    Write,
    ReadWrite,
    Shared,
    Lock,
    Unlock,
    Reset,
    Put,            // Put (文件I/O，与Get对应)
    Seek,
    Line,           // Line Input / Line控件
    Width,
    Print,
    Name,           // Name 旧文件名 As 新文件名
    FreeFile,
    EOF_keyword,    // EOF (避免同名宏)

    // === 文件系统 ===
    ChDir,
    ChDrive,
    MkDir,
    RmDir,
    CurDir,
    Dir,
    FileCopy,
    Kill,
    SetAttr,
    GetAttr,
    FileLen,
    FileDateTime,

    // === 杂项语句关键字 ===
    Beep,
    DoEvents,
    SendKeys,
    AppActivate,
    Shell,
    Environ,
    Command,
    Randomize,
    Timer,

    // === 日期/时间 ===
    DateValue,
    TimeValue,
    DateAdd,
    DateDiff,
    DatePart,
    DateSerial,
    TimeSerial,

    // === 杂项 ===
    Option,
    Explicit,
    Compare,
    Base,
    Private2,       // Option Private Module (Private是声明关键字,此处复用)
    Text,
    Binary2,        // Option Compare Text/Binary

    Attribute,
    Begin,          // .frm控件块
    REM_keyword,    // REM注释关键字

    // === 条件编译 ===
    HashIf,         // #If
    HashElseIf,     // #ElseIf
    HashElse,       // #Else
    HashEnd,        // #End If
    HashConst,      // #Const

    // === 内置函数(部分高频函数直接做keyword) ===
    MsgBox,
    InputBox,
    RGB,
    QBColor,
    Load,
    Unload,
    LSet,
    RSet,
    Mid,
    Format,
    SavePicture,
    LoadPicture,
    CreateObject,
    GetObject,

    // === 注释 ===
    Comment,        // ' 或 REM

    // === 行续接 ===
    LineContinuation, // _ 行尾续行符

    // === 错误Token ===
    Invalid,
};

// Token结构体
struct Token {
    TokenKind kind = TokenKind::EndOfFile;
    std::string text;     // 原始文本
    uint32_t line = 1;    // 行号 (1-based)
    uint32_t column = 1;  // 列号 (1-based)
    uint32_t length = 0;  // 文本长度

    // 字面量值 (按类型使用)
    union {
        int32_t intValue = 0;
        int64_t longValue;
        float floatValue;
        double doubleValue;
    };

    bool isKeyword() const;
    bool isOperator() const;
    bool isLiteral() const;
    bool isTypeSuffix() const;
    bool isStatementStart() const;

    std::string toString() const;
    static const char* kindToString(TokenKind kind);
};

} // namespace vb6c3
