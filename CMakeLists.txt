cmake_minimum_required(VERSION 3.10)
project(gitlite LANGUAGES CXX)

# TODO: 请在此处自行完成项目的编译配置。
#
# 要求：
# 1. 在 build/ 中创建一个名称为 gitlite 的可执行目标；
# 2. 将 main.cpp 和所有需要的实现文件加入该目标；
# 3. 自行设置所需的 C++ 标准、头文件目录和编译选项。

# 使用 C++17 标准 (支持 std::optional 等特性)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译警告与优化选项
add_compile_options(-Wall -Wextra -O2)

# 包含头文件目录 include/
include_directories(${PROJECT_SOURCE_DIR}/include)

# 自动收集 src/ 下的所有源文件以及根目录的 main.cpp
file(GLOB SRC_FILES "${PROJECT_SOURCE_DIR}/src/*.cpp")

# 生成可执行文件 gitlite (当使用 cmake -B build 时, 默认输出在 build/gitlite)
add_executable(gitlite
    ${PROJECT_SOURCE_DIR}/main.cpp
    ${SRC_FILES}
)

# 强制确保 gitlite 输出在当前构建目录 (即 ./build/gitlite)
set_target_properties(gitlite PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# ==================== ACMOJ 配置（请勿删除）====================
# ACMOJ 的 Git 评测会从仓库根目录读取名为 code 的编译产物。
# 上方必须已经通过 add_executable(gitlite ...) 创建 gitlite 目标。
add_custom_command(
    TARGET gitlite POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:gitlite>"
            "${CMAKE_SOURCE_DIR}/code"
    COMMENT "Copying gitlite executable to ACMOJ artifact: code"
    VERBATIM
)
# ================== ACMOJ 配置结束（请勿删除）==================
