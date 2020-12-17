
project(kioslave-file)

include(ConfigureChecks.cmake)

########### next target ###############

if(WIN32)
  set(kio_file_PART_SRCS file.cpp file_win.cpp )
else()
  set(kio_file_PART_SRCS file.cpp file_unix.cpp fdreceiver.cpp legacycodec.cpp )
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

add_library(kio_file MODULE ${kio_file_PART_SRCS})
target_link_libraries(kio_file KF5::KIOCore KF5::I18n Qt5::DBus Qt5::Network)

if(UNIX)
  target_link_libraries(kio_file Qt5::Network KF5::AuthCore)
endif()

if (HAVE_VOLMGT AND CMAKE_SYSTEM_NAME MATCHES SunOS)
    target_link_libraries(kio_file -lvolmgt)
endif ()

if(ACL_FOUND)
   target_link_libraries(kio_file ${ACL_LIBS})
endif()

set_target_properties(kio_file PROPERTIES OUTPUT_NAME "file")
set_target_properties(kio_file PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/kf5/kio")
install(TARGETS kio_file  DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf5/kio)

if (UNIX)
  add_subdirectory(kauth)
endif()
