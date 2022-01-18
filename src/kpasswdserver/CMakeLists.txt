find_package(KF5Wallet ${KF_DEP_VERSION})

# tell what is missing without wallet
set_package_properties(KF5Wallet PROPERTIES DESCRIPTION "Safe desktop-wide storage for passwords"
                       TYPE OPTIONAL
                       PURPOSE "Required to have permanent storage of passwords for kpasswdserver"
                      )

if (KF5Wallet_FOUND)
    add_definitions(-DHAVE_KF5WALLET)
    set(WALLET_LIB KF5::Wallet)
endif()

if(BUILD_TESTING)
    add_subdirectory(autotests)
endif()

kcoreaddons_add_plugin(kiod_kpasswdserver
    INSTALL_NAMESPACE "kf${QT_MAJOR_VERSION}/kiod"
)
set_target_properties(kiod_kpasswdserver PROPERTIES
    OUTPUT_NAME kpasswdserver
)

set(kiod_kpasswdserver_dbus_SRCS)
qt_add_dbus_adaptor(kiod_kpasswdserver_dbus_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../core/org.kde.KPasswdServer.xml kpasswdserver.h KPasswdServer)

target_sources(kiod_kpasswdserver PRIVATE
    ${kiod_kpasswdserver_dbus_SRCS}
    kpasswdserver.cpp
    kiod_kpasswdserver.cpp
)

ecm_qt_export_logging_category(
    IDENTIFIER category
    CATEGORY_NAME kf.kio.kpasswdserver
    OLD_CATEGORY_NAMES org.kde.kio.kpasswdserver
    DESCRIPTION "KPasswdServer (KIO)"
    EXPORT KIO
)

target_link_libraries(kiod_kpasswdserver
   KF5::DBusAddons    # KDED Module
   Qt${QT_MAJOR_VERSION}::DBus
   KF5::KIOCore
   KF5::WidgetsAddons # KPasswordDialog
   KF5::I18n
   KF5::WindowSystem
   ${WALLET_LIB}
)

kdbusaddons_generate_dbus_service_file(kiod5 org.kde.kpasswdserver ${KDE_INSTALL_FULL_LIBEXECDIR_KF})
