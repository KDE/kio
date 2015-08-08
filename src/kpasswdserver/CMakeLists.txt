find_package(KF5Wallet ${KF5_DEP_VERSION} REQUIRED)

add_subdirectory(autotests)

set(kded_kpasswdserver_SRCS kpasswdserver.cpp )

qt5_add_dbus_adaptor(kded_kpasswdserver_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../core/org.kde.KPasswdServer.xml kpasswdserver.h KPasswdServer)

add_library(kded_kpasswdserver MODULE ${kded_kpasswdserver_SRCS})
set_target_properties(kded_kpasswdserver PROPERTIES
    OUTPUT_NAME kpasswdserver
)

target_link_libraries(kded_kpasswdserver
   KF5::DBusAddons    # KDED Module
   Qt5::DBus
   KF5::KIOCore
   KF5::Wallet
   KF5::WidgetsAddons # KPasswordDialog
   KF5::I18n
   KF5::WindowSystem
)

kcoreaddons_desktop_to_json(kded_kpasswdserver kpasswdserver.desktop)

install( TARGETS kded_kpasswdserver  DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf5/kded )
