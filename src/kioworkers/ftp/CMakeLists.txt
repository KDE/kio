include(ConfigureChecks.cmake)

configure_file(config-kioworker-ftp.h.in ${CMAKE_CURRENT_BINARY_DIR}/config-kioworker-ftp.h )

kcoreaddons_add_plugin(kio_ftp INSTALL_NAMESPACE "kf6/kio")

target_sources(kio_ftp PRIVATE
    ftp.cpp
)

ecm_qt_export_logging_category(
    IDENTIFIER KIO_FTP
    CATEGORY_NAME kf.kio.workers.ftp
    OLD_CATEGORY_NAMES kf5.kio.kio_ftp kf.kio.slaves.ftp
    DEFAULT_SEVERITY Warning
    DESCRIPTION "kio ftp (KIO)"
    EXPORT KIO
)

target_link_libraries(kio_ftp Qt6::Network KF6::KIOCore KF6::I18n KF6::ConfigCore)
