find_package(KF6Wallet ${KF_DEP_VERSION})

# tell what is missing without wallet
set_package_properties(KF6Wallet PROPERTIES DESCRIPTION "Safe desktop-wide storage for passwords"
                       TYPE OPTIONAL
                       PURPOSE "Required to have permanent storage of passwords for kpasswdserver"
                      )

if (KF6Wallet_FOUND)
    add_definitions(-DHAVE_KF6WALLET)
    set(WALLET_LIB KF6::Wallet)
endif()

if(BUILD_TESTING)
    add_subdirectory(autotests)
endif()

kcoreaddons_add_plugin(kiod_kpasswdserver
    INSTALL_NAMESPACE "kf6/kiod"
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
   KF6::DBusAddons    # KDED Module
   ${DBUS_LIB}
   KF6::KIOCore
   KF6::WidgetsAddons # KPasswordDialog
   KF6::I18n
   KF6::WindowSystem
   ${WALLET_LIB}
)

kdbusaddons_generate_dbus_service_file(kiod6 org.kde.kpasswdserver6 ${KDE_INSTALL_FULL_LIBEXECDIR_KF})
