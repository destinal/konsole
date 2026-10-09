include(ECMAddTests)

ecm_add_test(
    SessionStatusTest.cpp
    ../sessionstatus.cpp
    ../sessionstatus.h
    TEST_NAME ClaudeCodeSessionStatusTest
    LINK_LIBRARIES Qt::Test konsoleprivate
)
target_include_directories(ClaudeCodeSessionStatusTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
