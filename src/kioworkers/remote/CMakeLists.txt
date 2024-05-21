if (HAVE_QTDBUS)
    add_subdirectory( kdedmodule )
endif()

kcoreaddons_add_plugin(kio_remote
    INSTALL_NAMESPACE kf6/kio
)

target_sources(kio_remote PRIVATE
    kio_remote.cpp
    remoteimpl.cpp
)

ecm_qt_declare_logging_category(kio_remote
    HEADER debug.h
    IDENTIFIER KIOREMOTE_LOG
    CATEGORY_NAME kf.kio.workers.remote
    OLD_CATEGORY_NAMES kf5.kio.kio_remote kf.kio.slaves.remote
    DEFAULT_SEVERITY Info
    DESCRIPTION "kio_remote (KIO)"
    EXPORT KIO
)

target_link_libraries(kio_remote KF6::KIOCore KF6::I18n KF6::ConfigCore Qt6::Network)
