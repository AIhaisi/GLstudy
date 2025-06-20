﻿# CMakeList.txt: 顶层 CMake 项目文件，在此处执行全局配置
# 并包含子项目。
#
cmake_minimum_required (VERSION 3.8)

# 如果支持，请为 MSVC 编译器启用热重载。
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("GLstudy")
project ("GLtest")

include_directories(
	SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/src/include
)

link_directories(
	SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/src/lib
)
link_directories(
	SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/src/dlls
)

file(GLOB LIB "./src/lib")
file(GLOB DLL "./src/dlls/*")

file(COPY ${LIB} DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${DLL} DESTINATION ${CMAKE_BINARY_DIR})

# 包含子项目。
add_subdirectory (func)

add_executable (GLstudy "main.cpp" "src/glad.c")
add_executable (GLtest "test.cpp" "src/glad.c" "src/stb_image.cpp")

target_link_libraries(GLstudy PRIVATE glfw3 assimp-vc143-mt)
target_link_libraries(GLtest PRIVATE glfw3 assimp-vc143-mt)
