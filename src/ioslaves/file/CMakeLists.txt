include(ConfigureChecks.cmake)

kcoreaddons_add_plugin(kio_file INSTALL_NAMESPACE "kf5/kio")

if(WIN32)
    target_sources(kio_file PRIVATE
        file.cpp
        file_win.cpp
    )
else()
    target_sources(kio_file PRIVATE
        file.cpp
        file_unix.cpp
        fdreceiver.cpp
        legacycodec.cpp
    )
endif()

check_include_files(sys/xattr.h HAVE_SYS_XATTR_H)

configure_file(config-kioslave-file.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kioslave-file.h )

ecm_qt_export_logging_category(
    IDENTIFIER KIO_FILE
    CATEGORY_NAME kf.kio.slaves.file
    OLD_CATEGORY_NAMES kf5.kio.kio_file
    DESCRIPTION "kiofile (KIO)"
    EXPORT KIO
)

target_link_libraries(kio_file KF5::KIOCore KF5::I18n KF5::ConfigCore Qt5::DBus Qt5::Network)

if(UNIX)
  target_link_libraries(kio_file Qt5::Network KF5::AuthCore)
endif()

if(ACL_FOUND)
   target_link_libraries(kio_file ${ACL_LIBS})
endif()

if (UNIX)
  add_subdirectory(kauth)
endif()
