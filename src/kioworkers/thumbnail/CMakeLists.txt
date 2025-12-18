add_library(kio_thumbnail MODULE)
set_target_properties(kio_thumbnail PROPERTIES
    OUTPUT_NAME "thumbnail"
)

target_sources(kio_thumbnail PRIVATE
    thumbnail.cpp
    imagefilter.cpp
)

ecm_qt_declare_logging_category(kio_thumbnail
    HEADER thumbnail-logsettings.h
    IDENTIFIER KIO_THUMBNAIL_LOG
    CATEGORY_NAME kf.kio.workers.thumbnail
    OLD_CATEGORY_NAMES log_kio_thumbnail kf.kio.slaves.thumbnail
    DESCRIPTION "KIO thumbnail"
    EXPORT KIO_EXTRAS
)

target_link_libraries(kio_thumbnail
    PUBLIC
        Qt6::Widgets
        KF6::CoreAddons
        KF6::KIOCore
        KF6::KIOGui
        KF6::I18n
)

install(TARGETS kio_thumbnail DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf6/kio)