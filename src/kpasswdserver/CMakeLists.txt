find_package(KF5Wallet ${KF5_DEP_VERSION})

# tell what is missing without wallet
set_package_properties(KF5Wallet PROPERTIES DESCRIPTION "Safe desktop-wide storage for passwords"
                       TYPE OPTIONAL
                       PURPOSE "Required to have permanent storage of passwords for kpasswdserver"
                      )

if (KF5Wallet_FOUND)
    add_definitions (-DHAVE_KF5WALLET)
endif()

add_subdirectory(autotests)

set(kiod_kpasswdserver_SRCS kpasswdserver.cpp kiod_kpasswdserver.cpp)

qt5_add_dbus_adaptor(kiod_kpasswdserver_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../core/org.kde.KPasswdServer.xml kpasswdserver.h KPasswdServer)

kcoreaddons_add_plugin(kiod_kpasswdserver INSTALL_NAMESPACE "kf5/kiod" JSON kpasswdserver.json SOURCES ${kiod_kpasswdserver_SRCS})
set_target_properties(kiod_kpasswdserver PROPERTIES
    OUTPUT_NAME kpasswdserver
)

target_link_libraries(kiod_kpasswdserver
   KF5::DBusAddons    # KDED Module
   Qt5::DBus
   KF5::KIOCore
   KF5::WidgetsAddons # KPasswordDialog
   KF5::I18n
   KF5::WindowSystem
)

if (KF5Wallet_FOUND)
    target_link_libraries(kiod_kpasswdserver KF5::Wallet)
endif()
