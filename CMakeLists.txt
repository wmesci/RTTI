# RTTI CMakeLists.txt
# 设置最低CMake版本

cmake_minimum_required(VERSION 3.14)

# 创建项目
project(RTTI VERSION 1.0.0 LANGUAGES CXX)

# 设置C++标准为C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 设置库名称
set(RTTI_LIBRARY_NAME rtti)

# 使用GLOB_RECURSE搜集头文件
file(GLOB_RECURSE RTTI_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

# 创建接口库
add_library(${RTTI_LIBRARY_NAME} INTERFACE)

# 设置库的公共头文件目录
target_include_directories(${RTTI_LIBRARY_NAME}
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include/RTTI>
)

# 设置C++标准
target_compile_features(${RTTI_LIBRARY_NAME} INTERFACE cxx_std_20)

# 创建别名目标，方便外部项目引用
add_library(rtti::rtti ALIAS ${RTTI_LIBRARY_NAME})

# 添加构建测试的选项，默认关闭
option(BUILD_RTTI_TESTS "Build RTTI tests" OFF)

# 如果启用测试构建
if(BUILD_RTTI_TESTS)
    # 启用测试
    enable_testing()
    
    # 添加测试可执行文件
    add_executable(rtti_test tests/test.cpp)
    
    if(MSVC)
        target_compile_options(rtti_test PRIVATE /W4 /WX)
    else()
        target_compile_options(rtti_test PRIVATE -Wall -Wextra -Werror)
    endif()

    # 链接RTTI库
    target_link_libraries(rtti_test PRIVATE ${RTTI_LIBRARY_NAME})
    
    # 设置测试可执行文件的包含目录
    target_include_directories(rtti_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    
    # 添加测试
    add_test(NAME RTTI_Test COMMAND rtti_test)
endif()

# 安装规则
include(GNUInstallDirs)

# 安装库文件
install(TARGETS ${RTTI_LIBRARY_NAME}
    EXPORT RTTITargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# 安装头文件
install(FILES ${RTTI_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/RTTI
)

# 安装导出目标
install(EXPORT RTTITargets
    FILE RTTITargets.cmake
    NAMESPACE rtti::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rtti
)

# 创建配置文件
include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RTTIConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/RTTIConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rtti
)

# 创建版本文件
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/RTTIConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# 安装配置文件
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/RTTIConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/RTTIConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rtti
)

# 导出目标
export(EXPORT RTTITargets
    FILE "${CMAKE_CURRENT_BINARY_DIR}/RTTITargets.cmake"
    NAMESPACE rtti::
)