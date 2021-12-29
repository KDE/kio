include(ECMMarkNonGuiExecutable)

add_executable(kiod5)

target_sources(kiod5 PRIVATE
    kiod_main.cpp
)

if (APPLE)
    target_sources(kiod5 PRIVATE
        kiod_agent.mm
    )
endif()

ecm_qt_export_logging_category(
    IDENTIFIER KIOD_CATEGORY
    CATEGORY_NAME kf.kio.kiod
    OLD_CATEGORY_NAMES kf5.kiod
    DESCRIPTION "KIO Daemon (KIO)"
    EXPORT KIO
)

target_link_libraries(kiod5
   KF5::KIOCore     # ksslcertificatemanager
   KF5::DBusAddons  # kdedmodule
   KF5::CoreAddons  # kpluginfactory
   Qt${QT_MAJOR_VERSION}::Network
   Qt${QT_MAJOR_VERSION}::DBus
   Qt${QT_MAJOR_VERSION}::Widgets     # QApplication
)

if (APPLE)
    target_link_libraries(kiod5 "-framework AppKit -framework CoreFoundation")

    # Mark it as non-gui so we won't create an app bundle on Mac OS X
    ecm_mark_nongui_executable(kiod5)
endif ()

install(TARGETS kiod5 DESTINATION ${KDE_INSTALL_LIBEXECDIR_KF})

kdbusaddons_generate_dbus_service_file(kiod5 org.kde.kiod5 ${KDE_INSTALL_FULL_LIBEXECDIR_KF})

