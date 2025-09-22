include(fetch_content_wrapper)
cmake_language(CALL ${PROJECT_NAME}_fetch_content_wrapper
    mdn_status
    GIT_REPOSITORY https://github.com/meni-adin/c-mdn-status.git
    GIT_TAG        main
)
cmake_language(CALL ${PROJECT_NAME}_fetch_content_wrapper
    mdn_mock_wrapper
    GIT_REPOSITORY https://github.com/meni-adin/c-mdn-mock-wrapper.git
    GIT_TAG        main
)

add_subdirectory(logger)
