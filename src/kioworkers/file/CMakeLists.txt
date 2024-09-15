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
    )
endif()

check_include_files(sys/xattr.h HAVE_SYS_XATTR_H)

configure_file(config-kioworker-file.h.in ${CMAKE_CURRENT_BINARY_DIR}/config-kioworker-file.h)

configure_file(config-stat-unix.h.in ${CMAKE_CURRENT_BINARY_DIR}/config-stat-unix.file.h)

ecm_qt_export_logging_category(
    IDENTIFIER KIO_FILE
    CATEGORY_NAME kf.kio.workers.file
    OLD_CATEGORY_NAMES kf5.kio.kio_file kf.kio.slaves.file
    DESCRIPTION "kiofile (KIO)"
    EXPORT KIO
)

target_link_libraries(kio_file KF6::KIOCore KF6::I18n KF6::ConfigCore ${DBUS_LIB} Qt6::Network ${LIBURING_LIBRARIES})
target_include_directories(kio_file INTERFACE ${LIBURING_INCLUDE_DIRS})

if(UNIX)
  target_link_libraries(kio_file Qt6::Network KF6::AuthCore)
endif()

if (HAIKU)
  # for accept, recvmsg, socket, bind and listen (network)
  # for fsetxattr (gnu)
  target_link_libraries(kio_file gnu network)
endif ()


if(ACL_FOUND)
   target_link_libraries(kio_file ${ACL_LIBS})
endif()
