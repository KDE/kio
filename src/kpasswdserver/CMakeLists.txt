add_subdirectory(tests)

set(kded_kpasswdserver_SRCS kpasswdserver.cpp )

qt5_add_dbus_adaptor(kded_kpasswdserver_SRCS org.kde.KPasswdServer.xml kpasswdserver.h KPasswdServer)

add_library(kded_kpasswdserver MODULE ${kded_kpasswdserver_SRCS})

target_link_libraries(kded_kpasswdserver KF5::DBusAddons KF5::KIOCore KF5::Wallet KF5::KDE4Support)

install(TARGETS kded_kpasswdserver  DESTINATION ${PLUGIN_INSTALL_DIR} )

install( FILES kpasswdserver.desktop  DESTINATION  ${SERVICES_INSTALL_DIR}/kded )
