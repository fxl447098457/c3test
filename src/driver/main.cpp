#include "driver/driver.hpp"

int main(int argc, char* argv[]) {
    vb6c3::Driver driver;
    auto result = driver.compile(argc, argv);

    if (!result.success && result.errorCount == 0) {
        // 命令行解析阶段已经输出了错误信息
        return 1;
    }

    return result.errorCount > 0 ? 1 : 0;
}
