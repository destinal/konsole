kcoreaddons_add_plugin(konsole_claudecodeplugin
SOURCES
    claudecodeplugin.cpp
    claudecodedbus.cpp
    claudesessionsmodel.cpp
    claudesessionswidget.cpp
    sessionnavigation.cpp
    statusnotifier.cpp
    programstatusrecords.cpp
    sessionstatus.cpp
    sessionstatusregistry.cpp
INSTALL_NAMESPACE
    "konsoleplugins"
)

configure_file(konsole_claudecode.in.json konsole_claudecode.json)
target_link_libraries(konsole_claudecodeplugin
    konsoleprivate
    konsoleapp
    Qt::DBus
    KF6::I18n
    KF6::Notifications
    KF6::WindowSystem
)
target_compile_definitions(konsole_claudecodeplugin PRIVATE -DRELEASE_SERVICE_VERSION="${RELEASE_SERVICE_VERSION}")

add_subdirectory(it2)

if(BUILD_TESTING)
    add_subdirectory(autotests)
endif()
