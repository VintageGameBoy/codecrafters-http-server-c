# 最低版本要求
cmake_minimum_required(VERSION 3.10)

# 项目名称
project(CodeCraftersProject C)

# 设置C语言标准为C11
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

# 设置可执行文件的输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 收集所有的C源文件
file(GLOB_RECURSE SOURCES "app/*.c")

# 添加可执行文件
add_executable(CODECRAFTERS-HTTP-SERVER-C ${SOURCES})

target_link_libraries(CODECRAFTERS-HTTP-SERVER-C -lz)
# 如果需要链接其他库，可以在这里添加
# target_link_libraries(CodeCraftersExecutable some_library)
