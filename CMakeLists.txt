cmake_minimum_required(VERSION 3.10)
project(gitlite LANGUAGES CXX)

# 使用 C++17 标准 (支持 std::optional 等特性)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译警告与优化选项
add_compile_options(-Wall -Wextra -O2)

# 自动收集 src/ 下的所有源文件以及根目录的 main.cpp
file(GLOB SRC_FILES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/*.cpp")

# 1. 先创建可执行文件 gitlite
add_executable(gitlite
    ${PROJECT_SOURCE_DIR}/main.cpp
    ${SRC_FILES}
)

# 2. 再为 gitlite 目标指定头文件目录
target_include_directories(gitlite PRIVATE ${PROJECT_SOURCE_DIR}/include)

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