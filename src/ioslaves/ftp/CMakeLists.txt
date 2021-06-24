include(ConfigureChecks.cmake)
configure_file(config-kioslave-ftp.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kioslave-ftp.h )

add_library(kio_ftp MODULE)

set_target_properties(kio_ftp PROPERTIES
    OUTPUT_NAME "ftp"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/kf5/kio"
)

target_sources(kio_ftp PRIVATE
    ftp.cpp
)

ecm_qt_export_logging_category(
    IDENTIFIER KIO_FTP
    CATEGORY_NAME kf.kio.slaves.ftp
    OLD_CATEGORY_NAMES kf5.kio.kio_ftp
    DEFAULT_SEVERITY Warning
    DESCRIPTION "kio ftp (KIO)"
    EXPORT KIO
)

target_link_libraries(kio_ftp Qt5::Network KF5::KIOCore KF5::I18n KF5::ConfigCore)

install(TARGETS kio_ftp  DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf5/kio)
