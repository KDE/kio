include(ConfigureChecks.cmake)

kcoreaddons_add_plugin(kio_file INSTALL_NAMESPACE "kf6/kio")

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
    CATEGORY_NAME kf.kio.workers.file
    OLD_CATEGORY_NAMES kf5.kio.kio_file kf.kio.slaves.file
    DESCRIPTION "kiofile (KIO)"
    EXPORT KIO
)

target_link_libraries(kio_file KF6::KIOCore KF6::I18n KF6::ConfigCore Qt6::DBus Qt6::Network)

if(UNIX)
  target_link_libraries(kio_file Qt6::Network KF6::AuthCore)
endif()

if(ACL_FOUND)
   target_link_libraries(kio_file ${ACL_LIBS})
endif()

if (TARGET Qt6::Core5Compat)
    target_link_libraries(kio_file Qt6::Core5Compat)  # QTextCodec
endif()

if (UNIX)
  add_subdirectory(kauth)
endif()
