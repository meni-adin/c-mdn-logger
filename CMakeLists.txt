cmake_minimum_required(VERSION 3.28)
project(mdn_logger
    LANGUAGES C CXX
)
string(TOUPPER ${PROJECT_NAME} PROJECT_NAME_UC)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(${PROJECT_NAME_UC}_ENABLE_TESTS "Enable building tests" OFF)
option(${PROJECT_NAME_UC}_SAFE_MODE "Enable safe mode for the library" OFF)
option(${PROJECT_NAME_UC}_SANITIZED_BUILD "Enable sanitizers for the build" OFF)

if(NOT (CMAKE_C_COMPILER_ID STREQUAL CMAKE_CXX_COMPILER_ID))
    message(FATAL_ERROR "CMAKE_C_COMPILER_ID (${CMAKE_C_COMPILER_ID}) expected to be identical to CMAKE_CXX_COMPILER_ID (${CMAKE_CXX_COMPILER_ID})")
endif()

file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" MY_CMAKE_SCRIPTS_DIR)
set(CMAKE_MODULE_PATH "${MY_CMAKE_SCRIPTS_DIR}" ${CMAKE_MODULE_PATH})

include(fetch_content_wrapper)
include(utils)

if(${PROJECT_NAME_UC}_SAFE_MODE)
    add_compile_definitions(${PROJECT_NAME_UC}_SAFE_MODE)
endif()

cmake_language(CALL ${PROJECT_NAME}_init_fetch_content_wrapper)
cmake_language(CALL ${PROJECT_NAME}_set_compiler_flags)

cmake_language(CALL ${PROJECT_NAME}_print_variable CMAKE_C_COMPILER_ID)
cmake_language(CALL ${PROJECT_NAME}_print_variable CMAKE_CXX_COMPILER_ID)
cmake_language(CALL ${PROJECT_NAME}_print_variable ${PROJECT_NAME_UC}_ENABLE_TESTS)
cmake_language(CALL ${PROJECT_NAME}_print_variable ${PROJECT_NAME_UC}_SAFE_MODE)
cmake_language(CALL ${PROJECT_NAME}_print_variable ${PROJECT_NAME_UC}_SANITIZED_BUILD)

if(${PROJECT_NAME_UC}_SANITIZED_BUILD)
    cmake_language(CALL ${PROJECT_NAME}_enable_sanitizers)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(${PROJECT_NAME_UC}_ENABLE_COVERAGE ON)
    cmake_language(CALL ${PROJECT_NAME}_delete_gcda_files)
    add_compile_definitions(MDN_MW_ENABLE_MOCKING)
else ()
    set(${PROJECT_NAME_UC}_ENABLE_COVERAGE OFF)
endif()

include(CTest)

add_subdirectory(src)
if(${PROJECT_NAME_UC}_ENABLE_TESTS)
    add_subdirectory(test)
endif()
