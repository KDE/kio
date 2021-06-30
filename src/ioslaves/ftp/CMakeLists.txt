include(ConfigureChecks.cmake)
configure_file(config-kioslave-ftp.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kioslave-ftp.h )

kcoreaddons_add_plugin(kio_ftp INSTALL_NAMESPACE "kf5/kio")

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
