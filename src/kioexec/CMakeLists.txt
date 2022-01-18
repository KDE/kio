kcoreaddons_add_plugin(kioexecd
    INSTALL_NAMESPACE "kf${QT_MAJOR_VERSION}/kiod")

set(kioexecd_dbus_SRCS)
qt_generate_dbus_interface(${CMAKE_CURRENT_SOURCE_DIR}/kioexecd.h org.kde.KIOExecd.xml)
qt_add_dbus_adaptor(kioexecd_dbus_SRCS ${CMAKE_CURRENT_BINARY_DIR}/org.kde.KIOExecd.xml kioexecd.h KIOExecd)

target_sources(kioexecd PRIVATE
    kioexecd.cpp
    ${kioexecd_dbus_SRCS}
)

ecm_qt_declare_logging_category(kioexecd
    HEADER kioexecdebug.h
    IDENTIFIER KIOEXEC
    CATEGORY_NAME kf.kio.execd
    OLD_CATEGORY_NAMES kf5.kio.execd
    DESCRIPTION "kioexecd (KIO)"
    EXPORT KIO
)

target_link_libraries(kioexecd KF5::I18n KF5::DBusAddons KF5::WidgetsAddons KF5::KIOCore)

kdbusaddons_generate_dbus_service_file(kiod5 org.kde.kioexecd ${KDE_INSTALL_FULL_LIBEXECDIR_KF})

# next target

add_executable(kioexec)
ecm_mark_nongui_executable(kioexec)

set(kioexec_dbus_SRCS)
qt_add_dbus_interface(kioexec_dbus_SRCS ${CMAKE_CURRENT_BINARY_DIR}/org.kde.KIOExecd.xml kioexecdinterface)

target_sources(kioexec PRIVATE
    ${kioexec_dbus_SRCS}
    main.cpp
)

configure_file(config-kioexec.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kioexec.h)

target_link_libraries(kioexec
    Qt${QT_MAJOR_VERSION}::Widgets
    KF5::I18n
    KF5::KIOWidgets    # KRun
    KF5::WidgetsAddons # KMessageBox
    KF5::WindowSystem  # KStartupInfo
    KF5::DBusAddons    # KDBusService
 )

if (HAVE_X11)
    if (TARGET Qt5::X11Extras)
        target_link_libraries(kioexec Qt5::X11Extras)
    elseif (TARGET Qt6::Gui)
        target_link_libraries(kioexec Qt${QT_MAJOR_VERSION}::GuiPrivate) # qtx11extras_p.h
    endif()
endif()

install(TARGETS kioexec DESTINATION ${KDE_INSTALL_LIBEXECDIR_KF} )

