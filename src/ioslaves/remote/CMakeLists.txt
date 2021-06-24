add_subdirectory( kdedmodule ) 

kcoreaddons_add_plugin(kio_remote
    INSTALL_NAMESPACE kf5/kio
)

set_target_properties(kio_remote PROPERTIES OUTPUT_NAME "remote")

target_sources(kio_remote PRIVATE
    kio_remote.cpp
    remoteimpl.cpp
)

ecm_qt_declare_logging_category(kio_remote
    HEADER debug.h
    IDENTIFIER KIOREMOTE_LOG
    CATEGORY_NAME kf.kio.slaves.remote
    OLD_CATEGORY_NAMES kf5.kio.kio_remote
    DEFAULT_SEVERITY Info
    DESCRIPTION "kio_remote (KIO)"
    EXPORT KIO
)

target_link_libraries(kio_remote KF5::KIOCore KF5::I18n KF5::ConfigCore Qt5::Network)
