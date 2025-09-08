cmake_minimum_required(VERSION 3.5)

# 如果项目作为子项目（通过FetchContent等方式引入），则PROJECT_NAME变量已经定义
if(NOT DEFINED PROJECT_NAME)
  SET(RTTI_MASTER_PROJECT ON)
  project(RTTI)
else()
  SET(RTTI_MASTER_PROJECT OFF)
endif()

SET(TARGET_NAME RTTI)

SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)

FILE(GLOB_RECURSE RTTI_SOURCE src/*.cpp)

SET(TARGET_SOURCE ${RTTI_SOURCE})

ADD_LIBRARY(${TARGET_NAME} STATIC ${TARGET_SOURCE})

TARGET_INCLUDE_DIRECTORIES(${TARGET_NAME} PUBLIC 
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

# 创建别名目标，便于在其他项目中使用
add_library(rtti::rtti ALIAS ${TARGET_NAME})

# for test
OPTION(RTTI_BUILD_TEST "Built RTTI Test" OFF)

# 只有当这是主项目或者明确要求构建测试时才构建测试
IF(RTTI_BUILD_TEST AND RTTI_MASTER_PROJECT)
    MESSAGE("Build RTTI Test")

    ADD_EXECUTABLE(${TARGET_NAME}-Test tests/test.cpp)

    TARGET_LINK_LIBRARIES(${TARGET_NAME}-Test ${TARGET_NAME})
ENDIF()

# 如果这是主项目，设置安装规则
if(RTTI_MASTER_PROJECT)
    install(TARGETS ${TARGET_NAME}
        EXPORT RTTITargets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include
    )

    install(DIRECTORY src/ 
        DESTINATION include
        FILES_MATCHING PATTERN "*.h"
    )

    install(EXPORT RTTITargets
        FILE RTTIConfig.cmake
        NAMESPACE RTTI::
        DESTINATION lib/cmake/RTTI
    )
endif()