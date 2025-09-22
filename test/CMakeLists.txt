include(fetch_content_wrapper)
cmake_language(CALL ${PROJECT_NAME}_fetch_content_wrapper
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        main
)
cmake_language(CALL ${PROJECT_NAME}_fetch_content_wrapper
    mdn_gtest_extension
    GIT_REPOSITORY https://github.com/meni-adin/cpp-mdn-gtest-extension.git
    GIT_TAG        main
)
cmake_language(CALL ${PROJECT_NAME}_fetch_content_wrapper
    mdn_standard_streams_redirection
    GIT_REPOSITORY https://github.com/meni-adin/c-mdn-standard-streams-redirection.git
    GIT_TAG        main
)

enable_testing()

add_subdirectory(logger_test)
