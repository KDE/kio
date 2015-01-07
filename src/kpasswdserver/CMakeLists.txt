find_package(KF5Wallet ${KF5_DEP_VERSION} REQUIRED)

add_subdirectory(tests)

set(kded_kpasswdserver_SRCS kpasswdserver.cpp )

qt5_add_dbus_adaptor(kded_kpasswdserver_SRCS org.kde.KPasswdServer.xml kpasswdserver.h KPasswdServer)

add_library(kded_kpasswdserver MODULE ${kded_kpasswdserver_SRCS})
set_target_properties(kded_kpasswdserver PROPERTIES
    OUTPUT_NAME kpasswdserver
)

target_link_libraries(kded_kpasswdserver KF5::DBusAddons KF5::KIOCore KF5::Wallet KF5::WidgetsAddons KF5::I18n KF5::WindowSystem)

install(TARGETS kded_kpasswdserver  DESTINATION ${PLUGIN_INSTALL_DIR}/kf5/kded )

install( FILES kpasswdserver.desktop  DESTINATION  ${SERVICES_INSTALL_DIR}/kded )
