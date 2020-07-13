add_subdirectory( kdedmodule ) 

set(kio_remote_SRCS
    kio_remote.cpp
    remoteimpl.cpp
    )

ecm_qt_declare_logging_category(kio_remote_SRCS
    HEADER debug.h
    IDENTIFIER KIOREMOTE_LOG
    CATEGORY_NAME kf.kio.slaves.remote
    OLD_CATEGORY_NAMES kf5.kio.kio_remote
    DEFAULT_SEVERITY Info
    DESCRIPTION "kio_remote (KIO)"
    EXPORT KIO
)

kcoreaddons_add_plugin(kio_remote
    SOURCES ${kio_remote_SRCS}
    INSTALL_NAMESPACE kf5/kio)

target_link_libraries(kio_remote KF5::KIOCore KF5::I18n Qt5::Network)

set_target_properties(kio_remote PROPERTIES OUTPUT_NAME "remote")
