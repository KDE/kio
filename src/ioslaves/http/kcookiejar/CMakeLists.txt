####### kcookiejar: command line tool for talking to the kded kcookiejar module #######
qt5_generate_dbus_interface(${CMAKE_CURRENT_SOURCE_DIR}/kcookieserver.h org.kde.KCookieServer.xml)
set(kcookieserver_xml ${CMAKE_CURRENT_BINARY_DIR}/org.kde.KCookieServer.xml)
set_source_files_properties(${kcookieserver_xml} PROPERTIES INCLUDE "kcookiejar_include.h")
qt5_add_dbus_interfaces(kcookiejar_SRCS ${kcookieserver_xml})

set(kcookiejar_SRCS ${kcookiejar_SRCS} main.cpp)
add_executable(kcookiejar5 WIN32 ${kcookiejar_SRCS})

target_link_libraries( kcookiejar5
   Qt5::DBus
   KF5::I18n
   KF5::WidgetsAddons
)

install(TARGETS kcookiejar5 ${KF5_INSTALL_TARGETS_DEFAULT_ARGS} )

########### kded kcookiejar module ###############

set(kded_kcookiejar_SRCS
   kcookiejar.cpp
   kcookieserver.cpp
   kcookiewin.cpp
)

qt5_add_dbus_adaptor( kded_kcookiejar_SRCS ${CMAKE_CURRENT_BINARY_DIR}/org.kde.KCookieServer.xml kcookieserver.h KCookieServer )


add_library(kded_kcookiejar MODULE ${kded_kcookiejar_SRCS})
set_target_properties(kded_kcookiejar PROPERTIES
    OUTPUT_NAME kcookiejar
)
kcoreaddons_desktop_to_json(kded_kcookiejar kcookiejar.desktop)

target_link_libraries(kded_kcookiejar
   KF5::WindowSystem
   KF5::Service # kpluginfactory
   KF5::DBusAddons
   KF5::I18n
   KF5::WidgetsAddons
)

install(TARGETS kded_kcookiejar  DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf5/kded )


########### install files ###############

install( FILES domain_info  DESTINATION  ${KDE_INSTALL_DATADIR_KF5}/kcookiejar )
install( FILES ${CMAKE_CURRENT_BINARY_DIR}/org.kde.KCookieServer.xml DESTINATION ${KDE_INSTALL_DBUSINTERFACEDIR} RENAME kf5_org.kde.KCookieServer.xml)
