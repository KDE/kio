kcoreaddons_add_plugin(kio_http
    INSTALL_NAMESPACE kf6/kio
)

target_sources(kio_http PRIVATE
    http.cpp
)

ecm_qt_declare_logging_category(kio_http
    HEADER debug.h
    IDENTIFIER KIOHTTP_LOG
    CATEGORY_NAME kf.kio.workers.http
    DEFAULT_SEVERITY Info
    DESCRIPTION "kio_http (KIO)"
    EXPORT KIO
)

target_link_libraries(kio_http KF6::KIOCore KF6::I18n KF6::ConfigCore Qt6::Network Qt6::Xml)
