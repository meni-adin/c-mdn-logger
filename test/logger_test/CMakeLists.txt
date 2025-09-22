set(TARGET_NAME logger_test)

include(GoogleTest)

set(TARGET_SOURCES
    "logger_test.cpp"
)

add_executable(${TARGET_NAME}
    ${TARGET_SOURCES}
)
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(${TARGET_NAME} PRIVATE
        /wd5045
    )
endif()

target_link_libraries(${TARGET_NAME}
    GTest::gmock
    mdn_gtest_extension
    mdn_mock_wrapper
    mdn_standard_streams_redirection
    mdn_logger
)

cmake_language(CALL ${PROJECT_NAME}_set_target_cpp_compiler_flags ${TARGET_NAME})
gtest_discover_tests(${TARGET_NAME})
