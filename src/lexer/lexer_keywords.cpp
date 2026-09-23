#include "lexer/lexer.hpp"

namespace vb6c3 {

// === 关键字映射 ===

void Lexer::initKeywords() {
    // VB6关键字 (全小写映射, 因为VB6不区分大小写)
    keywords_ = {
        {"if", TokenKind::If}, {"then", TokenKind::Then},
        {"elseif", TokenKind::ElseIf}, {"else", TokenKind::Else},
        {"end", TokenKind::End},

        {"for", TokenKind::For}, {"to", TokenKind::To},
        {"step", TokenKind::Step}, {"next", TokenKind::Next},
        {"each", TokenKind::Each}, {"in", TokenKind::In},

        {"do", TokenKind::Do}, {"loop", TokenKind::Loop},
        {"while", TokenKind::While}, {"until", TokenKind::Until},
        {"wend", TokenKind::Wend},

        {"select", TokenKind::Select}, {"case", TokenKind::Case},
        {"is", TokenKind::IsKeyword},

        {"with", TokenKind::With},

        {"dim", TokenKind::Dim}, {"redim", TokenKind::ReDim},
        {"preserve", TokenKind::Preserve},
        {"const", TokenKind::Const},
        {"public", TokenKind::Public}, {"private", TokenKind::Private},
        {"static", TokenKind::Static}, {"friend", TokenKind::Friend},
        {"global", TokenKind::Global},

        {"as", TokenKind::As}, {"new", TokenKind::New},
        {"withevents", TokenKind::WithEvents},

        {"sub", TokenKind::Sub}, {"function", TokenKind::Function},
        {"property", TokenKind::Property},
        {"get", TokenKind::Get}, {"let", TokenKind::Let}, {"set", TokenKind::Set},
        {"call", TokenKind::Call},
        {"declare", TokenKind::Declare}, {"lib", TokenKind::Lib},
        {"alias", TokenKind::Alias}, {"cdecl", TokenKind::CDecl},
        {"byval", TokenKind::ByVal}, {"byref", TokenKind::ByRef},
        {"optional", TokenKind::Optional},
        {"paramarray", TokenKind::ParamArray},
        {"default", TokenKind::Default},

        {"type", TokenKind::Type}, {"enum", TokenKind::Enum},
        {"event", TokenKind::Event}, {"raiseevent", TokenKind::RaiseEvent},
        {"delegate", TokenKind::Delegate},
        {"implements", TokenKind::Implements},
        {"class", TokenKind::Class},

        {"boolean", TokenKind::Boolean}, {"byte", TokenKind::Byte},
        {"integer", TokenKind::Integer}, {"long", TokenKind::Long},
        {"longlong", TokenKind::LongLong}, {"longptr", TokenKind::LongPtr},
        {"single", TokenKind::Single}, {"double", TokenKind::Double},
        {"currency", TokenKind::Currency}, {"decimal", TokenKind::Decimal},
        {"date", TokenKind::Date}, {"object", TokenKind::Object},
        {"string", TokenKind::String}, {"variant", TokenKind::Variant},
        {"any", TokenKind::Any},

        {"defbool", TokenKind::DefBool}, {"defbyte", TokenKind::DefByte},
        {"defint", TokenKind::DefInt}, {"deflng", TokenKind::DefLng},
        {"defcur", TokenKind::DefCur}, {"defsng", TokenKind::DefSng},
        {"defdbl", TokenKind::DefDbl}, {"defdate", TokenKind::DefDate},
        {"defstr", TokenKind::DefStr}, {"defobj", TokenKind::DefObj},
        {"defvar", TokenKind::DefVar},

        {"goto", TokenKind::GoTo}, {"gosub", TokenKind::GoSub},
        {"return", TokenKind::Return}, {"on", TokenKind::On},
        {"resume", TokenKind::Resume}, {"error", TokenKind::Error},
        {"stop", TokenKind::Stop},
        {"exit", TokenKind::Exit}, {"continue", TokenKind::Continue},

        {"open", TokenKind::Open}, {"close", TokenKind::Close},
        {"input", TokenKind::Input}, {"output", TokenKind::Output},
        {"append", TokenKind::Append}, {"binary", TokenKind::Binary},
        {"random", TokenKind::Random}, {"access", TokenKind::Access},
        {"read", TokenKind::Read}, {"write", TokenKind::Write},
        {"readwrite", TokenKind::ReadWrite}, {"shared", TokenKind::Shared},
        {"lock", TokenKind::Lock}, {"unlock", TokenKind::Unlock}, {"reset", TokenKind::Reset},
        {"get", TokenKind::Get}, {"put", TokenKind::Put},
        {"seek", TokenKind::Seek}, {"line", TokenKind::Line},
        {"width", TokenKind::Width}, {"print", TokenKind::Print},
        {"name", TokenKind::Name}, {"freefile", TokenKind::FreeFile},
        {"eof", TokenKind::EOF_keyword},

        {"chdir", TokenKind::ChDir}, {"chdrive", TokenKind::ChDrive},
        {"mkdir", TokenKind::MkDir}, {"rmdir", TokenKind::RmDir},
        {"curdir", TokenKind::CurDir}, {"dir", TokenKind::Dir},
        {"filecopy", TokenKind::FileCopy}, {"kill", TokenKind::Kill},
        {"setattr", TokenKind::SetAttr}, {"getattr", TokenKind::GetAttr},
        {"filelen", TokenKind::FileLen}, {"filedatetime", TokenKind::FileDateTime},

        {"beep", TokenKind::Beep}, {"doevents", TokenKind::DoEvents},
        {"sendkeys", TokenKind::SendKeys}, {"appactivate", TokenKind::AppActivate},
        {"shell", TokenKind::Shell}, {"environ", TokenKind::Environ},
        {"command", TokenKind::Command},
        {"randomize", TokenKind::Randomize}, {"timer", TokenKind::Timer},

        {"option", TokenKind::Option}, {"explicit", TokenKind::Explicit},
        {"compare", TokenKind::Compare}, {"base", TokenKind::Base},
        {"text", TokenKind::Text},

        {"attribute", TokenKind::Attribute},
        {"begin", TokenKind::Begin},
        {"rem", TokenKind::REM_keyword},

        {"mid", TokenKind::Mid}, {"lset", TokenKind::LSet},
        {"rset", TokenKind::RSet},
        {"msgbox", TokenKind::MsgBox}, {"inputbox", TokenKind::InputBox},
        {"rgb", TokenKind::RGB}, {"qbcolor", TokenKind::QBColor},
        {"load", TokenKind::Load}, {"unload", TokenKind::Unload},
        {"savepicture", TokenKind::SavePicture}, {"loadpicture", TokenKind::LoadPicture},
        {"createobject", TokenKind::CreateObject},
        {"getobject", TokenKind::GetObject},
        {"format", TokenKind::Format},

        {"and", TokenKind::And}, {"or", TokenKind::Or},
        {"xor", TokenKind::Xor}, {"not", TokenKind::Not},
        {"eqv", TokenKind::Eqv}, {"imp", TokenKind::Imp},
        {"like", TokenKind::Like}, {"mod", TokenKind::Mod},
        {"addressof", TokenKind::AddressOf},
        {"typeof", TokenKind::TypeOf},

        {"true", TokenKind::TrueKeyword}, {"false", TokenKind::FalseKeyword},
        {"nothing", TokenKind::NothingKeyword}, {"empty", TokenKind::EmptyKeyword},
        {"null", TokenKind::NullKeyword}, {"me", TokenKind::MeKeyword},

        // 条件编译 (以#开头, 但关键字映射同样)
        {"#if", TokenKind::HashIf},
        {"#elseif", TokenKind::HashElseIf},
        {"#else", TokenKind::HashElse},
        {"#end", TokenKind::HashEnd},
        {"#const", TokenKind::HashConst},
    };
}

} // namespace vb6c3
