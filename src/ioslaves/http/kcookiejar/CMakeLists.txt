####### kcookiejar: command line tool for talking to the kded kcookiejar module #######
add_executable( kcookiejar5)
ecm_mark_nongui_executable(kcookiejar5)

qt_generate_dbus_interface(${CMAKE_CURRENT_SOURCE_DIR}/kcookieserver.h org.kde.KCookieServer.xml)
set(kcookieserver_xml ${CMAKE_CURRENT_BINARY_DIR}/org.kde.KCookieServer.xml)
set_source_files_properties(${kcookieserver_xml} PROPERTIES INCLUDE "kcookiejar_include.h")
set(kcookiejar_dbus_SRCS)
qt_add_dbus_interfaces(kcookiejar_dbus_SRCS ${kcookieserver_xml})

target_sources(kcookiejar5 PRIVATE
    ${kcookiejar_dbus_SRCS}
    main.cpp
)

target_link_libraries( kcookiejar5
   Qt${QT_MAJOR_VERSION}::DBus
   KF5::I18n
   KF5::WidgetsAddons
)

install(TARGETS kcookiejar5 ${KF5_INSTALL_TARGETS_DEFAULT_ARGS} )

########### kded kcookiejar module ###############

kcoreaddons_add_plugin(kded_kcookiejar
    INSTALL_NAMESPACE "kf${QT_MAJOR_VERSION}/kded"
)
set_target_properties(kded_kcookiejar PROPERTIES
    OUTPUT_NAME kcookiejar
)

set(kded_kcookiejar_dbus_SRCS)
qt_add_dbus_adaptor(kded_kcookiejar_dbus_SRCS ${CMAKE_CURRENT_BINARY_DIR}/org.kde.KCookieServer.xml kcookieserver.h KCookieServer)

target_sources(kded_kcookiejar PRIVATE
   ${kded_kcookiejar_dbus_SRCS}
   kcookiejar.cpp
   kcookieserver.cpp
   kcookiewin.cpp
)

ecm_qt_export_logging_category(
    IDENTIFIER KIO_COOKIEJAR
    CATEGORY_NAME kf.kio.slaves.http.cookiejar
    OLD_CATEGORY_NAMES kf5.kio.cookiejar
    DESCRIPTION "kcookiejar (KIO)"
    EXPORT KIO
)

target_link_libraries(kded_kcookiejar
   KF5::WindowSystem
   KF5::ConfigCore
   KF5::CoreAddons
   KF5::DBusAddons
   KF5::I18n
   KF5::WidgetsAddons
)

########### install files ###############

install( FILES domain_info  DESTINATION  ${KDE_INSTALL_DATADIR_KF}/kcookiejar )
install( FILES ${CMAKE_CURRENT_BINARY_DIR}/org.kde.KCookieServer.xml DESTINATION ${KDE_INSTALL_DBUSINTERFACEDIR} RENAME kf5_org.kde.KCookieServer.xml)

kdbusaddons_generate_dbus_service_file(kded5 org.kde.kcookiejar5 ${KDE_INSTALL_FULL_BINDIR})
