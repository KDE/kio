set(kioexecd_SRCS kioexecd.cpp)

qt5_generate_dbus_interface(${CMAKE_CURRENT_SOURCE_DIR}/kioexecd.h org.kde.KIOExecd.xml)
qt5_add_dbus_adaptor(kioexecd_SRCS ${CMAKE_CURRENT_BINARY_DIR}/org.kde.KIOExecd.xml kioexecd.h KIOExecd)


ecm_qt_declare_logging_category(kioexecd_SRCS
    HEADER kioexecdebug.h
    IDENTIFIER KIOEXEC
    CATEGORY_NAME kf.kio.execd
    OLD_CATEGORY_NAMES kf5.kio.execd
    DESCRIPTION "kioexecd (KIO)"
    EXPORT KIO
)

kcoreaddons_add_plugin(kioexecd
    SOURCES ${kioexecd_SRCS}
    JSON kioexecd.json
    INSTALL_NAMESPACE "kf5/kiod")

target_link_libraries(kioexecd KF5::I18n KF5::DBusAddons KF5::WidgetsAddons KF5::KIOCore)

kdbusaddons_generate_dbus_service_file(kiod5 org.kde.kioexecd ${KDE_INSTALL_FULL_LIBEXECDIR_KF5})

# next target

set(kioexec_SRCS main.cpp)
qt5_add_dbus_interface(kioexec_SRCS ${CMAKE_CURRENT_BINARY_DIR}/org.kde.KIOExecd.xml kioexecdinterface)

add_executable(kioexec ${kioexec_SRCS})

configure_file(config-kioexec.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kioexec.h)

ecm_mark_nongui_executable(kioexec)

target_link_libraries(kioexec
    Qt5::Widgets
    KF5::I18n
    KF5::KIOWidgets    # KRun
    KF5::WidgetsAddons # KMessageBox
    KF5::WindowSystem  # KStartupInfo
    KF5::DBusAddons    # KDBusService
 )

if (HAVE_X11)
    target_link_libraries(kioexec Qt5::X11Extras)
endif()

install(TARGETS kioexec DESTINATION ${KDE_INSTALL_LIBEXECDIR_KF5} )

