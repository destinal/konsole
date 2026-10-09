include(ECMAddTests)

ecm_add_test(
    SessionStatusTest.cpp
    ../sessionstatus.cpp
    ../sessionstatus.h
    TEST_NAME ClaudeCodeSessionStatusTest
    LINK_LIBRARIES Qt::Test konsoleprivate
)
target_include_directories(ClaudeCodeSessionStatusTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

ecm_add_test(
    ProgramStatusRecordsTest.cpp
    ../programstatusrecords.cpp
    ../programstatusrecords.h
    ../sessionstatus.cpp
    ../sessionstatus.h
    TEST_NAME ClaudeCodeProgramStatusRecordsTest
    LINK_LIBRARIES Qt::Test KF6::I18n konsoleprivate
)
target_include_directories(ClaudeCodeProgramStatusRecordsTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

ecm_add_test(
    ClippingStoreTest.cpp
    ../clippingstore.cpp
    ../clippingstore.h
    TEST_NAME ClaudeCodeClippingStoreTest
    LINK_LIBRARIES Qt::Test konsoleprivate
)
target_include_directories(ClaudeCodeClippingStoreTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
