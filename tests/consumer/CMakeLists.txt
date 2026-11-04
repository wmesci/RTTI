cmake_minimum_required(VERSION 3.14)
project(RTTI_Consumer LANGUAGES CXX)

find_package(RTTI REQUIRED)

add_executable(rtti_consumer main.cpp)
target_link_libraries(rtti_consumer PRIVATE rtti::rtti)
target_compile_features(rtti_consumer PRIVATE cxx_std_20)
