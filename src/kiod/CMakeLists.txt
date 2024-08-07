include(ECMMarkNonGuiExecutable)

add_executable(kiod6)

target_sources(kiod6 PRIVATE
    kiod_main.cpp
)

if (APPLE)
    target_sources(kiod6 PRIVATE
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

target_link_libraries(kiod6
   KF6::KIOCore     # ksslcertificatemanager
   KF6::DBusAddons  # kdedmodule
   KF6::CoreAddons  # kpluginfactory
   KF6::Crash
   Qt6::Network
   ${DBUS_LIB}
   Qt6::Widgets     # QApplication
)

if (APPLE)
    target_link_libraries(kiod6 "-framework AppKit -framework CoreFoundation")

    # Mark it as non-gui so we won't create an app bundle on Mac OS X
    ecm_mark_nongui_executable(kiod6)
endif ()

install(TARGETS kiod6 DESTINATION ${KDE_INSTALL_LIBEXECDIR_KF})
install(FILES org.kde.kiod6.desktop DESTINATION ${KDE_INSTALL_APPDIR})

kdbusaddons_generate_dbus_service_file(kiod6 org.kde.kiod6 ${KDE_INSTALL_FULL_LIBEXECDIR_KF})

