# A stand-in for iTerm2's it2 command, covering what cc-status runs. It is
# named it2 because cc-status looks for it2 next to its own executable.
add_executable(claudecode-it2 it2.cpp)
set_target_properties(claudecode-it2 PROPERTIES
    OUTPUT_NAME it2
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/claudecode
)
target_link_libraries(claudecode-it2 Qt::Core Qt::DBus)
install(TARGETS claudecode-it2 DESTINATION ${KDE_INSTALL_LIBEXECDIR}/konsole-claudecode)

# The hook Claude Code runs, installed beside it2 and cc-status. cc-status is
# built separately, with Swift, from iTerm2's source.
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../hook/claude-code-hook ${CMAKE_BINARY_DIR}/bin/claudecode/claude-code-hook COPYONLY)
install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/../hook/claude-code-hook DESTINATION ${KDE_INSTALL_LIBEXECDIR}/konsole-claudecode)
