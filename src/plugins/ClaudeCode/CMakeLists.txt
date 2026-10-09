kcoreaddons_add_plugin(konsole_claudecodeplugin
SOURCES
    claudecodeplugin.cpp
    claudecodedbus.cpp
    claudehooks.cpp
    claudesessionsmodel.cpp
    claudesessionswidget.cpp
    clippingstore.cpp
    clippingswidget.cpp
    sessionnavigation.cpp
    statusnotifier.cpp
    workgroup.cpp
    workgroupmanager.cpp
    workgroupswitcher.cpp
    programstatusrecords.cpp
    sessionstatus.cpp
    sessionstatusregistry.cpp
    statusbadges.cpp
INSTALL_NAMESPACE
    "konsoleplugins"
)

configure_file(konsole_claudecode.in.json konsole_claudecode.json)
target_link_libraries(konsole_claudecodeplugin
    konsoleprivate
    konsoleapp
    Qt::DBus
    KF6::ConfigCore
    KF6::I18n
    KF6::Notifications
    KF6::WidgetsAddons
    KF6::WindowSystem
)
target_compile_definitions(konsole_claudecodeplugin PRIVATE
    -DRELEASE_SERVICE_VERSION="${RELEASE_SERVICE_VERSION}"
    -DCLAUDECODE_TOOLS_DIRECTORY="${KDE_INSTALL_FULL_LIBEXECDIR}/konsole-claudecode"
)

# The workgroup's member scripts, beside it2 and the hook. review-prompt.txt
# is iTerm2's default code review prompt.
foreach(file claudecode-diff claudecode-review review-prompt.txt code-review-system-prompt.txt code-review-settings.json)
    configure_file(workgroup/${file} ${CMAKE_BINARY_DIR}/bin/claudecode/${file} COPYONLY)
endforeach()
install(PROGRAMS workgroup/claudecode-diff workgroup/claudecode-review DESTINATION ${KDE_INSTALL_LIBEXECDIR}/konsole-claudecode)
# konsole-claude-setup adds the hooks to Claude Code's settings. The build
# tree's copy points at the build tree's hook.
set(CLAUDECODE_TOOLS_DIRECTORY ${CMAKE_BINARY_DIR}/bin/claudecode)
configure_file(setup/konsole-claude-setup.in ${CMAKE_BINARY_DIR}/bin/claudecode/konsole-claude-setup @ONLY
               FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
set(CLAUDECODE_TOOLS_DIRECTORY ${KDE_INSTALL_FULL_LIBEXECDIR}/konsole-claudecode)
configure_file(setup/konsole-claude-setup.in ${CMAKE_CURRENT_BINARY_DIR}/konsole-claude-setup @ONLY)
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/konsole-claude-setup DESTINATION ${KDE_INSTALL_BINDIR})

install(FILES workgroup/review-prompt.txt workgroup/code-review-system-prompt.txt workgroup/code-review-settings.json
        DESTINATION ${KDE_INSTALL_LIBEXECDIR}/konsole-claudecode)

add_subdirectory(it2)

if(BUILD_TESTING)
    add_subdirectory(autotests)
endif()
