include(ConfigureChecks.cmake)

kcoreaddons_add_plugin(kio_file INSTALL_NAMESPACE "kf${QT_MAJOR_VERSION}/kio")

if (UNIX AND NOT APPLE AND NOT ANDROID)
    qt_add_dbus_interface(auth_daemon_interface_SRCS
        auth-daemon/org.kde.kio.filemanagement.xml authdaemoninterface
    )
endif()

if(WIN32)
    target_sources(kio_file PRIVATE
        file.cpp
        file_win.cpp
    )
else()
    target_sources(kio_file PRIVATE
        file.cpp
        file_unix.cpp
        legacycodec.cpp
        ${auth_daemon_interface_SRCS}
    )
endif()

check_include_files(sys/xattr.h HAVE_SYS_XATTR_H)

if (UNIX AND NOT APPLE AND NOT ANDROID)
    set(HAVE_AUTH_HELPER 1)
endif()
configure_file(config-kioslave-file.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kioslave-file.h )

ecm_qt_export_logging_category(
    IDENTIFIER KIO_FILE
    CATEGORY_NAME kf.kio.slaves.file
    OLD_CATEGORY_NAMES kf5.kio.kio_file
    DESCRIPTION "kiofile (KIO)"
    EXPORT KIO
)

target_link_libraries(kio_file KF5::KIOCore KF5::I18n KF5::ConfigCore Qt${QT_MAJOR_VERSION}::DBus Qt${QT_MAJOR_VERSION}::Network)

if(UNIX AND NOT APPLE AND NOT ANDROID)
  target_link_libraries(kio_file Qt${QT_MAJOR_VERSION}::Network)
endif()

if(ACL_FOUND)
   target_link_libraries(kio_file ${ACL_LIBS})
endif()

if (TARGET Qt6::Core5Compat)
    target_link_libraries(kio_file Qt6::Core5Compat)  # QTextCodec
endif()

if (UNIX AND NOT APPLE AND NOT ANDROID)
  add_subdirectory(auth-daemon)
endif()
