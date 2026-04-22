cmake_minimum_required(VERSION 3.20)
project(owplog VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(OWPLOG_BUILD_TESTS "Build owplog tests" ON)
option(OWPLOG_BUILD_SHARED_C_API "Build owplog C ABI shared library" ON)
option(OWPLOG_BUILD_E2E_EXAMPLES "Build end-to-end cross-language examples" ON)

find_package(Threads REQUIRED)

add_library(owplog STATIC
    src/filter.cpp
    src/formatter.cpp
    src/logger.cpp
    src/sink.cpp
)
add_library(OWPLOG::owplog ALIAS owplog)

target_include_directories(owplog
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(owplog PUBLIC Threads::Threads)

if(OWPLOG_BUILD_SHARED_C_API)
    add_library(owplog_c SHARED src/owplog_c_api.cpp)
    add_library(OWPLOG::owplog_c ALIAS owplog_c)
    target_include_directories(owplog_c PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(owplog_c PRIVATE owplog)
endif()

if(OWPLOG_BUILD_TESTS)
    include(CTest)
    enable_testing()

    find_package(GTest CONFIG QUIET)
    if(NOT GTest_FOUND)
        include(FetchContent)
        FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        )
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()

    add_executable(owplog_tests tests/logger_tests.cpp)
    target_link_libraries(owplog_tests PRIVATE owplog GTest::gtest_main)
    if(TARGET owplog_c)
        target_link_libraries(owplog_tests PRIVATE owplog_c)
    endif()

    include(GoogleTest)
    gtest_discover_tests(owplog_tests)
endif()

if(OWPLOG_BUILD_E2E_EXAMPLES)
    add_subdirectory(examples/e2e)
endif()
