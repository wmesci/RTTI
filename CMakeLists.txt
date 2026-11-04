cmake_minimum_required(VERSION 3.14)

project(RTTI VERSION 1.1.0 LANGUAGES CXX)

set(RTTI_LIBRARY_NAME rtti)

set(RTTI_HEADERS
    src/Attributable.h
    src/Config.h
    src/DynamicType.h
    src/Error.h
    src/MethodInfo.h
    src/Object.h
    src/ObjectBox.h
    src/PropertyInfo.h
    src/RTTI.h
    src/Reflection.h
    src/System.h
    src/Type.h
    src/TypeRegister.h
    src/nameof.hpp
)

add_library(${RTTI_LIBRARY_NAME} INTERFACE)

target_include_directories(${RTTI_LIBRARY_NAME}
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include/RTTI>
)

target_compile_features(${RTTI_LIBRARY_NAME} INTERFACE cxx_std_20)

add_library(rtti::rtti ALIAS ${RTTI_LIBRARY_NAME})

option(RTTI_BUILD_TESTS "Build RTTI tests" OFF)
# Backwards-compatible name used by older integrations.
if(DEFINED BUILD_RTTI_TESTS AND BUILD_RTTI_TESTS)
    message(DEPRECATION "BUILD_RTTI_TESTS is deprecated; use RTTI_BUILD_TESTS")
    set(RTTI_BUILD_TESTS ON)
endif()

if(RTTI_BUILD_TESTS)
    enable_testing()

    add_executable(rtti_test tests/test.cpp)

    if(MSVC)
        target_compile_options(rtti_test PRIVATE /W4 /WX /permissive-)
    else()
        target_compile_options(rtti_test PRIVATE -Wall -Wextra -Werror)
    endif()

    target_link_libraries(rtti_test PRIVATE ${RTTI_LIBRARY_NAME})
    target_include_directories(rtti_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

    add_test(NAME RTTI_Test COMMAND rtti_test)
    set_tests_properties(RTTI_Test PROPERTIES TIMEOUT 60)
endif()

include(GNUInstallDirs)

install(TARGETS ${RTTI_LIBRARY_NAME}
    EXPORT RTTITargets
)

install(FILES ${RTTI_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/RTTI
)

install(FILES LICENSE README.md MIGRATION.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/rtti
)

install(EXPORT RTTITargets
    FILE RTTITargets.cmake
    NAMESPACE rtti::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rtti
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RTTIConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/RTTIConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rtti
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/RTTIConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
    ARCH_INDEPENDENT
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/RTTIConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/RTTIConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rtti
)

export(EXPORT RTTITargets
    FILE "${CMAKE_CURRENT_BINARY_DIR}/RTTITargets.cmake"
    NAMESPACE rtti::
)
