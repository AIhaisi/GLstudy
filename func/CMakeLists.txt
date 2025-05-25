# CMakeList.txt: GLstudy 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#

# 将源代码添加到此项目的可执行文件。

# add_executable (GLstudy "GLstudy.cpp" "GLstudy.h")

file(GLOB_RECURSE FUNCS ./ *.cpp)

add_library (funcs ${FUNCS})


# TODO: 如有需要，请添加测试并安装目标。
