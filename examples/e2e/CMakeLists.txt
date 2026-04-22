cmake_minimum_required(VERSION 3.20)

add_executable(owplog_e2e_native
    native_main.cpp
    c_component.c
    cpp_component.cpp
)
target_include_directories(owplog_e2e_native PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(owplog_e2e_native PRIVATE owplog)
if(TARGET owplog_c)
    target_link_libraries(owplog_e2e_native PRIVATE owplog_c)
endif()

include(CheckLanguage)
check_language(Fortran)
if(CMAKE_Fortran_COMPILER)
    enable_language(Fortran)
    add_library(owplog_e2e_fortran STATIC
        ${CMAKE_SOURCE_DIR}/interop/fortran/owplog_logger_mod.f90
        fortran_component.f90
    )
    target_link_libraries(owplog_e2e_native PRIVATE owplog_e2e_fortran)
    target_compile_definitions(owplog_e2e_native PRIVATE OWPLOG_E2E_HAVE_FORTRAN=1)
endif()

find_package(pybind11 CONFIG QUIET)
find_package(Python3 COMPONENTS Interpreter Development QUIET)
if(pybind11_FOUND AND Python3_FOUND)
    target_link_libraries(owplog_e2e_native PRIVATE pybind11::embed)
    target_compile_definitions(owplog_e2e_native PRIVATE OWPLOG_E2E_HAVE_PYTHON_EMBED=1)

    add_executable(owplog_e2e_pybind11 pybind11_embed_main.cpp)
    target_include_directories(owplog_e2e_pybind11 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(owplog_e2e_pybind11 PRIVATE owplog pybind11::embed)
    if(TARGET owplog_c)
        target_link_libraries(owplog_e2e_pybind11 PRIVATE owplog_c)
    endif()
endif()
