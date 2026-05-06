cmake_minimum_required(VERSION 3.20)

set(Python3_FIND_VIRTUALENV FIRST)
set(Python3_FIND_STRATEGY LOCATION)
set(OWPLOG_PYTHON_PREFIX "" CACHE PATH
        "Optional Python/Conda prefix used to find Python development files and pybind11"
)

set(_owplog_dependency_prefixes)

if(OWPLOG_PYTHON_PREFIX)
    list(APPEND _owplog_dependency_prefixes "${OWPLOG_PYTHON_PREFIX}")
endif()

if(DEFINED ENV{_CONDA_ROOT})
    message(STATUS "Conda prefix found: $ENV{_CONDA_ROOT}")
    get_filename_component(_owplog_conda_parent "$ENV{_CONDA_ROOT}" ABSOLUTE)
    get_filename_component(_owplog_conda_root "$ENV{_CONDA_ROOT}/../.." ABSOLUTE)
    list(APPEND _owplog_dependency_prefixes "${_owplog_conda_root}")
else()
    message(STATUS "Conda prefix wasn't found")
endif()

if(DEFINED ENV{CONDA_EXE})
    message(STATUS "The conda executable was found: $ENV{CONDA_EXE}")
    get_filename_component(_owplog_conda_bin "$ENV{CONDA_EXE}" DIRECTORY)
    get_filename_component(_owplog_conda_root "${_owplog_conda_bin}" DIRECTORY)
    list(APPEND _owplog_dependency_prefixes "${_owplog_conda_root}")
endif()

if(NOT Python3_EXECUTABLE)
    foreach(_owplog_python_prefix IN LISTS _owplog_dependency_prefixes)
        if(EXISTS "${_owplog_python_prefix}/bin/python3")
            set(Python3_EXECUTABLE "${_owplog_python_prefix}/bin/python3" CACHE FILEPATH
                "Python Interpreter used for owplog Python embed examples"
            )
            set(Python3_ROOT_DIR "${_owplog_python_prefix}" CACHE PATH
                "Python root used for owplog Python embed examples"
            )
            message(STATUS "The Python 3 executable was found at: ${Python3_EXECUTABLE}")
            message(STATUS "The root directory for the found Python 3 executable was set to: ${Python3_ROOT_DIR}")
            break()
        elseif (EXISTS "${_owplog_python_prefix}/bin/python")
            set(Python3_EXECUTABLE "${_owplog_python_prefix}/bin/python" CACHE FILEPATH
                    "Python Interpreter used for owplog Python embed examples"
            )
            set(Python3_ROOT_DIR "${_owplog_python_prefix}" CACHE PATH
                    "Python root used for owplog Python embed examples"
            )
            message(STATUS "The Python 3 executable was found at: ${Python3_EXECUTABLE}")
            message(STATUS "The root directory for the found Python 3 executable was set to: ${Python3_ROOT_DIR}")
            break()
        else()
            message(STATUS "Could not found a python executable under '${_owplog_python_prefix}'")
        endif ()
    endforeach ()
endif()

if(Python3_EXECUTABLE)
    get_filename_component(_owplog_python_bin "${Python3_EXECUTABLE}" DIRECTORY)
    get_filename_component(_owplog_python_prefix "${_owplog_python_bin}" DIRECTORY)
    message(STATUS "Since the python executable was found, set the _owplog_python_prefix to: ${_owplog_python_prefix}")
    list(APPEND _owplog_dependency_prefixes "${_owplog_python_prefix}")
endif()

if(_owplog_dependency_prefixes)
    list(REMOVE_DUPLICATES _owplog_dependency_prefixes)
    message(STATUS "The _owplog_dependency_prefixes are now: ${_owplog_dependency_prefixes}")
    list(PREPEND CMAKE_PREFIX_PATH ${_owplog_dependency_prefixes})
    message(STATUS "Set the CMAKE_PREFIX_PATH to ${CMAKE_PREFIX_PATH}")
endif()

add_executable(
    owplog_example
    main.cpp
    c_component.c
    cpp_component.cpp
    threadpool.cpp
)

target_include_directories(owplog_example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(owplog_example PRIVATE owplog)

if(TARGET owplog_c)
    target_link_libraries(owplog_example PRIVATE owplog_c)
endif()

include(CheckLanguage)
check_language(Fortran)
if(CMAKE_Fortran_COMPILER)
    enable_language(Fortran)
    add_library(owplog_e2e_fortran STATIC
        ${CMAKE_SOURCE_DIR}/interop/fortran/owplog_logger_mod.f90
        example_fortran.f90
    )
    target_link_libraries(owplog_example PRIVATE owplog_e2e_fortran)
    target_compile_definitions(owplog_example PRIVATE OWPLOG_E2E_HAVE_FORTRAN=1)
endif()

find_package(Python3 COMPONENTS Interpreter Development.Embed QUIET)

if(Python3_Interpreter_FOUND AND NOT pybind11_DIR)
    message(STATUS "Python3 interpreter found, but not pybind11. calling '${Python3_EXECUTABLE} -m pybind11 --cmakedir' to find it")
    execute_process(
            COMMAND "${Python3_EXECUTABLE}" -m pybind11 --cmakedir
            OUTPUT_VARIABLE _owplog_pybind11_cmake_dir
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE _owplog_pybind11_cmake_result
    )

    set(
        PYTHON_SCRIPTS
        ${CMAKE_CURRENT_SOURCE_DIR}/owplog_api.py
        ${CMAKE_CURRENT_SOURCE_DIR}/owplog_logger.py
        ${CMAKE_CURRENT_SOURCE_DIR}/python_consumer.py
    )

    add_custom_command(
        TARGET owplog_example
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${PYTHON_SCRIPTS}
            $<TARGET_FILE_DIR:owplog_example>
    )

    if (_owplog_pybind11_cmake_result EQUAL 0 AND EXISTS "${_owplog_pybind11_cmake_dir}")
        set(pybind11_DIR "${_owplog_pybind11_cmake_dir}")
        message(STATUS "pybind11 found and had its directory set to: ${pybind11_DIR}")
    else()
        message(STATUS "pybind11 was not found!")
        message(STATUS "_owplog_pybind11_cmake_result: ${_owplog_pybind11_cmake_result}")
        message(STATUS "_owplog_pybind11_cmake_dir: ${_owplog_pybind11_cmake_dir}")
    endif()

endif ()

find_package(pybind11 CONFIG QUIET)

if(pybind11_FOUND AND Python3_Development.Embed_FOUND)
    target_link_libraries(owplog_example PRIVATE pybind11::embed)
    target_compile_definitions(owplog_example PRIVATE OWPLOG_E2E_HAVE_PYTHON_EMBED=1)
    message(STATUS "pybind11 Found and Python3 Development.Embed found and attached.")
else()
    message(STATUS "Python embed example disabled:")
    message(STATUS "    Python3_EXECUTABLE='${Python3_EXECUTABLE}'")
    message(STATUS "    Python3_Development.Embed_FOUND='${Python3_Development.Embed_FOUND}'")
    message(STATUS "    pybind11_FOUND='${pybind11_FOUND}'")
    message(STATUS "    pybind11_DIR='${pybind11_DIR}'")
endif()
