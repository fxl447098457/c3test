#pragma once
// AST 打印器 - 声明

#include <iostream>

namespace vb6c3 {

class Module;

// AST 文本树形打印 (实现在 ast_printer.cpp)
// 用法: ASTPrinter printer(std::cout); printer.print(module);
class ASTPrinter {
public:
    explicit ASTPrinter(std::ostream& os = std::cout);
    void print(Module& module);

private:
    std::ostream& os_;
};

} // namespace vb6c3
